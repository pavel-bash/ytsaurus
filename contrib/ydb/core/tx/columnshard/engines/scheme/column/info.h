#pragma once
#include <contrib/ydb/core/formats/arrow/accessor/abstract/constructor.h>
#include <contrib/ydb/library/formats/arrow/validation/validation.h>
#include <contrib/ydb/core/formats/arrow/dictionary/object.h>
#include <contrib/ydb/core/formats/arrow/save_load/loader.h>
#include <contrib/ydb/core/formats/arrow/save_load/saver.h>
#include <contrib/ydb/core/formats/arrow/serializer/abstract.h>
#include <contrib/ydb/library/formats/arrow/transformer/abstract.h>
#include <contrib/ydb/core/tx/columnshard/engines/scheme/abstract/index_info.h>
#include <contrib/ydb/core/tx/columnshard/engines/scheme/defaults/common/scalar.h>

#include <contrib/ydb/library/accessor/accessor.h>

#include <contrib/libs/apache/arrow/cpp/src/arrow/array/array_base.h>
#include <contrib/libs/apache/arrow/cpp/src/arrow/type.h>

namespace NKikimr::NOlap {

class IPortionDataChunk;

class TSimpleColumnInfo {
private:
    YDB_READONLY(ui32, ColumnId, 0);
    YDB_READONLY_DEF(std::optional<ui32>, PKColumnIndex);
    YDB_READONLY_DEF(TString, ColumnName);
    YDB_READONLY_DEF(std::shared_ptr<arrow::Field>, ArrowField);
    YDB_READONLY(NArrow::NSerialization::TSerializerContainer, Serializer, NArrow::NSerialization::TSerializerContainer::GetDefaultSerializer());
    YDB_READONLY(NArrow::NAccessor::TConstructorContainer, DataAccessorConstructor, NArrow::NAccessor::TConstructorContainer::GetDefaultConstructor());
    YDB_READONLY(bool, NeedMinMax, false);
    YDB_READONLY(bool, IsSorted, false);
    YDB_READONLY(bool, IsNullable, false);
    YDB_READONLY_DEF(TColumnDefaultScalarValue, DefaultValue);
    std::shared_ptr<TColumnLoader> Loader;

public:
    TSimpleColumnInfo(const ui32 columnId, const std::shared_ptr<arrow::Field>& arrowField,
        const NArrow::NSerialization::TSerializerContainer& serializer, const bool needMinMax, const bool isSorted, const bool isNullable,
        const std::shared_ptr<arrow::Scalar>& defaultValue, const std::optional<ui32>& pkColumnIndex);

    TColumnSaver GetColumnSaver() const {
        AFL_VERIFY(Serializer);
        return TColumnSaver(Serializer);
    }

    std::vector<std::shared_ptr<IPortionDataChunk>> ActualizeColumnData(
        const std::vector<std::shared_ptr<IPortionDataChunk>>& source, const TSimpleColumnInfo& sourceColumnFeatures) const;

    TString DebugString() const {
        TStringBuilder sb;
        sb << "serializer=" << (Serializer ? Serializer->DebugString() : "NO") << ";";
        sb << "loader=" << (Loader ? Loader->DebugString() : "NO") << ";";
        return sb;
    }

    TConclusionStatus DeserializeFromProto(const NKikimrSchemeOp::TOlapColumnDescription& columnInfo);

    const std::shared_ptr<TColumnLoader>& GetLoader() const {
        AFL_VERIFY(Loader);
        return Loader;
    }
};

}   // namespace NKikimr::NOlap
