#include "IDisk.h"
#include <IO/ReadBufferFromFileBase.h>
#include <IO/WriteBufferFromFileBase.h>
#include <IO/copyData.h>
#include <DBPoco/Logger.h>
#include <Interpreters/Context.h>
#include <Common/threadPoolCallbackRunner.h>
#include <Common/logger_useful.h>
#include <Common/setThreadName.h>
#include <Core/ServerUUID.h>
#include <Disks/FakeDiskTransaction.h>

namespace DB
{

namespace ErrorCodes
{
    extern const int NOT_IMPLEMENTED;
    extern const int CANNOT_READ_ALL_DATA;
    extern const int LOGICAL_ERROR;
}

bool IDisk::isDirectoryEmpty(const String & path) const
{
    return !iterateDirectory(path)->isValid();
}

void IDisk::copyFile( /// NOLINT
    const String & from_file_path,
    IDisk & to_disk,
    const String & to_file_path,
    const ReadSettings & read_settings,
    const WriteSettings & write_settings,
    const std::function<void()> & cancellation_hook
    )
{
    LOG_DEBUG(getLogger("IDisk"), "Copying from {} (path: {}) {} to {} (path: {}) {}.",
              getName(), getPath(), from_file_path, to_disk.getName(), to_disk.getPath(), to_file_path);

    auto in = readFile(from_file_path, read_settings);
    auto out = to_disk.writeFile(to_file_path, DBMS_DEFAULT_BUFFER_SIZE, WriteMode::Rewrite, write_settings);
    copyData(*in, *out, cancellation_hook);
    out->finalize();
}

DiskTransactionPtr IDisk::createTransaction()
{
    return std::make_shared<FakeDiskTransaction>(*this);
}

void IDisk::removeSharedFiles(const RemoveBatchRequest & files, bool keep_all_batch_data, const NameSet & file_names_remove_metadata_only)
{
    for (const auto & file : files)
    {
        bool keep_file = keep_all_batch_data || file_names_remove_metadata_only.contains(fs::path(file.path).filename());
        if (file.if_exists)
            removeSharedFileIfExists(file.path, keep_file);
        else
            removeSharedFile(file.path, keep_file);
    }
}

std::unique_ptr<ReadBufferFromFileBase> IDisk::readEncryptedFile(const String &, const ReadSettings &) const
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "File encryption is not implemented for disk of type {}", getDataSourceDescription().type);
}

std::unique_ptr<WriteBufferFromFileBase> IDisk::writeEncryptedFile(const String &, size_t, WriteMode, const WriteSettings &) const
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "File encryption is not implemented for disk of type {}", getDataSourceDescription().type);
}

size_t IDisk::getEncryptedFileSize(const String &) const
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "File encryption is not implemented for disk of type {}", getDataSourceDescription().type);
}

size_t IDisk::getEncryptedFileSize(size_t) const
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "File encryption is not implemented for disk of type {}", getDataSourceDescription().type);
}

UInt128 IDisk::getEncryptedFileIV(const String &) const
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "File encryption is not implemented for disk of type {}", getDataSourceDescription().type);
}

void asyncCopy(
    IDisk & from_disk,
    String from_path,
    IDisk & to_disk,
    String to_path,
    ThreadPoolCallbackRunnerLocal<void> & runner,
    bool copy_root_dir,
    const ReadSettings & read_settings,
    const WriteSettings & write_settings,
    const std::function<void()> & cancellation_hook)
{
    if (from_disk.isFile(from_path))
    {
        runner(
            [&from_disk, from_path, &to_disk, to_path, &read_settings, &write_settings, &cancellation_hook] {
                from_disk.copyFile(
                    from_path, to_disk, fs::path(to_path) / fileName(from_path), read_settings, write_settings, cancellation_hook);
            });
    }
    else
    {
        fs::path dest(to_path);
        if (copy_root_dir)
        {
            fs::path dir_name = fs::path(from_path).parent_path().filename();
            dest /= dir_name;
            to_disk.createDirectories(dest);
        }

        for (auto it = from_disk.iterateDirectory(from_path); it->isValid(); it->next())
            asyncCopy(from_disk, it->path(), to_disk, dest, runner, true, read_settings, write_settings, cancellation_hook);
    }
}

