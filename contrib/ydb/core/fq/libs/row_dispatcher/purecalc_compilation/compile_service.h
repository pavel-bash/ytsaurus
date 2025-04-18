#pragma once

#include <contrib/ydb/core/fq/libs/config/protos/row_dispatcher.pb.h>

#include <contrib/ydb/library/actors/core/actor.h>

namespace NFq::NRowDispatcher {

NActors::IActor* CreatePurecalcCompileService(const NConfig::TCompileServiceConfig& config, NMonitoring::TDynamicCounterPtr counters);

}  // namespace NFq::NRowDispatcher
