IF (NOT EXPORT_CMAKE OR NOT OPENSOURCE OR OPENSOURCE_PROJECT != "yt")

YQL_UDF_TEST()

DEPENDS(yql/essentials/udfs/language/yql)

END()

ENDIF()

