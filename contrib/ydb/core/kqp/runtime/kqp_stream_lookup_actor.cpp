#include "kqp_stream_lookup_actor.h"

#include <contrib/ydb/core/actorlib_impl/long_timer.h>
#include <contrib/ydb/core/base/tablet_pipecache.h>
#include <contrib/ydb/core/engine/minikql/minikql_engine_host.h>
#include <contrib/ydb/core/kqp/common/kqp_resolve.h>
#include <contrib/ydb/core/kqp/common/kqp_event_ids.h>
#include <contrib/ydb/core/kqp/gateway/kqp_gateway.h>
#include <contrib/ydb/core/kqp/runtime/kqp_scan_data.h>
#include <contrib/ydb/core/kqp/runtime/kqp_read_iterator_common.h>
#include <contrib/ydb/core/kqp/runtime/kqp_stream_lookup_worker.h>
#include <contrib/ydb/core/protos/kqp_stats.pb.h>
#include <contrib/ydb/core/tx/scheme_cache/scheme_cache.h>

#include <contrib/ydb/library/actors/core/actor_bootstrapped.h>
#include <yql/essentials/public/issue/yql_issue_message.h>
#include <contrib/ydb/library/yql/dq/actors/compute/dq_compute_actor_impl.h>
#include <contrib/ydb/library/wilson_ids/wilson.h>

namespace NKikimr {
namespace NKqp {

namespace {

static constexpr TDuration SCHEME_CACHE_REQUEST_TIMEOUT = TDuration::Seconds(10);
NActors::TActorId MainPipeCacheId = NKikimr::MakePipePerNodeCacheID(false);

class TKqpStreamLookupActor : public NActors::TActorBootstrapped<TKqpStreamLookupActor>, public NYql::NDq::IDqComputeActorAsyncInput {
public:
    TKqpStreamLookupActor(NYql::NDq::IDqAsyncIoFactory::TInputTransformArguments&& args, NKikimrKqp::TKqpStreamLookupSettings&& settings,
        TIntrusivePtr<TKqpCounters> counters)
        : LogPrefix(TStringBuilder() << "StreamLookupActor, inputIndex: " << args.InputIndex << ", CA Id " << args.ComputeActorId)
        , InputIndex(args.InputIndex)
        , Input(args.TransformInput)
        , ComputeActorId(args.ComputeActorId)
        , TypeEnv(args.TypeEnv)
        , Alloc(args.Alloc)
        , Snapshot(settings.GetSnapshot().GetStep(), settings.GetSnapshot().GetTxId())
        , AllowInconsistentReads(settings.GetAllowInconsistentReads())
        , LockTxId(settings.HasLockTxId() ? settings.GetLockTxId() : TMaybe<ui64>())
        , NodeLockId(settings.HasLockNodeId() ? settings.GetLockNodeId() : TMaybe<ui32>())
        , LockMode(settings.HasLockMode() ? settings.GetLockMode() : TMaybe<NKikimrDataEvents::ELockMode>())
        , SchemeCacheRequestTimeout(SCHEME_CACHE_REQUEST_TIMEOUT)
        , LookupStrategy(settings.GetLookupStrategy())
        , StreamLookupWorker(CreateStreamLookupWorker(std::move(settings), args.TypeEnv, args.HolderFactory, args.InputDesc))
        , Counters(counters)
        , LookupActorSpan(TWilsonKqp::LookupActor, std::move(args.TraceId), "LookupActor")
    {
        IngressStats.Level = args.StatsLevel;
    }

    virtual ~TKqpStreamLookupActor() {
        if (Alloc) {
            TGuard<NMiniKQL::TScopedAlloc> allocGuard(*Alloc);
            Input.Clear();
            StreamLookupWorker.reset();
        }
    }

    void Bootstrap() {
        CA_LOG_D("Start stream lookup actor");

        Counters->StreamLookupActorsCount->Inc();
        ResolveTableShards();
        Become(&TKqpStreamLookupActor::StateFunc);
    }

    static constexpr NKikimrServices::TActivity::EType ActorActivityType() {
        return NKikimrServices::TActivity::KQP_STREAM_LOOKUP_ACTOR;
    }

