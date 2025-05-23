IF (CLANG AND NOT WITH_VALGRIND)
YQL_UDF_CONTRIB(clickhouse_client_udf)

    YQL_LAST_ABI_VERSION()

    NO_COMPILER_WARNINGS()

    SRCS(
        clickhouse_client_udf.cpp

        base/common/DateLUT.cpp
        base/common/DateLUTImpl.cpp
        base/common/demangle.cpp
        base/common/errnoToString.cpp
        base/common/getFQDNOrHostName.cpp
        base/common/getPageSize.cpp
        base/common/getThreadId.cpp
        base/common/JSON.cpp
        base/common/mremap.cpp
        base/common/shift10.cpp
        base/common/sleep.cpp
        base/common/StringRef.cpp
        base/common/getResource.cpp
        base/common/preciseExp10.cpp

        src/Common/AlignedBuffer.cpp
        src/Common/Allocator.cpp
        src/Common/checkStackSize.cpp
        src/Common/createHardLink.cpp
        src/Common/CurrentMetrics.cpp
        src/Common/Epoll.cpp
        src/Common/escapeForFileName.cpp
        src/Common/formatIPv6.cpp
        src/Common/formatReadable.cpp
        src/Common/getMultipleKeysFromConfig.cpp
        src/Common/getNumberOfPhysicalCPUCores.cpp
        src/Common/hasLinuxCapability.cpp
        src/Common/hex.cpp
        src/Common/isLocalAddress.cpp
        src/Common/IntervalKind.cpp
        src/Common/parseAddress.cpp
        src/Common/ClickHouseRevision.cpp
        src/Common/CurrentMemoryTracker.cpp
        src/Common/CurrentThread.cpp
        src/Common/DNSResolver.cpp
        src/Common/Exception.cpp
        src/Common/ErrorCodes.cpp
        src/Common/FieldVisitorDump.cpp
        src/Common/FieldVisitorToString.cpp
        src/Common/FieldVisitorWriteBinary.cpp
        src/Common/IPv6ToBinary.cpp
        src/Common/MemoryTracker.cpp
        src/Common/OpenSSLHelpers.cpp
        src/Common/PipeFDs.cpp
        src/Common/PODArray.cpp
        src/Common/ProcfsMetricsProvider.cpp
        src/Common/ProfileEvents.cpp
        src/Common/quoteString.cpp
        src/Common/randomSeed.cpp
        src/Common/RemoteHostFilter.cpp
        src/Common/setThreadName.cpp
        src/Common/TaskStatsInfoGetter.cpp
        src/Common/ThreadPool.cpp
        src/Common/ThreadProfileEvents.cpp
        src/Common/ThreadStatus.cpp
        src/Common/Throttler.cpp
        src/Common/TimerDescriptor.cpp
        src/Common/thread_local_rng.cpp
        src/Common/ZooKeeper/IKeeper.cpp

        src/Common/Config/AbstractConfigurationComparison.cpp

        src/Core/BaseSettings.cpp
        src/Core/Block.cpp
        src/Core/BlockInfo.cpp
        src/Core/Field.cpp
        src/Core/ColumnWithTypeAndName.cpp
        src/Core/NamesAndTypes.cpp
        src/Core/Settings.cpp
        src/Core/SettingsEnums.cpp
        src/Core/SettingsFields.cpp

        src/Formats/FormatFactory.cpp
        src/Formats/JSONEachRowUtils.cpp
        src/Formats/NativeFormat.cpp
        src/Formats/ProtobufReader.cpp
        src/Formats/ProtobufWriter.cpp
        src/Formats/registerFormats.cpp
        src/Formats/verbosePrintString.cpp

        src/AggregateFunctions/AggregateFunctionFactory.cpp
        src/AggregateFunctions/AggregateFunctionCombinatorFactory.cpp
        src/AggregateFunctions/IAggregateFunction.cpp

        #src/Columns/Collator.cpp
        src/Columns/ColumnAggregateFunction.cpp
        src/Columns/ColumnArray.cpp
        src/Columns/ColumnCompressed.cpp
        src/Columns/ColumnConst.cpp
        src/Columns/ColumnFunction.cpp
        src/Columns/ColumnNullable.cpp
        src/Columns/ColumnsCommon.cpp
        src/Columns/ColumnString.cpp
        src/Columns/ColumnTuple.cpp
        src/Columns/ColumnVector.cpp
        src/Columns/ColumnDecimal.cpp
        src/Columns/ColumnFixedString.cpp
        src/Columns/ColumnLowCardinality.cpp
        src/Columns/ColumnMap.cpp
        src/Columns/FilterDescription.cpp
        src/Columns/IColumn.cpp
        src/Columns/MaskOperations.cpp

        src/IO/AsynchronousReadBufferFromFile.cpp
        src/IO/AsynchronousReadBufferFromFileDescriptor.cpp
        src/IO/CompressionMethod.cpp
        src/IO/copyData.cpp
        src/IO/createReadBufferFromFileBase.cpp
        src/IO/DoubleConverter.cpp
        src/IO/MMappedFile.cpp
        src/IO/MMappedFileDescriptor.cpp
        src/IO/MMapReadBufferFromFile.cpp
        src/IO/MMapReadBufferFromFileDescriptor.cpp
        src/IO/MMapReadBufferFromFileWithCache.cpp
        src/IO/OpenedFile.cpp
        src/IO/parseDateTimeBestEffort.cpp
        src/IO/PeekableReadBuffer.cpp
        src/IO/Progress.cpp
        src/IO/ReadBufferFromFile.cpp
        src/IO/ReadBufferFromFileBase.cpp
        src/IO/ReadBufferFromFileDescriptor.cpp
        src/IO/ReadBufferFromMemory.cpp
        src/IO/ReadBufferFromPocoSocket.cpp
        src/IO/readFloatText.cpp
        src/IO/ReadHelpers.cpp
        src/IO/ReadSettings.cpp
        src/IO/SynchronousReader.cpp
        src/IO/TimeoutSetter.cpp
        src/IO/ThreadPoolReader.cpp
        src/IO/UseSSL.cpp
        src/IO/WriteHelpers.cpp
        src/IO/WriteBufferFromFile.cpp
        src/IO/WriteBufferFromFileBase.cpp
        src/IO/WriteBufferFromFileDescriptor.cpp
        src/IO/WriteBufferFromFileDescriptorDiscardOnFailure.cpp
        src/IO/WriteBufferFromPocoSocket.cpp
        src/IO/WriteBufferValidUTF8.cpp

        src/Compression/CompressionCodecLZ4.cpp
        src/Compression/CompressionCodecMultiple.cpp
        src/Compression/CompressionCodecNone.cpp
        src/Compression/CompressionFactory.cpp
        src/Compression/CompressedReadBuffer.cpp
        src/Compression/CompressedReadBufferBase.cpp
        src/Compression/CompressedReadBufferFromFile.cpp
        src/Compression/CompressedWriteBuffer.cpp
        src/Compression/ICompressionCodec.cpp
        src/Compression/LZ4_decompress_faster.cpp

        src/DataStreams/BlockStreamProfileInfo.cpp
        src/DataStreams/ColumnGathererStream.cpp
        src/DataStreams/ExecutionSpeedLimits.cpp
        src/DataStreams/IBlockInputStream.cpp
        src/DataStreams/materializeBlock.cpp
        src/DataStreams/NativeBlockInputStream.cpp
        src/DataStreams/NativeBlockOutputStream.cpp
        src/DataStreams/SizeLimits.cpp

        src/DataTypes/DataTypeArray.cpp
        src/DataTypes/DataTypeDate.cpp
        src/DataTypes/DataTypeDateTime.cpp
        src/DataTypes/DataTypeEnum.cpp
        src/DataTypes/DataTypeFactory.cpp
        src/DataTypes/DataTypeFunction.cpp
        src/DataTypes/DataTypeNested.cpp
        src/DataTypes/DataTypeNothing.cpp
        src/DataTypes/DataTypeNullable.cpp
        src/DataTypes/DataTypeNumberBase.cpp
        src/DataTypes/DataTypesNumber.cpp
        src/DataTypes/DataTypeString.cpp
        src/DataTypes/DataTypeTuple.cpp
        src/DataTypes/DataTypeUUID.cpp
        src/DataTypes/DataTypesDecimal.cpp
        src/DataTypes/DataTypeDecimalBase.cpp
        src/DataTypes/DataTypeLowCardinality.cpp
        src/DataTypes/DataTypeMap.cpp
        src/DataTypes/DataTypeInterval.cpp
        src/DataTypes/DataTypeDate32.cpp
        src/DataTypes/DataTypeFixedString.cpp
        src/DataTypes/DataTypeDateTime64.cpp
        src/DataTypes/DataTypeAggregateFunction.cpp
        src/DataTypes/DataTypeCustomGeo.cpp
        src/DataTypes/DataTypeCustomIPv4AndIPv6.cpp
        src/DataTypes/DataTypeCustomSimpleAggregateFunction.cpp
        src/DataTypes/DataTypeLowCardinalityHelpers.cpp
        src/DataTypes/EnumValues.cpp
        src/DataTypes/IDataType.cpp
        src/DataTypes/getLeastSupertype.cpp
        src/DataTypes/NestedUtils.cpp
        src/DataTypes/registerDataTypeDateTime.cpp

        src/DataTypes/Serializations/ISerialization.cpp
        src/DataTypes/Serializations/SerializationArray.cpp
        src/DataTypes/Serializations/SerializationDate.cpp
        src/DataTypes/Serializations/SerializationDateTime.cpp
        src/DataTypes/Serializations/SerializationEnum.cpp
        src/DataTypes/Serializations/SerializationNothing.cpp
        src/DataTypes/Serializations/SerializationNullable.cpp
        src/DataTypes/Serializations/SerializationNumber.cpp
        src/DataTypes/Serializations/SerializationString.cpp
        src/DataTypes/Serializations/SerializationTuple.cpp
        src/DataTypes/Serializations/SerializationTupleElement.cpp
        src/DataTypes/Serializations/SerializationUUID.cpp
        src/DataTypes/Serializations/SerializationWrapper.cpp
        src/DataTypes/Serializations/SerializationDecimal.cpp
        src/DataTypes/Serializations/SerializationDecimalBase.cpp
        src/DataTypes/Serializations/SerializationMap.cpp
        src/DataTypes/Serializations/SerializationLowCardinality.cpp
        src/DataTypes/Serializations/SerializationDate32.cpp
        src/DataTypes/Serializations/SerializationFixedString.cpp
        src/DataTypes/Serializations/SerializationDateTime64.cpp
        src/DataTypes/Serializations/SerializationIP.cpp
        src/DataTypes/Serializations/SerializationAggregateFunction.cpp
        src/DataTypes/Serializations/SerializationCustomSimpleText.cpp

        src/Parsers/ASTAlterQuery.cpp
        src/Parsers/ASTAsterisk.cpp
        src/Parsers/ASTBackupQuery.cpp
        src/Parsers/ASTColumnDeclaration.cpp
        src/Parsers/ASTColumnsMatcher.cpp
        src/Parsers/ASTColumnsTransformers.cpp
        src/Parsers/ASTConstraintDeclaration.cpp
        src/Parsers/ASTCreateQuery.cpp
        src/Parsers/ASTDatabaseOrNone.cpp
        src/Parsers/ASTDictionary.cpp
        src/Parsers/ASTDictionaryAttributeDeclaration.cpp
        src/Parsers/ASTDropQuery.cpp
        src/Parsers/ASTExpressionList.cpp
        src/Parsers/ASTFunction.cpp
        src/Parsers/ASTFunctionWithKeyValueArguments.cpp
        src/Parsers/ASTIdentifier.cpp
        src/Parsers/ASTIndexDeclaration.cpp
        src/Parsers/ASTKillQueryQuery.cpp
        src/Parsers/ASTLiteral.cpp
        src/Parsers/ASTNameTypePair.cpp
        src/Parsers/ASTOptimizeQuery.cpp
        src/Parsers/ASTOrderByElement.cpp
        src/Parsers/ASTPartition.cpp
        src/Parsers/ASTProjectionDeclaration.cpp
        src/Parsers/ASTProjectionSelectQuery.cpp
        src/Parsers/ASTQualifiedAsterisk.cpp
        src/Parsers/ASTQueryWithOnCluster.cpp
        src/Parsers/ASTQueryWithOutput.cpp
        src/Parsers/ASTQueryWithTableAndOutput.cpp
        src/Parsers/ASTRolesOrUsersSet.cpp
        src/Parsers/ASTSelectQuery.cpp
        src/Parsers/ASTSelectWithUnionQuery.cpp
        src/Parsers/ASTSetQuery.cpp
        src/Parsers/ASTSetRoleQuery.cpp
        src/Parsers/ASTSettingsProfileElement.cpp
        src/Parsers/ASTShowGrantsQuery.cpp
        src/Parsers/ASTShowTablesQuery.cpp
        src/Parsers/ASTSubquery.cpp
        src/Parsers/ASTTablesInSelectQuery.cpp
        src/Parsers/ASTTTLElement.cpp
        src/Parsers/ASTWindowDefinition.cpp
        src/Parsers/ASTWithAlias.cpp
        src/Parsers/ASTQueryParameter.cpp
        src/Parsers/ASTInsertQuery.cpp
        src/Parsers/ASTWithElement.cpp
        src/Parsers/ASTSampleRatio.cpp
        src/Parsers/ASTSystemQuery.cpp
        src/Parsers/ASTUserNameWithHost.cpp
        src/Parsers/CommonParsers.cpp
        src/Parsers/ExpressionElementParsers.cpp
        src/Parsers/ExpressionListParsers.cpp
        src/Parsers/formatAST.cpp
        src/Parsers/formatSettingName.cpp
        src/Parsers/IAST.cpp
        src/Parsers/InsertQuerySettingsPushDownVisitor.cpp
        src/Parsers/IParserBase.cpp
        src/Parsers/Lexer.cpp
        src/Parsers/parseDatabaseAndTableName.cpp
        src/Parsers/parseIdentifierOrStringLiteral.cpp
        src/Parsers/parseIntervalKind.cpp
        src/Parsers/parseQuery.cpp
        src/Parsers/parseUserName.cpp
        src/Parsers/ParserAlterQuery.cpp
        src/Parsers/ParserBackupQuery.cpp
        src/Parsers/ParserCase.cpp
        src/Parsers/ParserCheckQuery.cpp
        src/Parsers/ParserCreateQuery.cpp
        src/Parsers/ParserDatabaseOrNone.cpp
        src/Parsers/ParserDataType.cpp
        src/Parsers/ParserDescribeTableQuery.cpp
        src/Parsers/ParserDictionary.cpp
        src/Parsers/ParserDictionaryAttributeDeclaration.cpp
        src/Parsers/ParserDropQuery.cpp
        src/Parsers/ParserExplainQuery.cpp
        src/Parsers/ParserExternalDDLQuery.cpp
        src/Parsers/ParserInsertQuery.cpp
        src/Parsers/ParserKillQueryQuery.cpp
        src/Parsers/ParserOptimizeQuery.cpp
        src/Parsers/ParserPartition.cpp
        src/Parsers/ParserProjectionSelectQuery.cpp
        src/Parsers/ParserQuery.cpp
        src/Parsers/ParserRenameQuery.cpp
        src/Parsers/ParserRolesOrUsersSet.cpp
        src/Parsers/ParserSelectWithUnionQuery.cpp
        src/Parsers/ParserSetQuery.cpp
        src/Parsers/ParserSetRoleQuery.cpp
        src/Parsers/ParserSettingsProfileElement.cpp
        src/Parsers/ParserSelectQuery.cpp
        src/Parsers/ParserTablePropertiesQuery.cpp
        src/Parsers/ParserTablesInSelectQuery.cpp
        src/Parsers/ParserSampleRatio.cpp
        src/Parsers/ParserShowGrantsQuery.cpp
        src/Parsers/ParserShowPrivilegesQuery.cpp
        src/Parsers/ParserShowTablesQuery.cpp
        src/Parsers/ParserSystemQuery.cpp
        src/Parsers/ParserUnionQueryElement.cpp
        src/Parsers/ParserUseQuery.cpp
        src/Parsers/ParserUserNameWithHost.cpp
        src/Parsers/ParserWatchQuery.cpp
        src/Parsers/ParserWithElement.cpp
        src/Parsers/queryToString.cpp
        src/Parsers/QueryWithOutputSettingsPushDownVisitor.cpp
        src/Parsers/TokenIterator.cpp

        src/Processors/Chunk.cpp
        src/Processors/ConcatProcessor.cpp
        src/Processors/IAccumulatingTransform.cpp
        src/Processors/IProcessor.cpp
        src/Processors/ISimpleTransform.cpp
        src/Processors/ISink.cpp
        src/Processors/LimitTransform.cpp
        src/Processors/ISource.cpp
        src/Processors/Port.cpp
        src/Processors/ResizeProcessor.cpp

        src/Processors/Formats/IRowOutputFormat.cpp
        src/Processors/Formats/IRowInputFormat.cpp
        src/Processors/Formats/IInputFormat.cpp
        src/Processors/Formats/IOutputFormat.cpp
        src/Processors/Formats/OutputStreamToOutputFormat.cpp
        src/Processors/Formats/RowInputFormatWithDiagnosticInfo.cpp

        src/Interpreters/castColumn.cpp
        src/Interpreters/ClientInfo.cpp
        src/Interpreters/InternalTextLogsQueue.cpp
        src/Interpreters/QueryLog.cpp
        src/Interpreters/QueryThreadLog.cpp
        src/Interpreters/ProfileEventsExt.cpp
        src/Interpreters/TablesStatus.cpp

        src/Functions/CastOverloadResolver.cpp
        src/Functions/FunctionHelpers.cpp
        src/Functions/FunctionsConversion.cpp
        src/Functions/IFunction.cpp
        src/Functions/FunctionFactory.cpp
        src/Functions/extractTimeZoneFromFunctionArguments.cpp
        src/Functions/toFixedString.cpp

        src/Processors/Executors/PollingQueue.cpp

        src/Processors/Formats/Impl/ArrowBlockInputFormat.cpp
        src/Processors/Formats/Impl/ArrowBufferedStreams.cpp
        src/Processors/Formats/Impl/ArrowColumnToCHColumn.cpp
        src/Processors/Formats/Impl/AvroRowInputFormat.cpp
        src/Processors/Formats/Impl/CHColumnToArrowColumn.cpp
        src/Processors/Formats/Impl/CSVRowInputFormat.cpp
        src/Processors/Formats/Impl/CSVRowOutputFormat.cpp
        src/Processors/Formats/Impl/JSONAsStringRowInputFormat.cpp
        src/Processors/Formats/Impl/JSONEachRowRowInputFormat.cpp
        src/Processors/Formats/Impl/JSONEachRowRowOutputFormat.cpp
        src/Processors/Formats/Impl/ORCBlockInputFormat.cpp
        src/Processors/Formats/Impl/ParquetBlockInputFormat.cpp
        src/Processors/Formats/Impl/ParquetBlockOutputFormat.cpp
        src/Processors/Formats/Impl/RawBLOBRowInputFormat.cpp
        src/Processors/Formats/Impl/TabSeparatedRowInputFormat.cpp
        src/Processors/Formats/Impl/TabSeparatedRowOutputFormat.cpp
        src/Processors/Formats/Impl/TSKVRowInputFormat.cpp
        src/Processors/Formats/Impl/TSKVRowOutputFormat.cpp
    )

    PEERDIR(
        contrib/libs/cctz
        contrib/restricted/boost/multi_index
        contrib/restricted/boost/program_options
        contrib/restricted/cityhash-1.0.2
        contrib/restricted/fast_float
        #contrib/libs/icu
        contrib/libs/pdqsort
        contrib/libs/lz4
        contrib/restricted/dragonbox
        contrib/libs/poco/Util
        contrib/libs/poco/Net
        contrib/libs/poco/NetSSL_OpenSSL
        contrib/libs/fmt
        contrib/libs/re2
        contrib/libs/apache/arrow
        contrib/libs/apache/orc
        contrib/libs/apache/avro
        library/cpp/sanitizer/include
        yql/essentials/minikql/dom
        yql/essentials/utils
    )

    ADDINCL(
        GLOBAL contrib/restricted/dragonbox
        contrib/restricted/fast_float/include
        #contrib/libs/icu/common
        #contrib/libs/icu/i18n
        contrib/libs/pdqsort
        contrib/libs/lz4
        contrib/libs/apache/arrow/src
        contrib/libs/apache/avro/include
        contrib/libs/apache/orc/c++/include
        contrib/ydb/library/yql/udfs/common/clickhouse/client/base
        contrib/ydb/library/yql/udfs/common/clickhouse/client/base/pcg-random
        contrib/ydb/library/yql/udfs/common/clickhouse/client/src
    )

    CFLAGS (
       -DARCADIA_BUILD -DUSE_ARROW=0 -DUSE_PARQUET=1 -DUSE_SNAPPY=1 -DUSE_ORC=0 -DUSE_AVRO=0 -DUSE_UNWIND=0 -DDBMS_VERSION_MAJOR=21 -DDBMS_VERSION_MINOR=18 -DDBMS_VERSION_PATCH=0
       -Wno-unused-parameter
    )

    IF (OS_DARWIN)
        CFLAGS(
            GLOBAL -DOS_DARWIN
        )
    ELSEIF (OS_LINUX)
        CFLAGS(
            GLOBAL -DOS_LINUX
        )
    ENDIF()


    END()
ELSE()
    LIBRARY()
    END()
ENDIF()

RECURSE_FOR_TESTS(
    test
)