void IDisk::copyThroughBuffers(
    const String & from_path,
    const std::shared_ptr<IDisk> & to_disk,
    const String & to_path,
    bool copy_root_dir,
    const ReadSettings & read_settings,
    WriteSettings write_settings,
    const std::function<void()> & cancellation_hook)
{
    ThreadPoolCallbackRunnerLocal<void> runner(copying_thread_pool, "AsyncCopy");

    /// Disable parallel write. We already copy in parallel.
    /// Avoid high memory usage. See test_s3_zero_copy_ttl/test.py::test_move_and_s3_memory_usage
    write_settings.s3_allow_parallel_part_upload = false;
    write_settings.azure_allow_parallel_part_upload = false;

    asyncCopy(*this, from_path, *to_disk, to_path, runner, copy_root_dir, read_settings, write_settings, cancellation_hook);

    runner.waitForAllToFinishAndRethrowFirstError();
}


void IDisk::copyDirectoryContent(
    const String & from_dir,
    const std::shared_ptr<IDisk> & to_disk,
    const String & to_dir,
    const ReadSettings & read_settings,
    const WriteSettings & write_settings,
    const std::function<void()> & cancellation_hook)
{
    if (!to_disk->exists(to_dir))
        to_disk->createDirectories(to_dir);

    copyThroughBuffers(from_dir, to_disk, to_dir, /* copy_root_dir= */ false, read_settings, write_settings, cancellation_hook);
}

void IDisk::truncateFile(const String &, size_t)
{
    throw Exception(ErrorCodes::NOT_IMPLEMENTED, "Truncate operation is not implemented for disk of type {}", getDataSourceDescription().type);
}

SyncGuardPtr IDisk::getDirectorySyncGuard(const String & /* path */) const
{
    return nullptr;
}

void IDisk::startup(ContextPtr context, bool skip_access_check)
{
    if (!skip_access_check)
    {
        if (isReadOnly())
        {
            LOG_DEBUG(getLogger("IDisk"),
                "Skip access check for disk {} (read-only disk).",
                getName());
        }
        else
            checkAccess();
    }
    startupImpl(context);
}

void IDisk::checkAccess()
{
    DB::UUID server_uuid = DB::ServerUUID::get();
    if (server_uuid == DB::UUIDHelpers::Nil)
        throw Exception(ErrorCodes::LOGICAL_ERROR, "Server UUID is not initialized");
    const String path = fmt::format("clickhouse_access_check_{}", toString(server_uuid));

    checkAccessImpl(path);
}

/// NOTE: should we mark the disk readonly if the write/unlink fails instead of throws?
void IDisk::checkAccessImpl(const String & path)
try
{
    const std::string_view payload("test", 4);

    /// write
    {
        auto file = writeFile(path, DBMS_DEFAULT_BUFFER_SIZE, WriteMode::Rewrite);
        try
        {
            file->write(payload.data(), payload.size());
            file->finalize();
        }
        catch (...)
        {
            /// Log current exception, because finalize() can throw a different exception.
            tryLogCurrentException(__PRETTY_FUNCTION__);
            throw;
        }
    }

    /// read
    {
        auto file = readFile(path);
        String buf(payload.size(), '0');
        file->readStrict(buf.data(), buf.size());
        if (buf != payload)
        {
            throw Exception(ErrorCodes::CANNOT_READ_ALL_DATA,
                "Content of {}::{} does not matches after read ({} vs {})", name, path, buf, payload);
        }
    }

    /// read with offset
    {
        auto file = readFile(path);
        auto offset = 2;
        String buf(payload.size() - offset, '0');
        file->seek(offset, 0);
        file->readStrict(buf.data(), buf.size());
        if (buf != payload.substr(offset))
        {
            throw Exception(ErrorCodes::CANNOT_READ_ALL_DATA,
                "Content of {}::{} does not matches after read with offset ({} vs {})", name, path, buf, payload.substr(offset));
        }
    }

    /// remove
    removeFile(path);
}
catch (Exception & e)
{
    e.addMessage(fmt::format("While checking access for disk {}", name));
    throw;
}

void IDisk::applyNewSettings(const DBPoco::Util::AbstractConfiguration & config, ContextPtr /*context*/, const String & config_prefix, const DisksMap & /*map*/)
{
    copying_thread_pool.setMaxThreads(config.getInt(config_prefix + ".thread_pool_size", 16));
}

}