    void FillExtraStats(NYql::NDqProto::TDqTaskStats* stats , bool last, const NYql::NDq::TDqMeteringStats* mstats) override {
        if (last) {
            NYql::NDqProto::TDqTableStats* tableStats = nullptr;
            for (auto& table : *stats->MutableTables()) {
                if (table.GetTablePath() == StreamLookupWorker->GetTablePath()) {
                    tableStats = &table;
                }
            }

            if (!tableStats) {
                tableStats = stats->AddTables();
                tableStats->SetTablePath(StreamLookupWorker->GetTablePath());
            }

            ui64 rowsReadEstimate = ReadRowsCount;
            ui64 bytesReadEstimate = ReadBytesCount;

            if (mstats) {
                switch(LookupStrategy) {
                    case NKqpProto::EStreamLookupStrategy::LOOKUP: {
                        // in lookup case we return as result actual data, that we read from the datashard.
                        rowsReadEstimate = mstats->Inputs[InputIndex]->RowsConsumed;
                        bytesReadEstimate = mstats->Inputs[InputIndex]->BytesConsumed;
                        break;
                    }
                    default:
                        ;
                }
            }

            // TODO: use evread statistics after KIKIMR-16924
            tableStats->SetReadRows(tableStats->GetReadRows() + rowsReadEstimate);
            tableStats->SetReadBytes(tableStats->GetReadBytes() + bytesReadEstimate);
            tableStats->SetAffectedPartitions(tableStats->GetAffectedPartitions() + ReadsPerShard.size());

            NKqpProto::TKqpTableExtraStats tableExtraStats;
            auto readActorTableAggrExtraStats = tableExtraStats.MutableReadActorTableAggrExtraStats();
            for (const auto& [shardId, _] : ReadsPerShard) {
                readActorTableAggrExtraStats->AddAffectedShards(shardId);
            }

            tableStats->MutableExtra()->PackFrom(tableExtraStats);
        }
    }

private:
    enum class EReadState {
        Initial,
        Running,
        Blocked, // Read can't accept new data, but not finished yet
        Finished,
    };

    std::string_view ReadStateToString(EReadState state) {
        switch (state) {
            case EReadState::Initial: return "Initial"sv;
            case EReadState::Running: return "Running"sv;
            case EReadState::Blocked: return "Blocked"sv;
            case EReadState::Finished: return "Finished"sv;
        }
    }

    struct TReadState {
        TReadState(ui64 id, ui64 shardId)
            : Id(id)
            , ShardId(shardId)
            , State(EReadState::Initial) {}

        void SetFinished() {
            State = EReadState::Finished;
        }

        bool Finished() const {
            return (State == EReadState::Finished);
        }

        void SetBlocked() {
            State = EReadState::Blocked;
        }

        const ui64 Id;
        const ui64 ShardId;
        EReadState State;
        TMaybe<TOwnedCellVec> LastProcessedKey;
        ui32 FirstUnprocessedQuery = 0;
        ui64 LastSeqNo = 0;
    };

    struct TShardState {
        ui64 RetryAttempts = 0;
        std::vector<TReadState*> Reads;
    };

    struct TEvPrivate {
        enum EEv {
            EvRetryRead = EventSpaceBegin(TKikimrEvents::ES_PRIVATE),
            EvSchemeCacheRequestTimeout
        };

        struct TEvSchemeCacheRequestTimeout : public TEventLocal<TEvSchemeCacheRequestTimeout, EvSchemeCacheRequestTimeout> {
        };

        struct TEvRetryRead : public TEventLocal<TEvRetryRead, EvRetryRead> {
            explicit TEvRetryRead(ui64 readId, ui64 lastSeqNo, bool instantStart = false)
                : ReadId(readId)
                , LastSeqNo(lastSeqNo)
                , InstantStart(instantStart) {
            }

            const ui64 ReadId;
            const ui64 LastSeqNo;
            const bool InstantStart;
        };
    };

private:
    void SaveState(const NYql::NDqProto::TCheckpoint&, NYql::NDq::TSourceState&) final {}
    void LoadState(const NYql::NDq::TSourceState&) final {}
    void CommitState(const NYql::NDqProto::TCheckpoint&) final {}

