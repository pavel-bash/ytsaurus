#pragma once

#include <contrib/ydb/core/tx/columnshard/columnshard_schema.h>
#include <contrib/ydb/core/tx/columnshard/normalizer/abstract/abstract.h>

namespace NKikimr::NOlap {

class TGCCountersNormalizer: public TNormalizationController::INormalizerComponent {
private:
    using TBase = TNormalizationController::INormalizerComponent;

public:
    static TString GetClassNameStatic() {
        return "GCCountersNormalizer";
    }

private:
    class TNormalizerResult;

    static const inline INormalizerComponent::TFactory::TRegistrator<TGCCountersNormalizer> Registrator =
        INormalizerComponent::TFactory::TRegistrator<TGCCountersNormalizer>(GetClassNameStatic());

public:
    TGCCountersNormalizer(const TNormalizationController::TInitContext& context)
        : TBase(context) {
    }

    virtual std::optional<ENormalizerSequentialId> DoGetEnumSequentialId() const override {
        return ENormalizerSequentialId::GCCountersNormalizer;
    }

    virtual TString GetClassName() const override {
        return GetClassNameStatic();
    }

    virtual TConclusion<std::vector<INormalizerTask::TPtr>> DoInit(
        const TNormalizationController& controller, NTabletFlatExecutor::TTransactionContext& txc) override;
};

}   // namespace NKikimr::NOlap
