#include "bundle_state.h"
#include "config.h"
#include "move_iteration.h"
#include "private.h"
#include "table_registry.h"

#include <yt/yt/server/lib/tablet_balancer/balancing_helpers.h>
#include <yt/yt/server/lib/tablet_balancer/config.h>
#include <yt/yt/server/lib/tablet_balancer/parameterized_balancing_helpers.h>
#include <yt/yt/server/lib/tablet_balancer/table.h>
#include <yt/yt/server/lib/tablet_balancer/tablet_cell_bundle.h>

#include <yt/yt/client/table_client/schema.h>

namespace NYT::NTabletBalancer {

using namespace NConcurrency;
using namespace NObjectClient;

////////////////////////////////////////////////////////////////////////////////

static constexpr auto& Logger = TabletBalancerLogger;

////////////////////////////////////////////////////////////////////////////////

class TMoveIterationBase
    : public IMoveIteration
{
public:
    TMoveIterationBase(
        TString groupName,
        TBundleStatePtr bundle,
        TTabletBalancingGroupConfigPtr groupConfig,
        TTabletBalancerDynamicConfigPtr dynamicConfig)
        : BundleName_(bundle->GetBundle()->Name)
        , GroupName_(std::move(groupName))
        , Bundle_(std::move(bundle))
        , GroupConfig_(std::move(groupConfig))
        , DynamicConfig_(std::move(dynamicConfig))
    { }

    void StartIteration() const override
    {
        YT_LOG_DEBUG("Balancing tablets via move started (BundleName: %v, Group: %v, MoveBalancingType: %v)",
            BundleName_,
            GroupName_,
            GetActionSubtypeName());
    }

    void LogDisabledBalancing() const override
    {
        YT_LOG_DEBUG("Balancing tablets via move is disabled (BundleName: %v, Group: %v, MoveBalancingType: %v)",
            BundleName_,
            GroupName_,
            GetActionSubtypeName());
    }

    void FinishIteration(int actionCount) const override
    {
        YT_LOG_DEBUG("Balancing tablets via move finished (BundleName: %v, Group: %v, MoveBalancingType: %v, ActionCount: %v)",
            BundleName_,
            GroupName_,
            GetActionSubtypeName(),
            actionCount);
    }

    const TString& GetBundleName() const override
    {
        return BundleName_;
    }

    const TString& GetGroupName() const override
    {
        return GroupName_;
    }

    const TBundleStatePtr& GetBundle() const override
    {
        return Bundle_;
    }

    const TTabletBalancerDynamicConfigPtr& GetDynamicConfig() const override
    {
        return DynamicConfig_;
    }

    const TTabletBalancingGroupConfigPtr& GetGroupConfig() const override
    {
        return GroupConfig_;
    }

    void Prepare(const TTableRegistryPtr& /*tableRegistry*/) override
    { }

protected:
    const TString BundleName_;
    const TString GroupName_;
    const TBundleStatePtr Bundle_;
    const TTabletBalancingGroupConfigPtr GroupConfig_;
    const TTabletBalancerDynamicConfigPtr DynamicConfig_;

    TTableProfilingCounters& GetProfilingCounters(const TTable* table)
    {
        return Bundle_->GetProfilingCounters(table, GroupName_);
    }
};

////////////////////////////////////////////////////////////////////////////////

class TOrdinaryMoveIteration
    : public TMoveIterationBase
{
public:
    TOrdinaryMoveIteration(
        TBundleStatePtr bundle,
        TTabletBalancingGroupConfigPtr groupConfig,
        TTabletBalancerDynamicConfigPtr dynamicConfig)
        : TMoveIterationBase(
            LegacyOrdinaryGroupName,
            std::move(bundle),
            std::move(groupConfig),
            std::move(dynamicConfig))
    { }

    bool IsGroupBalancingEnabled() const override
    {
        return Bundle_->GetBundle()->Config->EnableCellBalancer;
    }

    EBalancingMode GetBalancingMode() const override
    {
        return EBalancingMode::OrdinaryMove;
    }

    TFuture<std::vector<TMoveDescriptor>> ReassignTablets(const IInvokerPtr& invoker) override
    {
        return BIND(
            ReassignOrdinaryTablets,
            Bundle_->GetBundle(),
            /*movableTables*/ std::nullopt,
            Logger())
            .AsyncVia(invoker)
            .Run();
    }

    TStringBuf GetActionSubtypeName() const override
    {
        static const TStringBuf subtypeName = "ordinary";
        return subtypeName;
    }

    void UpdateProfilingCounters(const TTable* table) override
    {
        GetProfilingCounters(table).OrdinaryMoves.Increment(1);
    }
};

////////////////////////////////////////////////////////////////////////////////

class TInMemoryMoveIteration
    : public TMoveIterationBase
{
public:
    TInMemoryMoveIteration(
        TBundleStatePtr bundle,
        TTabletBalancingGroupConfigPtr groupConfig,
        TTabletBalancerDynamicConfigPtr dynamicConfig)
        : TMoveIterationBase(
            LegacyInMemoryGroupName,
            std::move(bundle),
            std::move(groupConfig),
            std::move(dynamicConfig))
    { }

    bool IsGroupBalancingEnabled() const override
    {
        return Bundle_->GetBundle()->Config->EnableInMemoryCellBalancer;
    }

    EBalancingMode GetBalancingMode() const override
    {
        return EBalancingMode::InMemoryMove;
    }

    TFuture<std::vector<TMoveDescriptor>> ReassignTablets(const IInvokerPtr& invoker) override
    {
        return BIND(
            ReassignInMemoryTablets,
            Bundle_->GetBundle(),
            /*movableTables*/ std::nullopt,
            /*ignoreTableWiseConfig*/ false,
            Logger())
            .AsyncVia(invoker)
            .Run();
    }

    TStringBuf GetActionSubtypeName() const override
    {
        static const TStringBuf subtypeName = "in-memory";
        return subtypeName;
    }

    void UpdateProfilingCounters(const TTable* table) override
    {
        GetProfilingCounters(table).InMemoryMoves.Increment(1);
    }
};

////////////////////////////////////////////////////////////////////////////////

class TParameterizedMoveIterationBase
    : public TMoveIterationBase
{
public:
    TParameterizedMoveIterationBase(
        TString groupName,
        TBundleStatePtr bundle,
        TTableParameterizedMetricTrackerPtr metricTracker,
        TTabletBalancingGroupConfigPtr groupConfig,
        TTabletBalancerDynamicConfigPtr dynamicConfig)
        : TMoveIterationBase(
            std::move(groupName),
            std::move(bundle),
            std::move(groupConfig),
            std::move(dynamicConfig))
        , MetricTracker_(std::move(metricTracker))
    { }

    bool IsGroupBalancingEnabled() const override
    {
        return true;
    }

protected:
    TTableParameterizedMetricTrackerPtr MetricTracker_;

    TParameterizedReassignSolverConfig GetReassignSolverConfig()
    {
        return TParameterizedReassignSolverConfig{
            .MaxMoveActionCount = DynamicConfig_->MaxParameterizedMoveActionCount,
            .NodeDeviationThreshold = DynamicConfig_->ParameterizedNodeDeviationThreshold,
            .CellDeviationThreshold = DynamicConfig_->ParameterizedCellDeviationThreshold,
            .MinRelativeMetricImprovement = DynamicConfig_->ParameterizedMinRelativeMetricImprovement,
            .Metric = DynamicConfig_->DefaultParameterizedMetric,
            .Factors = DynamicConfig_->ParameterizedFactors,
        }.MergeWith(
            GroupConfig_->Parameterized,
            std::min(DynamicConfig_->MaxActionsPerGroup, DynamicConfig_->MaxParameterizedMoveActionHardLimit));
    }
};

////////////////////////////////////////////////////////////////////////////////

class TParameterizedMoveIteration
    : public TParameterizedMoveIterationBase
{
public:
    TParameterizedMoveIteration(
        TString groupName,
        TBundleStatePtr bundle,
        TTableParameterizedMetricTrackerPtr metricTracker,
        TTabletBalancingGroupConfigPtr groupConfig,
        TTabletBalancerDynamicConfigPtr dynamicConfig)
        : TParameterizedMoveIterationBase(
            std::move(groupName),
            std::move(bundle),
            std::move(metricTracker),
            std::move(groupConfig),
            std::move(dynamicConfig))
    { }

    EBalancingMode GetBalancingMode() const override
    {
        return EBalancingMode::ParameterizedMove;
    }

    TFuture<std::vector<TMoveDescriptor>> ReassignTablets(const IInvokerPtr& invoker) override
    {
        return BIND(
            ReassignTabletsParameterized,
            Bundle_->GetBundle(),
            Bundle_->PerformanceCountersKeys(),
            GetReassignSolverConfig(),
            GroupName_,
            MetricTracker_,
            Logger())
            .AsyncVia(invoker)
            .Run();
    }

    TStringBuf GetActionSubtypeName() const override
    {
        static const TStringBuf subtypeName = "parameterized";
        return subtypeName;
    }

    bool IsGroupBalancingEnabled() const override
    {
        return GroupConfig_->Parameterized->ReplicaClusters.empty();
    }

    void UpdateProfilingCounters(const TTable* table) override
    {
        GetProfilingCounters(table).ParameterizedMoves.Increment(1);
    }
};

////////////////////////////////////////////////////////////////////////////////

class TReplicaMoveIteration
    : public TParameterizedMoveIterationBase
{
public:
    TReplicaMoveIteration(
        TString groupName,
        TBundleStatePtr bundle,
        TTableParameterizedMetricTrackerPtr metricTracker,
        TTabletBalancingGroupConfigPtr groupConfig,
        TTabletBalancerDynamicConfigPtr dynamicConfig,
        std::string selfClusterName)
        : TParameterizedMoveIterationBase(
            std::move(groupName),
            std::move(bundle),
            std::move(metricTracker),
            std::move(groupConfig),
            std::move(dynamicConfig))
        , SelfClusterName_(std::move(selfClusterName))
    { }

    EBalancingMode GetBalancingMode() const override
    {
        return EBalancingMode::ReplicaMove;
    }

    void Prepare(const TTableRegistryPtr& tableRegistry) override
    {
        if (Bundle_->IsLastReplicaMoveBalancingFetchFailed()) {
            YT_LOG_DEBUG("Balancing tablets via replica move is not possible because "
                "last statistics fetch failed (BundleName: %v, Group: %v)",
                BundleName_,
                GroupName_);
            THROW_ERROR_EXCEPTION("Not all statistics for replica move balancing were fetched");
        }

        for (const auto& [id, table] : Bundle_->GetBundle()->Tables) {
            if (table->GetBalancingGroup() != GroupName_) {
                continue;
            }

            THROW_ERROR_EXCEPTION_IF(table->PivotKeys.empty(),
                "Pivot keys for table %v were not fetched successfully",
                id);

            YT_VERIFY(std::ssize(table->PivotKeys) == std::ssize(table->Tablets));

            for (const auto& [cluster, minorTablePaths] : table->GetReplicaBalancingMinorTables(SelfClusterName_)) {
                // TODO(alexelexa): detect bunned or unavaliable clusters and skip it.

                for (const auto& minorTablePath : minorTablePaths) {
                    auto it = tableRegistry->AlienTablePaths().find(TTableRegistry::TAlienTableTag(cluster, minorTablePath));
                    THROW_ERROR_EXCEPTION_IF(it == tableRegistry->AlienTablePaths().end(),
                        "Not all tables was resolved. Table id for table %v on cluster %v was not found. "
                        "Check that table path is correct",
                        minorTablePath,
                        cluster);
                }
            }
        }

        YT_LOG_DEBUG("Preparations for balancing tablets via move finished (BundleName: %v, Group: %v, MoveBalancingType: %v)",
            BundleName_,
            GroupName_,
            GetActionSubtypeName());
    }

    TFuture<std::vector<TMoveDescriptor>> ReassignTablets(const IInvokerPtr& invoker) override
    {
        return BIND(
            ReassignTabletsReplica,
            Bundle_->GetBundle(),
            Bundle_->PerformanceCountersKeys(),
            GetReassignSolverConfig(),
            GroupName_,
            MetricTracker_,
            Logger())
            .AsyncVia(invoker)
            .Run();
    }

    TStringBuf GetActionSubtypeName() const override
    {
        static const TStringBuf subtypeName = "replica";
        return subtypeName;
    }

    bool IsGroupBalancingEnabled() const override
    {
        return !GroupConfig_->Parameterized->ReplicaClusters.empty();
    }

    void UpdateProfilingCounters(const TTable* table) override
    {
        GetProfilingCounters(table).ReplicaMoves.Increment(1);
    }

private:
    const std::string SelfClusterName_;
};

////////////////////////////////////////////////////////////////////////////////

IMoveIterationPtr CreateOrdinaryMoveIteration(
    TBundleStatePtr bundleState,
    TTabletBalancingGroupConfigPtr groupConfig,
    TTabletBalancerDynamicConfigPtr dynamicConfig)
{
    return New<TOrdinaryMoveIteration>(
        std::move(bundleState),
        std::move(groupConfig),
        std::move(dynamicConfig));
}

IMoveIterationPtr CreateInMemoryMoveIteration(
    TBundleStatePtr bundleState,
    TTabletBalancingGroupConfigPtr groupConfig,
    TTabletBalancerDynamicConfigPtr dynamicConfig)
{
    return New<TInMemoryMoveIteration>(
        std::move(bundleState),
        std::move(groupConfig),
        std::move(dynamicConfig));
}

IMoveIterationPtr CreateParameterizedMoveIteration(
    TString groupName,
    TBundleStatePtr bundleState,
    TTableParameterizedMetricTrackerPtr metricTracker,
    TTabletBalancingGroupConfigPtr groupConfig,
    TTabletBalancerDynamicConfigPtr dynamicConfig)
{
    return New<TParameterizedMoveIteration>(
        std::move(groupName),
        std::move(bundleState),
        std::move(metricTracker),
        std::move(groupConfig),
        std::move(dynamicConfig));
}

IMoveIterationPtr CreateReplicaMoveIteration(
    TString groupName,
    TBundleStatePtr bundleState,
    TTableParameterizedMetricTrackerPtr metricTracker,
    TTabletBalancingGroupConfigPtr groupConfig,
    TTabletBalancerDynamicConfigPtr dynamicConfig,
    std::string selfClusterName)
{
    return New<TReplicaMoveIteration>(
        std::move(groupName),
        std::move(bundleState),
        std::move(metricTracker),
        std::move(groupConfig),
        std::move(dynamicConfig),
        std::move(selfClusterName));
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NTabletBalancer
