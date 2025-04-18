#pragma once

#include <Storages/MergeTree/MergeTreeIndices.h>
#include "base/types.h"

#include <optional>
#include <vector>

namespace DB
{

/// Class VectorSimilarityCondition is responsible for recognizing if the query can utilize vector similarity indexes.
/// Method alwaysUnknownOrTrue returns false if we can speed up the query, and true otherwise. It has
/// only one argument, the name of the distance function with which index was built. Two main patterns of queries are supported
///
/// - 1. WHERE queries:
///   SELECT * FROM * WHERE DistanceFunc(column, reference_vector) < floatLiteral LIMIT count
///
/// - 2. ORDER BY queries:
///   SELECT * FROM * WHERE * ORDER BY DistanceFunc(column, reference_vector) LIMIT count
///
/// Queries without LIMIT count are not supported
/// If the query is both of type 1. and 2., than we can't use the index and alwaysUnknownOrTrue returns true.
/// reference_vector should have float coordinates, e.g. [0.2, 0.1, .., 0.5]
///
/// If the query matches one of these two types, then this class extracts the main information needed for vector similarity indexes from the
/// query.
///
/// From matching query it extracts
/// - referenceVector
/// - distance function
/// - distance to compare(ONLY for search types, otherwise you get exception)
/// - spaceDimension(which is referenceVector's components count)
/// - column
/// - objects count from LIMIT clause(for both queries)
/// - queryHasOrderByClause and queryHasWhereClause return true if query matches the type
///
/// Search query type is also recognized for PREWHERE clause
class VectorSimilarityCondition
{
public:
    VectorSimilarityCondition(const SelectQueryInfo & query_info, ContextPtr context);

    /// Approximate nearest neighbour (ANN) / vector similarity queries have a similar structure:
    /// - reference vector from which all distances are calculated
    /// - distance function, e.g L2Distance
    /// - name of column with embeddings
    /// - type of query
    /// - maximum number of returned elements (LIMIT)
    ///
    /// And one optional parameter:
    /// - distance to compare with (only for where queries)
    ///
    /// This struct holds all these components.
    struct Info
    {
        enum class DistanceFunction : uint8_t
        {
            Unknown,
            L2
        };

        std::vector<Float32> reference_vector;
        DistanceFunction distance_function;
        String column_name;
        UInt64 limit;
        float distance = -1.0;
    };

    /// Returns false if query can be speeded up by an ANN index, true otherwise.
    bool alwaysUnknownOrTrue(String distance_function) const;

    std::vector<float> getReferenceVector() const;
    size_t getDimensions() const;
    String getColumnName() const;
    Info::DistanceFunction getDistanceFunction() const;
    UInt64 getIndexGranularity() const { return index_granularity; }
    UInt64 getLimit() const;

private:
    struct RPNElement
    {
        enum Function
        {
            /// DistanceFunctions
            FUNCTION_DISTANCE,

            //array(0.1, ..., 0.1)
            FUNCTION_ARRAY,

            /// Operators <, >, <=, >=
            FUNCTION_COMPARISON,

            /// Numeric float value
            FUNCTION_FLOAT_LITERAL,

            /// Numeric int value
            FUNCTION_INT_LITERAL,

            /// Column identifier
            FUNCTION_IDENTIFIER,

            /// Unknown, can be any value
            FUNCTION_UNKNOWN,

            /// [0.1, ...., 0.1] vector without word 'array'
            FUNCTION_LITERAL_ARRAY,

            /// if client parameters are used, cast will always be in the query
            FUNCTION_CAST,

            /// name of type in cast function
            FUNCTION_STRING_LITERAL,
        };

        explicit RPNElement(Function function_ = FUNCTION_UNKNOWN)
            : function(function_)
        {}

        Function function;
        String func_name = "Unknown";

        std::optional<float> float_literal;
        std::optional<String> identifier;
        std::optional<int64_t> int_literal;
        std::optional<Array> array_literal;

        UInt32 dim = 0;
    };

    using RPN = std::vector<RPNElement>;

    bool checkQueryStructure(const SelectQueryInfo & query);

    /// Util functions for the traversal of AST, parses AST and builds rpn
    void traverseAST(const ASTPtr & node, RPN & rpn);
    /// Return true if we can identify our node type
    bool traverseAtomAST(const ASTPtr & node, RPNElement & out);
    /// Checks if the AST stores ConstType expression
    bool tryCastToConstType(const ASTPtr & node, RPNElement & out);
    /// Traverses the AST of ORDERBY section
    void traverseOrderByAST(const ASTPtr & node, RPN & rpn);

    /// Returns true and stores ANNExpr if the query has valid WHERE section
    static bool matchRPNWhere(RPN & rpn, Info & info);

    /// Returns true and stores ANNExpr if the query has valid ORDERBY section
    static bool matchRPNOrderBy(RPN & rpn, Info & info);

    /// Returns true and stores Length if we have valid LIMIT clause in query
    static bool matchRPNLimit(RPNElement & rpn, UInt64 & limit);

    /// Matches dist function, reference vector, column name
    static bool matchMainParts(RPN::iterator & iter, const RPN::iterator & end, Info & info);

    /// Gets float or int from AST node
    static float getFloatOrIntLiteralOrPanic(const RPN::iterator& iter);

    Block block_with_constants;

    /// true if we have one of two supported query types
    std::optional<Info> query_information;

    // Get from settings ANNIndex parameters
    const UInt64 index_granularity;

    /// only queries with a lower limit can be considered to avoid memory overflow
    const UInt64 max_limit_for_ann_queries;

    bool index_is_useful = false;
};

}