    ui64 GetInputIndex() const final {
        return InputIndex;
    }

    const NYql::NDq::TDqAsyncStats& GetIngressStats() const final {
        return IngressStats;
    }

    void PassAway() final {
        Counters->StreamLookupActorsCount->Dec();
        {
            auto alloc = BindAllocator();
            Input.Clear();
            StreamLookupWorker.reset();
            for (auto& [id, state] : Reads) {
                Counters->SentIteratorCancels->Inc();
                auto cancel = MakeHolder<TEvDataShard::TEvReadCancel>();
                cancel->Record.SetReadId(id);
                Send(MainPipeCacheId, new TEvPipeCache::TEvForward(cancel.Release(), state.ShardId, false));
            }
        }

        Send(MainPipeCacheId, new TEvPipeCache::TEvUnlink(0));
        TActorBootstrapped<TKqpStreamLookupActor>::PassAway();

        LookupActorSpan.End();
    }

    i64 GetAsyncInputData(NKikimr::NMiniKQL::TUnboxedValueBatch& batch, TMaybe<TInstant>&, bool& finished, i64 freeSpace) final {
        YQL_ENSURE(!batch.IsWide(), "Wide stream is not supported");

        auto replyResultStats = StreamLookupWorker->ReplyResult(batch, freeSpace);
        ReadRowsCount += replyResultStats.ReadRowsCount;
        ReadBytesCount += replyResultStats.ReadBytesCount;

        auto status = FetchInputRows();

        if (Partitioning) {
            ProcessInputRows();
        }

        const bool inputRowsFinished = status == NUdf::EFetchStatus::Finish;
        const bool allReadsFinished = AllReadsFinished();
        const bool allRowsProcessed = StreamLookupWorker->AllRowsProcessed();

        if (inputRowsFinished && allReadsFinished && !allRowsProcessed) {
            // all reads are completed, but we have unprocessed rows
            Send(ComputeActorId, new TEvNewAsyncInputDataArrived(InputIndex));
        }

        finished = inputRowsFinished && allReadsFinished && allRowsProcessed;

        CA_LOG_D("Returned " << replyResultStats.ResultBytesCount << " bytes, " << replyResultStats.ResultRowsCount
            << " rows, finished: " << finished);

        return replyResultStats.ResultBytesCount;
    }

    TMaybe<google::protobuf::Any> ExtraData() override {
        google::protobuf::Any result;
        NKikimrTxDataShard::TEvKqpInputActorResultInfo resultInfo;
        for (auto& lock : Locks) {
            resultInfo.AddLocks()->CopyFrom(lock);
        }

        for (auto& lock : BrokenLocks) {
            resultInfo.AddLocks()->CopyFrom(lock);
        }

        result.PackFrom(resultInfo);
        return result;
    }

    STFUNC(StateFunc) {
        try {
            switch (ev->GetTypeRewrite()) {
                hFunc(TEvTxProxySchemeCache::TEvResolveKeySetResult, Handle);
                hFunc(TEvDataShard::TEvReadResult, Handle);
                hFunc(TEvPipeCache::TEvDeliveryProblem, Handle);
                hFunc(TEvPrivate::TEvSchemeCacheRequestTimeout, Handle);
                hFunc(TEvPrivate::TEvRetryRead, Handle);
                IgnoreFunc(TEvTxProxySchemeCache::TEvInvalidateTableResult);
                default:
                    RuntimeError(TStringBuilder() << "Unexpected event: " << ev->GetTypeRewrite(),
                        NYql::NDqProto::StatusIds::INTERNAL_ERROR);
            }
        } catch (const NKikimr::TMemoryLimitExceededException& e) {
            RuntimeError("Memory limit exceeded at stream lookup", NYql::NDqProto::StatusIds::PRECONDITION_FAILED);
        } catch (const yexception& e) {
            RuntimeError(e.what(), NYql::NDqProto::StatusIds::INTERNAL_ERROR);
        }
    }

