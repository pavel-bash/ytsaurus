#pragma once
#include <contrib/ydb/core/tx/columnshard/columnshard_impl.h>
#include <contrib/ydb/core/tx/columnshard/columnshard_private_events.h>

namespace NKikimr::NColumnShard {
class TTxInsertTableCleanup: public TTransactionBase<TColumnShard> {
private:
    THashSet<TInsertWriteId> WriteIdsToAbort;
    std::shared_ptr<NOlap::IBlobsDeclareRemovingAction> BlobsAction;
public:
    TTxInsertTableCleanup(TColumnShard* self, THashSet<TInsertWriteId>&& writeIdsToAbort)
        : TBase(self)
        , WriteIdsToAbort(std::move(writeIdsToAbort)) {
        Y_ABORT_UNLESS(WriteIdsToAbort.size() || self->InsertTable->GetAborted().size());
    }

    ~TTxInsertTableCleanup() {
    }

    virtual bool Execute(TTransactionContext& txc, const TActorContext& ctx) override;
    virtual void Complete(const TActorContext& ctx) override;
    TTxType GetTxType() const override { return TXTYPE_CLEANUP_INSERT_TABLE; }
};

}
