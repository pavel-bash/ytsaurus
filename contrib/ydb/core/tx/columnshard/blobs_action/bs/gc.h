#pragma once

#include "address.h"
#include "blob_manager.h"

#include <contrib/ydb/core/tx/columnshard/blob_cache.h>
#include <contrib/ydb/core/tx/columnshard/blobs_action/abstract/gc.h>
#include <contrib/ydb/core/tx/columnshard/blobs_action/counters/remove_gc.h>

namespace NKikimr::NOlap::NBlobOperations::NBlobStorage {

class TGCTask: public IBlobsGCAction {
private:
    using TBase = IBlobsGCAction;
public:
    struct TGCLists {
        THashSet<TLogoBlobID> KeepList;
        THashSet<TLogoBlobID> DontKeepList;
        mutable ui32 RequestsCount = 0;
        
        constexpr static ui32 RequestsLimit = 10;
    };
    using TGCListsByGroup = THashMap<TBlobAddress, TGCLists>;
private:
    TGCListsByGroup ListsByGroupId;
    const std::optional<TGenStep> CollectGenStepInFlight;
    const ui64 TabletId;
    const ui64 CurrentGen;
    std::deque<TUnifiedBlobId> KeepsToErase;
    std::shared_ptr<TBlobManager> Manager;
    size_t Failures = 0;
protected:
    virtual void RemoveBlobIdFromDB(const TTabletId tabletId, const TUnifiedBlobId& blobId, TBlobManagerDb& dbBlobs) override;
    virtual void DoOnExecuteTxAfterCleaning(NColumnShard::TColumnShard& self, TBlobManagerDb& dbBlobs) override;
    virtual bool DoOnCompleteTxAfterCleaning(NColumnShard::TColumnShard& self, const std::shared_ptr<IBlobsGCAction>& taskAction) override;

    virtual void DoOnExecuteTxBeforeCleaning(NColumnShard::TColumnShard& self, TBlobManagerDb& dbBlobs) override;
    virtual bool DoOnCompleteTxBeforeCleaning(NColumnShard::TColumnShard& self, const std::shared_ptr<IBlobsGCAction>& taskAction) override;

    virtual bool DoIsEmpty() const override {
        return !CollectGenStepInFlight && KeepsToErase.empty();
    }

public:
    TGCTask(const TString& storageId, TGCListsByGroup&& listsByGroupId, const std::optional<TGenStep>& collectGenStepInFlight, std::deque<TUnifiedBlobId>&& keepsToErase,
        const std::shared_ptr<TBlobManager>& manager, TBlobsCategories&& blobsToRemove, const std::shared_ptr<TRemoveGCCounters>& counters, const ui64 tabletId, const ui64 currentGen);

    const TGCListsByGroup& GetListsByGroupId() const {
        return ListsByGroupId;
    }

    ui64 GetTabletId() const {
        return TabletId;
    }

    bool IsFinished() const {
        return ListsByGroupId.empty();
    }

    bool HasFailures() const {
        return Failures != 0;
    }

    void OnGCResult(TEvBlobStorage::TEvCollectGarbageResult::TPtr ev);

    std::unique_ptr<TEvBlobStorage::TEvCollectGarbage> BuildRequest(const TBlobAddress& address) const;
};

}