    void Handle(TEvTxProxySchemeCache::TEvResolveKeySetResult::TPtr& ev) {
        ResoleShardsInProgress = false;
        CA_LOG_D("TEvResolveKeySetResult was received for table: " << StreamLookupWorker->GetTablePath());
        if (ev->Get()->Request->ErrorCount > 0) {
            TString errorMsg = TStringBuilder() << "Failed to get partitioning for table: "
                << StreamLookupWorker->GetTablePath();
            LookupActorStateSpan.EndError(errorMsg);

            return RuntimeError(errorMsg, NYql::NDqProto::StatusIds::SCHEME_ERROR);
        }

        LookupActorStateSpan.EndOk();

        auto& resultSet = ev->Get()->Request->ResultSet;
        YQL_ENSURE(resultSet.size() == 1, "Expected one result for range [NULL, +inf)");
        Partitioning = resultSet[0].KeyDescription->Partitioning;

        ProcessInputRows();
    }

    void Handle(TEvDataShard::TEvReadResult::TPtr& ev) {
        const auto& record = ev->Get()->Record;


        auto readIt = Reads.find(record.GetReadId());
        if (readIt == Reads.end() || readIt->second.State != EReadState::Running) {
            CA_LOG_D("Drop read with readId: " << record.GetReadId() << ", because it's already completed or blocked");
            return;
        }

        auto& read = readIt->second;

        CA_LOG_D("Recv TEvReadResult (stream lookup) from ShardID=" << read.ShardId
            << ", Table = " << StreamLookupWorker->GetTablePath()
            << ", ReadId=" << record.GetReadId() << " (current ReadId=" << ReadId << ")"
            << ", SeqNo=" << record.GetSeqNo()
            << ", Status=" << Ydb::StatusIds::StatusCode_Name(record.GetStatus().GetCode())
            << ", Finished=" << record.GetFinished()
            << ", RowCount=" << record.GetRowCount()
            << ", TxLocks= " << [&]() {
                TStringBuilder builder;
                for (const auto& lock : record.GetTxLocks()) {
                    builder << lock.ShortDebugString();
                }
                return builder;
            }()
            << ", BrokenTxLocks= " << [&]() {
                TStringBuilder builder;
                for (const auto& lock : record.GetBrokenTxLocks()) {
                    builder << lock.ShortDebugString();
                }
                return builder;
            }());

        for (auto& lock : record.GetBrokenTxLocks()) {
            BrokenLocks.push_back(lock);
        }

        for (auto& lock : record.GetTxLocks()) {
            Locks.push_back(lock);
        }

        if (!Snapshot.IsValid()) {
            Snapshot = IKqpGateway::TKqpSnapshot(record.GetSnapshot().GetStep(), record.GetSnapshot().GetTxId());
        }

        Counters->DataShardIteratorMessages->Inc();
        if (record.GetStatus().GetCode() != Ydb::StatusIds::SUCCESS) {
            Counters->DataShardIteratorFails->Inc();
        }

        auto getIssues = [&record]() {
            NYql::TIssues issues;
            NYql::IssuesFromMessage(record.GetStatus().GetIssues(), issues);
            return issues;
        };

        auto replyError = [&](auto message, auto status) {
            return RuntimeError(message, status, getIssues());
        };

        switch (record.GetStatus().GetCode()) {
            case Ydb::StatusIds::SUCCESS:
                break;
            case Ydb::StatusIds::NOT_FOUND:
            {
                StreamLookupWorker->ResetRowsProcessing(read.Id, read.FirstUnprocessedQuery, read.LastProcessedKey);
                read.SetFinished();
                CA_LOG_D("NOT_FOUND was received from tablet: " << read.ShardId << ". "
                    << getIssues().ToOneLineString());
                return ResolveTableShards();
            }
            case Ydb::StatusIds::OVERLOADED: {
                if (CheckTotalRetriesExeeded() || CheckShardRetriesExeeded(read)) {
                    return replyError(
                        TStringBuilder() << "Table '" << StreamLookupWorker->GetTablePath() << "' retry limit exceeded.",
                        NYql::NDqProto::StatusIds::OVERLOADED);
                }
                CA_LOG_D("OVERLOADED was received from tablet: " << read.ShardId << "."
                    << getIssues().ToOneLineString());
                read.SetBlocked();
                return RetryTableRead(read, /*allowInstantRetry = */false);
            }
            case Ydb::StatusIds::INTERNAL_ERROR: {
                if (CheckTotalRetriesExeeded() || CheckShardRetriesExeeded(read)) {
                    return replyError(
                        TStringBuilder() << "Table '" << StreamLookupWorker->GetTablePath() << "' retry limit exceeded.",
                        NYql::NDqProto::StatusIds::INTERNAL_ERROR);
                }
                CA_LOG_D("INTERNAL_ERROR was received from tablet: " << read.ShardId << "."
                    << getIssues().ToOneLineString());
                read.SetBlocked();
                return RetryTableRead(read);
            }
            default: {
                return replyError("Read request aborted", NYql::NDqProto::StatusIds::ABORTED);
            }
        }

        YQL_ENSURE(read.LastSeqNo < record.GetSeqNo());
        read.LastSeqNo = record.GetSeqNo();

        if (record.GetFinished()) {
            read.SetFinished();
        } else {
            YQL_ENSURE(record.HasContinuationToken(), "Successful TEvReadResult should contain continuation token");
            NKikimrTxDataShard::TReadContinuationToken continuationToken;
            bool parseResult = continuationToken.ParseFromString(record.GetContinuationToken());
            YQL_ENSURE(parseResult, "Failed to parse continuation token");
            read.FirstUnprocessedQuery = continuationToken.GetFirstUnprocessedQuery();

            if (continuationToken.HasLastProcessedKey()) {
                TSerializedCellVec lastKey(continuationToken.GetLastProcessedKey());
                read.LastProcessedKey = TOwnedCellVec(lastKey.GetCells());
            } else {
                read.LastProcessedKey.Clear();
            }

            Counters->SentIteratorAcks->Inc();
            THolder<TEvDataShard::TEvReadAck> request(new TEvDataShard::TEvReadAck());
            request->Record.SetReadId(record.GetReadId());
            request->Record.SetSeqNo(record.GetSeqNo());

            auto defaultSettings = GetDefaultReadAckSettings()->Record;
            request->Record.SetMaxRows(defaultSettings.GetMaxRows());
            request->Record.SetMaxBytes(defaultSettings.GetMaxBytes());

            Send(MainPipeCacheId, new TEvPipeCache::TEvForward(request.Release(), read.ShardId, true),
                IEventHandle::FlagTrackDelivery);

            CA_LOG_D("TEvReadAck was sent to shard: " << read.ShardId);

            if (auto delay = ShardTimeout()) {
                TlsActivationContext->Schedule(
                    *delay, new IEventHandle(SelfId(), SelfId(), new TEvPrivate::TEvRetryRead(read.Id, read.LastSeqNo))
                );
            }
        }

        auto guard = BindAllocator();
        StreamLookupWorker->AddResult(TKqpStreamLookupWorker::TShardReadResult{
            read.ShardId, THolder<TEventHandle<TEvDataShard::TEvReadResult>>(ev.Release())
        });
        Send(ComputeActorId, new TEvNewAsyncInputDataArrived(InputIndex));
    }

