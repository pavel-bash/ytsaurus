LIBRARY()

PEERDIR(
    contrib/libs/apache/arrow
    contrib/ydb/library/actors/util
    contrib/ydb/library/formats/arrow
    contrib/ydb/library/formats/arrow/hash
    contrib/ydb/library/mkql_proto
    contrib/ydb/library/yql/dq/actors/protos
    contrib/ydb/library/yql/dq/common
    contrib/ydb/library/yql/dq/expr_nodes
    contrib/ydb/library/yql/dq/type_ann
    contrib/ydb/library/yverify_stream
    yql/essentials/minikql
    yql/essentials/minikql/arrow
    yql/essentials/minikql/comp_nodes
    yql/essentials/minikql/computation
    yql/essentials/parser/pg_wrapper/interface
    yql/essentials/providers/common/comp_nodes
    yql/essentials/providers/common/schema/mkql
    yql/essentials/public/udf
)

SRCS(
    dq_arrow_helpers.cpp
    dq_async_input.cpp
    dq_async_output.cpp
    dq_columns_resolve.cpp
    dq_compute.cpp
    dq_input_channel.cpp
    dq_input_producer.cpp
    dq_output_channel.cpp
    dq_output_consumer.cpp
    dq_tasks_counters.cpp
    dq_tasks_runner.cpp
    dq_transport.cpp
)

GENERATE_ENUM_SERIALIZATION(dq_tasks_runner.h)

YQL_LAST_ABI_VERSION()

END()

RECURSE_FOR_TESTS(
    ut
)
