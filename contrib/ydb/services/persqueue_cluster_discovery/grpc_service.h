#pragma once

#include <contrib/ydb/library/actors/core/actorsystem.h>
#include <contrib/ydb/public/api/grpc/draft/ydb_persqueue_v1.grpc.pb.h>
#include <contrib/ydb/library/grpc/server/grpc_server.h>

namespace NKikimr::NGRpcService {

class TGRpcPQClusterDiscoveryService
   : public NYdbGrpc::TGrpcServiceBase<Ydb::PersQueue::V1::ClusterDiscoveryService> {
public:
    TGRpcPQClusterDiscoveryService(NActors::TActorSystem* system, TIntrusivePtr<::NMonitoring::TDynamicCounters> counters,
                                   NActors::TActorId id, const TMaybe<ui64>& requestsInflightLimit = Nothing());

    void InitService(grpc::ServerCompletionQueue* cq, NYdbGrpc::TLoggerPtr logger) override;
    void SetGlobalLimiterHandle(NYdbGrpc::TGlobalLimiter* limiter) override final;
    bool IncRequest() override final;
    void DecRequest() override final;
    void StopService() noexcept override;

    using NYdbGrpc::TGrpcServiceBase<Ydb::PersQueue::V1::ClusterDiscoveryService>::GetService;
private:
    void SetupIncomingRequests(NYdbGrpc::TLoggerPtr logger);

    NActors::TActorSystem* ActorSystem_;
    grpc::ServerCompletionQueue* CQ_ = nullptr;

    TIntrusivePtr<::NMonitoring::TDynamicCounters> Counters_;
    NActors::TActorId GRpcRequestProxyId_;
    THolder<NYdbGrpc::TGlobalLimiter> Limiter;
};

}