    void Handle(TEvPipeCache::TEvDeliveryProblem::TPtr& ev) {
        CA_LOG_D("TEvDeliveryProblem was received from tablet: " << ev->Get()->TabletId);

        const auto& tabletId = ev->Get()->TabletId;
        auto shardIt = ReadsPerShard.find(tabletId);
        YQL_ENSURE(shardIt != ReadsPerShard.end());

        TVector<TReadState*> toRetry;
        for (auto* read : shardIt->second.Reads) {
            if (read->State == EReadState::Running) {
                Counters->IteratorDeliveryProblems->Inc();
                toRetry.push_back(read);
            }
        }
        for (auto* read : toRetry) {
            read->SetBlocked();
            RetryTableRead(*read);
        }
    }

    void Handle(TEvPrivate::TEvSchemeCacheRequestTimeout::TPtr&) {
        CA_LOG_D("TEvSchemeCacheRequestTimeout was received, shards for table " << StreamLookupWorker->GetTablePath()
            << " was resolved: " << !!Partitioning);

        if (!Partitioning) {
            LookupActorStateSpan.EndError("timeout exceeded");
            CA_LOG_D("Retry attempt to resolve shards for table: " << StreamLookupWorker->GetTablePath());
            ResoleShardsInProgress = false;
            ResolveTableShards();
        }
    }

