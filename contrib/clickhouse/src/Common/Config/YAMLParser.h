#pragma once

#include "clickhouse_config.h"

#include <Common/ErrorCodes.h>
#include <Common/Exception.h>
#include <base/types.h>
#include <DBPoco/DOM/Document.h>
#include <DBPoco/DOM/AutoPtr.h>

#if USE_YAML_CPP

namespace DB
{

/// Real YAML parser: loads yaml file into a YAML::Node
class YAMLParserImpl
{
public:
    static DBPoco::AutoPtr<DBPoco::XML::Document> parse(const String& path);
};

using YAMLParser = YAMLParserImpl;

}

#else

namespace DB
{

namespace ErrorCodes
{
    extern const int CANNOT_PARSE_YAML;
}

/// Fake YAML parser: throws an exception if we try to parse YAML configs in a build without yaml-cpp
class DummyYAMLParser
{
public:
    static DBPoco::AutoPtr<DBPoco::XML::Document> parse(const String& path)
    {
        DBPoco::AutoPtr<DBPoco::XML::Document> xml = new DBPoco::XML::Document;
        throw Exception(ErrorCodes::CANNOT_PARSE_YAML, "Unable to parse YAML configuration file {} without usage of yaml-cpp library", path);
        return xml;
    }
};

using YAMLParser = DummyYAMLParser;

}

#endif
