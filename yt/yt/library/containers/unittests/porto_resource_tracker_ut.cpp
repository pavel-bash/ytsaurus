#include <yt/yt/core/test_framework/framework.h>

#include <yt/yt/core/ytree/convert.h>

#include <util/system/fs.h>
#include <util/system/tempfile.h>

#include <yt/yt/library/profiling/producer.h>
#include <yt/yt/library/containers/config.h>
#include <yt/yt/library/containers/porto_executor.h>
#include <yt/yt/library/containers/porto_resource_tracker.h>
#include <yt/yt/library/containers/instance.h>

#include <library/cpp/yt/memory/leaky_ref_counted_singleton.h>

#include <util/system/platform.h>
#include <util/system/env.h>

namespace NYT::NContainers {
namespace {

using namespace NConcurrency;

////////////////////////////////////////////////////////////////////////////////

static constexpr auto TestUpdatePeriod = TDuration::MilliSeconds(10);

class TPortoTrackerTest
    : public ::testing::Test
{
protected:
    IPortoExecutorPtr Executor_;

    void SetUp() override
    {
        if (GetEnv("SKIP_PORTO_TESTS") != "") {
            GTEST_SKIP();
        }

        Executor_ = CreatePortoExecutor(New<TPortoExecutorDynamicConfig>(), "default");
    }
};

TString GetUniqueName()
{
    return "yt_porto_ut_" + ToString(TGuid::Create());
}

TPortoResourceTrackerPtr CreateSumPortoTracker(IPortoExecutorPtr Executor, const TString& name)
{
    return New<TPortoResourceTracker>(
        GetPortoInstance(Executor, name),
        TestUpdatePeriod,
        false);
}

TPortoResourceProfilerPtr CreateDeltaPortoProfiler(IPortoExecutorPtr executor, const TString& name)
{
    auto instance = GetPortoInstance(executor, name);
    auto portoResourceTracker = New<TPortoResourceTracker>(
        instance,
        ResourceUsageUpdatePeriod,
        true,
        true);

    // Init metrics for delta tracker.
    portoResourceTracker->GetTotalStatistics();

    return LeakyRefCountedSingleton<TPortoResourceProfiler>(
        portoResourceTracker,
        New<TPodSpecConfig>(),
        TProfiler("/porto")
            .WithTag("porto_name", instance->GetName())
            .WithTag("container_category", "yt_daemon"));
}

void AssertGauges(const std::vector<std::tuple<std::string, TTagList, double>>& gauges)
{
    static const THashSet<std::string> sensors{
        "/cpu/burst",
        "/cpu/user",
        "/cpu/total",
        "/cpu/system",
        "/cpu/wait",
        "/cpu/throttled",
        "/cpu/cfs_throttled",
        "/cpu/guarantee",
        "/cpu/limit",
        "/cpu/thread_count",
        "/cpu/context_switches",

        "/memory/minor_page_faults",
        "/memory/major_page_faults",
        "/memory/file_cache_usage",
        "/memory/anon_usage",
        "/memory/anon_limit",
        "/memory/memory_usage",
        "/memory/memory_guarantee",
        "/memory/memory_limit",

        "/io/read_bytes",
        "/io/write_bytes",
        "/io/bytes_limit",

        "/io/read_ops",
        "/io/write_ops",
        "/io/ops",
        "/io/ops_limit",
        "/io/total",

        "/network/rx_bytes",
        "/network/rx_drops",
        "/network/rx_packets",
        "/network/rx_limit",
        "/network/tx_bytes",
        "/network/tx_drops",
        "/network/tx_packets",
        "/network/tx_limit",
    };

    static const THashSet<std::string> mayBeEmpty{
        "/cpu/burst",
        "/cpu/wait",
        "/cpu/throttled",
        "/cpu/cfs_throttled",
        "/cpu/guarantee",
        "/cpu/context_switches",
        "/memory/major_page_faults",
        "/memory/memory_guarantee",
        "/io/ops_limit",
        "/io/read_ops",
        "/io/write_ops",
        "/io/wait",
        "/io/bytes_limit",
        "/network/rx_bytes",
        "/network/rx_drops",
        "/network/rx_packets",
        "/network/rx_limit",
        "/network/tx_bytes",
        "/network/tx_drops",
        "/network/tx_packets",
        "/network/tx_limit"
    };

    for (const auto& [name, tags, value] : gauges) {
        EXPECT_TRUE(value >= 0 && sensors.contains(name) || mayBeEmpty.contains(name));
    }
}

TEST_F(TPortoTrackerTest, ValidateSummaryPortoTracker)
{
    auto name = GetUniqueName();

    WaitFor(Executor_->CreateContainer(
        TRunnableContainerSpec {
            .Name = name,
            .Command = "sleep .1",
        }, true))
        .ThrowOnError();

    auto tracker = CreateSumPortoTracker(Executor_, name);

    auto firstStatistics = tracker->GetTotalStatistics();

    WaitFor(Executor_->StopContainer(name))
        .ThrowOnError();
    WaitFor(Executor_->SetContainerProperty(
        name,
        "command",
        "find /"))
        .ThrowOnError();
    WaitFor(Executor_->StartContainer(name))
        .ThrowOnError();
    Sleep(TDuration::MilliSeconds(500));

    auto secondStatistics = tracker->GetTotalStatistics();

    WaitFor(Executor_->DestroyContainer(name))
        .ThrowOnError();
}

TEST_F(TPortoTrackerTest, ValidateDeltaPortoTracker)
{
    auto name = GetUniqueName();

    auto spec = TRunnableContainerSpec{
        .Name = name,
        .Command = "sleep .1",
    };

    WaitFor(Executor_->CreateContainer(spec, true))
        .ThrowOnError();

    auto profiler = CreateDeltaPortoProfiler(Executor_, name);

    WaitFor(Executor_->StopContainer(name))
        .ThrowOnError();
    WaitFor(Executor_->SetContainerProperty(
        name,
        "command",
        "find /"))
        .ThrowOnError();
    WaitFor(Executor_->StartContainer(name))
        .ThrowOnError();

    Sleep(TDuration::MilliSeconds(500));

    auto buffer = New<TSensorBuffer>();
    profiler->CollectSensors(buffer.Get());
    AssertGauges(buffer->GetGauges());

    WaitFor(Executor_->DestroyContainer(name))
        .ThrowOnError();
}

TEST_F(TPortoTrackerTest, ValidateDeltaRootPortoTracker)
{
    auto name = GetUniqueName();

    auto spec = TRunnableContainerSpec{
        .Name = name,
        .Command = "sleep .1",
    };

    WaitFor(Executor_->CreateContainer(spec, true))
        .ThrowOnError();

    auto profiler = CreateDeltaPortoProfiler(
        Executor_,
        GetPortoInstance(
            Executor_,
            *GetPortoInstance(Executor_, name)->GetRootName())->GetName());

    WaitFor(Executor_->StopContainer(name))
        .ThrowOnError();
    WaitFor(Executor_->SetContainerProperty(
        name,
        "command",
        "find /"))
        .ThrowOnError();
    WaitFor(Executor_->StartContainer(name))
        .ThrowOnError();

    Sleep(TDuration::MilliSeconds(500));

    auto buffer = New<TSensorBuffer>();
    profiler->CollectSensors(buffer.Get());
    AssertGauges(buffer->GetGauges());

    WaitFor(Executor_->DestroyContainer(name))
        .ThrowOnError();
}

////////////////////////////////////////////////////////////////////////////////

} // namespace
} // namespace NYT::NContainers