    void Handle(TEvPrivate::TEvRetryRead::TPtr& ev) {
        auto readIt = Reads.find(ev->Get()->ReadId);
        YQL_ENSURE(readIt != Reads.end(), "Unexpected readId: " << ev->Get()->ReadId);
        auto& read = readIt->second;

        YQL_ENSURE(read.State != EReadState::Blocked || read.LastSeqNo <= ev->Get()->LastSeqNo);

        if ((read.State == EReadState::Running && read.LastSeqNo <= ev->Get()->LastSeqNo) || read.State == EReadState::Blocked) {
            if (ev->Get()->InstantStart) {
                read.SetFinished();
                auto requests = StreamLookupWorker->RebuildRequest(read.Id, read.FirstUnprocessedQuery, read.LastProcessedKey, ReadId);
                for (auto& request : requests) {
                    StartTableRead(read.ShardId, std::move(request));
                }
            } else {
                RetryTableRead(read);
            }
        }
    }

    NUdf::EFetchStatus FetchInputRows() {
        auto guard = BindAllocator();

        NUdf::EFetchStatus status;
        NUdf::TUnboxedValue row;
        while ((status = Input.Fetch(row)) == NUdf::EFetchStatus::Ok) {
            StreamLookupWorker->AddInputRow(std::move(row));
        }

        return status;
    }

    void ProcessInputRows() {
        YQL_ENSURE(Partitioning, "Table partitioning should be initialized before lookup keys processing");

        auto guard = BindAllocator();

        auto requests = StreamLookupWorker->BuildRequests(Partitioning, ReadId);
        for (auto& [shardId, request] : requests) {
            StartTableRead(shardId, std::move(request));
        }
    }

    void StartTableRead(ui64 shardId, THolder<TEvDataShard::TEvRead> request) {
        Counters->CreatedIterators->Inc();
        auto& record = request->Record;

        CA_LOG_D("Start reading of table: " << StreamLookupWorker->GetTablePath() << ", readId: " << record.GetReadId()
            << ", shardId: " << shardId);

        TReadState read(record.GetReadId(), shardId);

        if (Snapshot.IsValid()) {
            record.MutableSnapshot()->SetStep(Snapshot.Step);
            record.MutableSnapshot()->SetTxId(Snapshot.TxId);
        } else {
            YQL_ENSURE(AllowInconsistentReads, "Expected valid snapshot or enabled inconsistent read mode");
        }

        if (LockTxId && BrokenLocks.empty()) {
            record.SetLockTxId(*LockTxId);
            if (LockMode) {
                record.SetLockMode(*LockMode);
            }
        }

        if (NodeLockId) {
            record.SetLockNodeId(*NodeLockId);
        }

        auto defaultSettings = GetDefaultReadSettings()->Record;
        record.SetMaxRows(defaultSettings.GetMaxRows());
        record.SetMaxBytes(defaultSettings.GetMaxBytes());
        record.SetResultFormat(NKikimrDataEvents::FORMAT_CELLVEC);

        CA_LOG_D(TStringBuilder() << "Send EvRead (stream lookup) to shardId=" << shardId
            << ", readId = " << record.GetReadId()
            << ", tablePath: " << StreamLookupWorker->GetTablePath()
            << ", snapshot=(txid=" << record.GetSnapshot().GetTxId() << ", step=" << record.GetSnapshot().GetStep() << ")"
            << ", lockTxId=" << record.GetLockTxId()
            << ", lockNodeId=" << record.GetLockNodeId());

        Send(MainPipeCacheId, new TEvPipeCache::TEvForward(request.Release(), shardId, true),
            IEventHandle::FlagTrackDelivery, 0, LookupActorSpan.GetTraceId());

        read.State = EReadState::Running;

        auto readId = read.Id;
        auto lastSeqNo = read.LastSeqNo;
        const auto [readIt, succeeded] = Reads.insert({readId, std::move(read)});
        YQL_ENSURE(succeeded);
        ReadsPerShard[shardId].Reads.push_back(&readIt->second);

        if (auto delay = ShardTimeout()) {
            TlsActivationContext->Schedule(
                *delay, new IEventHandle(SelfId(), SelfId(), new TEvPrivate::TEvRetryRead(readId, lastSeqNo))
            );
        }
    }

