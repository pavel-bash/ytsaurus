#include "functions.h"

#include <yt/yt/client/table_client/logical_type.h>

#include <library/cpp/yt/misc/variant.h>

namespace NYT::NQueryClient {

////////////////////////////////////////////////////////////////////////////////

TFunctionTypeInferrer::TFunctionTypeInferrer(
    std::unordered_map<TTypeParameter, TUnionType> typeParameterConstraints,
    std::vector<TType> argumentTypes,
    TType repeatedArgumentType,
    TType resultType)
    : TypeParameterConstraints_(std::move(typeParameterConstraints))
    , ArgumentTypes_(std::move(argumentTypes))
    , RepeatedArgumentType_(repeatedArgumentType)
    , ResultType_(resultType)
{ }

TFunctionTypeInferrer::TFunctionTypeInferrer(
    std::unordered_map<TTypeParameter, TUnionType> typeParameterConstraints,
    std::vector<TType> argumentTypes,
    TType resultType)
    : TFunctionTypeInferrer(
        std::move(typeParameterConstraints),
        std::move(argumentTypes),
        EValueType::Null,
        resultType)
{ }

TFunctionTypeInferrer::TFunctionTypeInferrer(
    std::vector<TType> argumentTypes,
    TType resultType)
    : TFunctionTypeInferrer(
        std::unordered_map<TTypeParameter, TUnionType>(),
        std::move(argumentTypes),
        resultType)
{ }

int TFunctionTypeInferrer::GetNormalizedConstraints(
    std::vector<TTypeSet>* typeConstraints,
    std::vector<int>* formalArguments,
    std::optional<std::pair<int, bool>>* repeatedType) const
{
    std::unordered_map<TTypeParameter, int> idToIndex;

    auto getIndex = [&] (const TType& type) -> int {
        return Visit(type,
            [&] (TTypeParameter genericId) -> int {
                auto itIndex = idToIndex.find(genericId);
                if (itIndex != idToIndex.end()) {
                    return itIndex->second;
                } else {
                    int index = typeConstraints->size();
                    auto it = TypeParameterConstraints_.find(genericId);
                    if (it == TypeParameterConstraints_.end()) {
                        typeConstraints->push_back(TTypeSet({
                            EValueType::Null,
                            EValueType::Int64,
                            EValueType::Uint64,
                            EValueType::Double,
                            EValueType::Boolean,
                            EValueType::String,
                            EValueType::Any,
                        }));
                    } else {
                        typeConstraints->push_back(TTypeSet(it->second.begin(), it->second.end()));
                    }
                    idToIndex.emplace(genericId, index);
                    return index;
                }
            },
            [&] (EValueType fixedType) -> int {
                int index = typeConstraints->size();
                typeConstraints->push_back(TTypeSet({fixedType}));
                return index;
            },
            [&] (const TUnionType& unionType) -> int {
                int index = typeConstraints->size();
                typeConstraints->push_back(TTypeSet(unionType.begin(), unionType.end()));
                return index;
            });
    };

    for (const auto& argumentType : ArgumentTypes_) {
        formalArguments->push_back(getIndex(argumentType));
    }

    if (!(std::holds_alternative<EValueType>(RepeatedArgumentType_) &&
        std::get<EValueType>(RepeatedArgumentType_) == EValueType::Null))
    {
        *repeatedType = std::pair(
            getIndex(RepeatedArgumentType_),
            std::get_if<TUnionType>(&RepeatedArgumentType_));
    }

    return getIndex(ResultType_);
}

std::vector<TTypeId> TFunctionTypeInferrer::InferTypes(TTypingCtx* typingCtx, TRange<TLogicalTypePtr> argumentTypes, TStringBuf name) const
{
    std::vector<TTypeId> argumentTypeIds;
    for (auto type : argumentTypes) {
        argumentTypeIds.push_back(typingCtx->GetTypeId(GetWireType(type)));
    }

    auto signature = GetSignature(typingCtx, std::ssize(argumentTypes));

    return typingCtx->InferFunctionType(name, {signature}, argumentTypeIds);
}

TTypingCtx::TFunctionSignature TFunctionTypeInferrer::GetSignature(TTypingCtx* typingCtx, int argumentCount) const
{
    TTypingCtx::TFunctionSignature signature({});
    int nextGenericId = 0;

    auto registerConstraints = [&] (int id, const TUnionType& unionType) {
        if (id >= std::ssize(signature.Constraints)) {
            signature.Constraints.resize(id + 1);
        }
        for (auto type : unionType) {
            signature.Constraints[id].push_back(typingCtx->GetTypeId(type));
        }
    };

    Visit(ResultType_,
        [&] (TTypeParameter genericId) {
            signature.Types.push_back(-(1 + genericId));
            nextGenericId = std::min(nextGenericId, genericId + 1);
        },
        [&] (EValueType fixedType) {
            signature.Types.push_back(typingCtx->GetTypeId(fixedType));
        },
        [&] (const TUnionType& /*unionType*/) {
            THROW_ERROR_EXCEPTION("Result type cannot be union");
        });

    for (const auto& formalArgument : ArgumentTypes_) {
        Visit(formalArgument,
            [&] (TTypeParameter genericId) {
                signature.Types.push_back(-(1 + genericId));
                nextGenericId = std::min(nextGenericId, genericId + 1);
            },
            [&] (EValueType fixedType) {
                signature.Types.push_back(typingCtx->GetTypeId(fixedType));
            },
            [&] (const TUnionType& unionType) {
                signature.Types.push_back(-(1 + nextGenericId));
                registerConstraints(nextGenericId, unionType);
                ++nextGenericId;
            });
    }

    if (!(std::holds_alternative<EValueType>(RepeatedArgumentType_) &&
        std::get<EValueType>(RepeatedArgumentType_) == EValueType::Null))
    {
        Visit(RepeatedArgumentType_,
            [&] (TTypeParameter genericId) {
                for (int i = std::ssize(ArgumentTypes_); i < argumentCount; ++i) {
                    signature.Types.push_back(-(1 + genericId));
                }
                nextGenericId = std::min(nextGenericId, genericId + 1);
            },
            [&] (EValueType fixedType) {
                for (int i = std::ssize(ArgumentTypes_); i < argumentCount; ++i) {
                    signature.Types.push_back(typingCtx->GetTypeId(fixedType));
                }
            },
            [&] (const TUnionType& unionType) {
                for (int i = std::ssize(ArgumentTypes_); i < argumentCount; ++i) {
                    signature.Types.push_back(-(1 + nextGenericId));
                    registerConstraints(nextGenericId, unionType);
                    ++nextGenericId;
                }
            });
    }

    for (const auto& [parameterId, unionType] : TypeParameterConstraints_) {
        registerConstraints(parameterId, unionType);
    }

    return signature;
}

////////////////////////////////////////////////////////////////////////////////

TAggregateFunctionTypeInferrer::TAggregateFunctionTypeInferrer(
    std::unordered_map<TTypeParameter, TUnionType> typeParameterConstraints,
    std::vector<TType> argumentTypes,
    TType stateType,
    TType resultType)
    : TypeParameterConstraints_(std::move(typeParameterConstraints))
    , ArgumentTypes_(std::move(argumentTypes))
    , StateType_(stateType)
    , ResultType_(resultType)
{ }

std::pair<int, int> TAggregateFunctionTypeInferrer::GetNormalizedConstraints(
    std::vector<TTypeSet>* typeConstraints,
    std::vector<int>* argumentConstraintIndexes) const
{
    std::unordered_map<TTypeParameter, int> idToIndex;

    auto getIndex = [&] (const TType& type) -> int {
        return Visit(type,
            [&] (EValueType fixedType) -> int {
                typeConstraints->push_back(TTypeSet({fixedType}));
                return typeConstraints->size() - 1;
            },
            [&] (TTypeParameter genericId) -> int {
                auto itIndex = idToIndex.find(genericId);
                if (itIndex != idToIndex.end()) {
                    return itIndex->second;
                } else {
                    int index = typeConstraints->size();
                    auto it = TypeParameterConstraints_.find(genericId);
                    if (it == TypeParameterConstraints_.end()) {
                        typeConstraints->push_back(TTypeSet({
                            EValueType::Null,
                            EValueType::Int64,
                            EValueType::Uint64,
                            EValueType::Double,
                            EValueType::Boolean,
                            EValueType::String,
                            EValueType::Any,
                        }));
                    } else {
                        typeConstraints->push_back(TTypeSet(it->second.begin(), it->second.end()));
                    }
                    idToIndex.emplace(genericId, index);
                    return index;
                }
            },
            [&] (const TUnionType& unionType) -> int {
                typeConstraints->push_back(TTypeSet(unionType.begin(), unionType.end()));
                return typeConstraints->size() - 1;
            });
    };

    for (const auto& argumentType : ArgumentTypes_) {
        argumentConstraintIndexes->push_back(getIndex(argumentType));
    }

    return std::pair(getIndex(StateType_), getIndex(ResultType_));
}

std::vector<TTypeId> TAggregateFunctionTypeInferrer::InferTypes(TTypingCtx* typingCtx, TRange<TLogicalTypePtr> argumentTypes, TStringBuf name) const
{
    std::vector<TTypeId> argumentTypeIds;
    for (auto type : argumentTypes) {
        argumentTypeIds.push_back(typingCtx->GetTypeId(GetWireType(type)));
    }

    auto signature = GetSignature(typingCtx);

    // TODO: Argument types and additional types (result, state)
    // Return two vectors?
    return typingCtx->InferFunctionType(name, {signature}, argumentTypeIds, 2);
}

TTypingCtx::TFunctionSignature TAggregateFunctionTypeInferrer::GetSignature(TTypingCtx* typingCtx) const
{
    TTypingCtx::TFunctionSignature signature({});
    int nextGenericId = 0;

    auto registerConstraints = [&] (int id, const TUnionType& unionType) {
        if (id >= std::ssize(signature.Constraints)) {
            signature.Constraints.resize(id + 1);
        }
        for (auto type : unionType) {
            signature.Constraints[id].push_back(typingCtx->GetTypeId(type));
        }
    };

    Visit(ResultType_,
        [&] (TTypeParameter genericId) {
            signature.Types.push_back(-(1 + genericId));
            nextGenericId = std::min(nextGenericId, genericId + 1);
        },
        [&] (EValueType fixedType) {
            signature.Types.push_back(typingCtx->GetTypeId(fixedType));
        },
        [&] (const TUnionType& /*unionType*/) {
            THROW_ERROR_EXCEPTION("Result type cannot be union");
        });

    Visit(StateType_,
        [&] (TTypeParameter genericId) {
            signature.Types.push_back(-(1 + genericId));
            nextGenericId = std::min(nextGenericId, genericId + 1);
        },
        [&] (EValueType fixedType) {
            signature.Types.push_back(typingCtx->GetTypeId(fixedType));
        },
        [&] (const TUnionType& /*unionType*/) {
            THROW_ERROR_EXCEPTION("State type cannot be union");
        });

    for (const auto& formalArgument : ArgumentTypes_) {
        Visit(formalArgument,
            [&] (TTypeParameter genericId) {
                signature.Types.push_back(-(1 + genericId));
                nextGenericId = std::min(nextGenericId, genericId + 1);
            },
            [&] (EValueType fixedType) {
                signature.Types.push_back(typingCtx->GetTypeId(fixedType));
            },
            [&] (const TUnionType& unionType) {
                signature.Types.push_back(-(1 + nextGenericId));
                registerConstraints(nextGenericId, unionType);
                ++nextGenericId;
            });
    }

    for (const auto& [parameterId, unionType] : TypeParameterConstraints_) {
        registerConstraints(parameterId, unionType);
    }

    return signature;
}

std::vector<TTypeId> TArrayAggTypeInferrer::InferTypes(TTypingCtx* typingCtx, TRange<TLogicalTypePtr> argumentTypes, TStringBuf /*name*/) const
{
    std::vector<TTypeId> argumentTypeIds;
    for (auto type : argumentTypes) {
        argumentTypeIds.push_back(typingCtx->GetTypeId(type));
    }

    YT_VERIFY(argumentTypes.size() > 0);
    auto argumentType = argumentTypes[0];

    auto resultType = ListLogicalType(argumentType);

    return {typingCtx->GetTypeId(resultType), typingCtx->GetTypeId(EValueType::String), argumentTypeIds[0], argumentTypeIds[1]};
}

////////////////////////////////////////////////////////////////////////////////

const ITypeInferrerPtr& TTypeInferrerMap::GetFunction(const std::string& functionName) const
{
    auto found = this->find(functionName);
    if (found == this->end()) {
        THROW_ERROR_EXCEPTION("Undefined function %Qv",
            functionName);
    }
    return found->second;
}

////////////////////////////////////////////////////////////////////////////////

bool IsUserCastFunction(const std::string& name)
{
    return name == "int64" || name == "uint64" || name == "double";
}

////////////////////////////////////////////////////////////////////////////////

} // namespace NYT::NQueryClient
