#pragma once

#include <contrib/ydb/library/actors/core/actorsystem.h>
#include <contrib/ydb/library/grpc/server/grpc_server.h>
#include <contrib/ydb/public/api/grpc/draft/ydb_object_storage_v1.grpc.pb.h>
#include <contrib/ydb/core/grpc_services/base/base_service.h>

namespace NKikimr {
namespace NGRpcService {

class TGRpcYdbObjectStorageService
    : public TGrpcServiceBase<Ydb::ObjectStorage::V1::ObjectStorageService>
{
public:
    using TGrpcServiceBase<Ydb::ObjectStorage::V1::ObjectStorageService>::TGrpcServiceBase;

private:
    void SetupIncomingRequests(NYdbGrpc::TLoggerPtr logger);
};

} // namespace NGRpcService
} // namespace NKikimr