    bool CheckTotalRetriesExeeded() {
        const auto limit = MaxTotalRetries();
        return limit && TotalRetryAttempts + 1 > *limit;
    }

    bool CheckShardRetriesExeeded(TReadState& failedRead) {
        const auto& shardState = ReadsPerShard[failedRead.ShardId];
        return shardState.RetryAttempts + 1 > MaxShardRetries();
    }

    void RetryTableRead(TReadState& failedRead, bool allowInstantRetry = true) {
        CA_LOG_D("Retry reading of table: " << StreamLookupWorker->GetTablePath() << ", readId: " << failedRead.Id
            << ", shardId: " << failedRead.ShardId);

        if (CheckTotalRetriesExeeded()) {
            return RuntimeError(TStringBuilder() << "Table '" << StreamLookupWorker->GetTablePath() << "' retry limit exceeded",
                NYql::NDqProto::StatusIds::UNAVAILABLE);
        }
        ++TotalRetryAttempts;

        if (CheckShardRetriesExeeded(failedRead)) {
            StreamLookupWorker->ResetRowsProcessing(failedRead.Id, failedRead.FirstUnprocessedQuery, failedRead.LastProcessedKey);
            failedRead.SetFinished();
            return ResolveTableShards();
        }
        auto& shardState = ReadsPerShard[failedRead.ShardId];
        ++shardState.RetryAttempts;

        auto delay = CalcDelay(shardState.RetryAttempts, allowInstantRetry);
        if (delay == TDuration::Zero()) {
            failedRead.SetFinished();
            auto requests = StreamLookupWorker->RebuildRequest(failedRead.Id, failedRead.FirstUnprocessedQuery, failedRead.LastProcessedKey, ReadId);
            for (auto& request : requests) {
                StartTableRead(failedRead.ShardId, std::move(request));
            }
        } else {
            CA_LOG_D("Schedule retry atempt for readId: " << failedRead.Id << " after " << delay);
            TlsActivationContext->Schedule(
                delay, new IEventHandle(SelfId(), SelfId(), new TEvPrivate::TEvRetryRead(failedRead.Id, failedRead.LastSeqNo, /*instantStart = */ true))
            );
        }
    }

    void ResolveTableShards() {
        if (ResoleShardsInProgress) {
            return;
        }

        if (++TotalResolveShardsAttempts > MaxShardResolves()) {
            return RuntimeError(TStringBuilder() << "Table '" << StreamLookupWorker->GetTablePath() << "' resolve attempts limit exceeded",
                NYql::NDqProto::StatusIds::UNAVAILABLE);
        }

        CA_LOG_D("Resolve shards for table: " << StreamLookupWorker->GetTablePath());
        ResoleShardsInProgress = true;

        Partitioning.reset();

        auto request = MakeHolder<NSchemeCache::TSchemeCacheRequest>();

        auto keyColumnTypes = StreamLookupWorker->GetKeyColumnTypes();

        TVector<TCell> minusInf(keyColumnTypes.size());
        TVector<TCell> plusInf;
        TTableRange range(minusInf, true, plusInf, true, false);

        request->ResultSet.emplace_back(MakeHolder<TKeyDesc>(StreamLookupWorker->GetTableId(), range, TKeyDesc::ERowOperation::Read,
            keyColumnTypes, TVector<TKeyDesc::TColumnOp>{}));

        Counters->IteratorsShardResolve->Inc();
        LookupActorStateSpan = NWilson::TSpan(TWilsonKqp::LookupActorShardsResolve, LookupActorSpan.GetTraceId(),
            "WaitForShardsResolve", NWilson::EFlags::AUTO_END);

        Send(MakeSchemeCacheID(), new TEvTxProxySchemeCache::TEvResolveKeySet(request));

        SchemeCacheRequestTimeoutTimer = CreateLongTimer(TlsActivationContext->AsActorContext(), SchemeCacheRequestTimeout,
            new IEventHandle(SelfId(), SelfId(), new TEvPrivate::TEvSchemeCacheRequestTimeout()));
    }

