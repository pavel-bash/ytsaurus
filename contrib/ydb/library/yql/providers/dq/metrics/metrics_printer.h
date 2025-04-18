#pragma once

#include <yql/essentials/providers/common/metrics/metrics_registry.h>

#include <contrib/ydb/library/actors/core/actor_bootstrapped.h>


namespace NYql {

NActors::IActor* CreateMetricsPrinter(const NMonitoring::TDynamicCounterPtr& counters);

} // namespace NYql
