#include "change_record.h"

#include <contrib/ydb/core/change_exchange/resolve_partition.h>
#include <contrib/ydb/core/protos/change_exchange.pb.h>
#include <contrib/ydb/core/protos/tx_datashard.pb.h>

namespace NKikimr::NDataShard {

void TChangeRecord::Serialize(NKikimrChangeExchange::TChangeRecord& record) const {
    record.SetOrder(Order);
    record.SetGroup(Group);
    record.SetStep(Step);
    record.SetTxId(TxId);
    record.SetPathOwnerId(PathId.OwnerId);
    record.SetLocalPathId(PathId.LocalPathId);

    switch (Kind) {
        case EKind::AsyncIndex: {
            Y_ENSURE(record.MutableAsyncIndex()->ParseFromArray(Body.data(), Body.size()));
            break;
        }
        case EKind::IncrementalRestore: {
            Y_ENSURE(record.MutableIncrementalRestore()->ParseFromArray(Body.data(), Body.size()));
            break;
        }
        case EKind::CdcDataChange: {
            Y_ENSURE(record.MutableCdcDataChange()->ParseFromArray(Body.data(), Body.size()));
            break;
        }
        case EKind::CdcHeartbeat: {
            break;
        }
    }
}

static auto ParseBody(const TString& protoBody) {
    NKikimrChangeExchange::TDataChange body;
    Y_ENSURE(body.ParseFromArray(protoBody.data(), protoBody.size()));
    return body;
}

TConstArrayRef<TCell> TChangeRecord::GetKey() const {
    if (Key) {
        return *Key;
    }

    switch (Kind) {
        case EKind::AsyncIndex:
        case EKind::IncrementalRestore:
        case EKind::CdcDataChange: {
            const auto parsed = ParseBody(Body);

            TSerializedCellVec key;
            Y_ENSURE(TSerializedCellVec::TryParse(parsed.GetKey().GetData(), key));

            Key.ConstructInPlace(key.GetCells());
            break;
        }

        case EKind::CdcHeartbeat: {
            Y_ENSURE(false, "Not supported");
        }
    }

    Y_ENSURE(Key);
    return *Key;
}

i64 TChangeRecord::GetSeqNo() const {
    Y_ENSURE(Order <= Max<i64>());
    return static_cast<i64>(Order);
}

TInstant TChangeRecord::GetApproximateCreationDateTime() const {
    return GetGroup()
        ? TInstant::MicroSeconds(GetGroup())
        : TInstant::MilliSeconds(GetStep());
}

bool TChangeRecord::IsBroadcast() const {
    switch (Kind) {
        case EKind::CdcHeartbeat:
            return true;
        default:
            return false;
    }
}

void TChangeRecord::Accept(NChangeExchange::IVisitor& visitor) const {
    return visitor.Visit(*this);
}

void TChangeRecord::Out(IOutputStream& out) const {
    out << "{"
        << " Order: " << Order
        << " Group: " << Group
        << " Step: " << Step
        << " TxId: " << TxId
        << " PathId: " << PathId
        << " Kind: " << Kind
        << " Source: " << Source
        << " Body: " << Body.size() << "b"
        << " TableId: " << TableId
        << " SchemaVersion: " << SchemaVersion
        << " LockId: " << LockId
        << " LockOffset: " << LockOffset
    << " }";
}

class TDefaultPartitionResolver final: public NChangeExchange::TBasePartitionResolver {
public:
    TDefaultPartitionResolver(const NKikimr::TKeyDesc& keyDesc)
        : KeyDesc(keyDesc)
    {
    }

    void Visit(const TChangeRecord& record) override {
        SetPartitionId(NChangeExchange::ResolveSchemaBoundaryPartitionId(KeyDesc, record.GetKey()));
    }

private:
    const NKikimr::TKeyDesc& KeyDesc;
};

NChangeExchange::IPartitionResolverVisitor* CreateDefaultPartitionResolver(const NKikimr::TKeyDesc& keyDesc) {
    return new TDefaultPartitionResolver(keyDesc);
}

}