    bool AllReadsFinished() const {
        for (const auto& [_, read] : Reads) {
            if (!read.Finished()) {
                return false;
            }
        }

        return true;
    }

    TGuard<NKikimr::NMiniKQL::TScopedAlloc> BindAllocator() {
        return TypeEnv.BindAllocator();
    }

    void RuntimeError(const TString& message, NYql::NDqProto::StatusIds::StatusCode statusCode, const NYql::TIssues& subIssues = {}) {
        NYql::TIssue issue(message);
        for (const auto& i : subIssues) {
            issue.AddSubIssue(MakeIntrusive<NYql::TIssue>(i));
        }

        NYql::TIssues issues;
        issues.AddIssue(std::move(issue));

        if (LookupActorSpan) {
            LookupActorSpan.EndError(issues.ToOneLineString());
        }

        Send(ComputeActorId, new TEvAsyncInputError(InputIndex, std::move(issues), statusCode));
    }

private:
    const TString LogPrefix;
    const ui64 InputIndex;
    NYql::NDq::TDqAsyncStats IngressStats;
    NUdf::TUnboxedValue Input;
    const NActors::TActorId ComputeActorId;
    const NMiniKQL::TTypeEnvironment& TypeEnv;
    std::shared_ptr<NKikimr::NMiniKQL::TScopedAlloc> Alloc;
    IKqpGateway::TKqpSnapshot Snapshot;
    const bool AllowInconsistentReads;
    const TMaybe<ui64> LockTxId;
    const TMaybe<ui32> NodeLockId;
    const TMaybe<NKikimrDataEvents::ELockMode> LockMode;
    std::unordered_map<ui64, TReadState> Reads;
    std::unordered_map<ui64, TShardState> ReadsPerShard;
    std::shared_ptr<const TVector<TKeyDesc::TPartitionInfo>> Partitioning;
    const TDuration SchemeCacheRequestTimeout;
    NActors::TActorId SchemeCacheRequestTimeoutTimer;
    TVector<NKikimrDataEvents::TLock> Locks;
    TVector<NKikimrDataEvents::TLock> BrokenLocks;
    NKqpProto::EStreamLookupStrategy LookupStrategy;
    std::unique_ptr<TKqpStreamLookupWorker> StreamLookupWorker;
    ui64 ReadId = 0;
    size_t TotalRetryAttempts = 0;
    size_t TotalResolveShardsAttempts = 0;
    bool ResoleShardsInProgress = false;

    // stats
    ui64 ReadRowsCount = 0;
    ui64 ReadBytesCount = 0;

    TIntrusivePtr<TKqpCounters> Counters;
    NWilson::TSpan LookupActorSpan;
    NWilson::TSpan LookupActorStateSpan;
};

} // namespace

std::pair<NYql::NDq::IDqComputeActorAsyncInput*, NActors::IActor*> CreateStreamLookupActor(NYql::NDq::IDqAsyncIoFactory::TInputTransformArguments&& args,
    NKikimrKqp::TKqpStreamLookupSettings&& settings, TIntrusivePtr<TKqpCounters> counters) {
    auto actor = new TKqpStreamLookupActor(std::move(args), std::move(settings), counters);
    return {actor, actor};
}

void InterceptStreamLookupActorPipeCache(NActors::TActorId id) {
    MainPipeCacheId = id;
}

} // namespace NKqp
} // namespace NKikimr
