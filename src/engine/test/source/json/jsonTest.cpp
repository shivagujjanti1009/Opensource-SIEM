#include "json.hpp"
#include "gtest/gtest.h"

#include <iostream>
#include <string>

#include "rapidjson/document.h"
#include "rapidjson/pointer.h"

#define GTEST_COUT std::cerr << "[          ] [ INFO ] "

using namespace std;
using namespace json;

TEST(JsonTest, InitializeDefault)
{
    ASSERT_NO_THROW(Document doc;);
    ASSERT_NO_THROW(Document doc{};);
}

TEST(JsonTest, InitializeCStr)
{
    ASSERT_NO_THROW(Document doc{R"({})"};);
}

TEST(JsonTest, InitializeValue)
{
    rapidjson::Value vInt(1);
    ASSERT_NO_THROW(Document doc{vInt};);
}

TEST(JsonTest, CopyInitialize)
{
    Document doc{R"({})"};
    ASSERT_NO_THROW(Document doc1{doc});
}

TEST(JsonTest, MoveCopyInitialize)
{
    Document doc{R"({})"};
    ASSERT_NO_THROW(Document doc1{move(doc)});
    ASSERT_NO_THROW(Document doc1{Document{}});
}

TEST(JsonTest, OperatorEqual)
{
    Document doc;
    ASSERT_NO_THROW(Document doc1 = doc);
}

TEST(JsonTest, MoveOperatorEqual)
{
    Document doc;
    ASSERT_NO_THROW(Document doc1 = Document{});
}

TEST(JsonTest, Get)
{
    Document doc{R"({"from": "value"})"};
    ASSERT_NO_THROW(doc.get("/from"));
    ASSERT_STREQ("value", doc.get("/from").GetString());
}

TEST(JsonTest, GetMultilevel)
{
    Document doc{R"({"from": {"multi": { "level": "value"}}})"};
    ASSERT_NO_THROW(doc.get("/from/multi/level"));
    ASSERT_STREQ("value", doc.get("/from/multi/level").GetString());
}

TEST(JsonTest, GetMultilevelIncorrectPath)
{
    Document doc{R"({"from": {"multi": { "level": "value"}}})"};
    ASSERT_THROW(doc.get("/from.multi.level"), std::invalid_argument);
}

TEST(JsonTest, Set)
{
    Document doc;
    string field = "/field";
    rapidjson::Document doc2;
    rapidjson::Value val("value", doc2.GetAllocator());
    ASSERT_NO_THROW(doc.set(field, val));
    ASSERT_EQ(val, doc.get(field));
    val = rapidjson::Value("other_value", doc2.GetAllocator());
    ASSERT_NE(val, doc.get(field));
}

TEST(JsonTest, SetOneLevel)
{
    Document doc;
    string field = "/field/level";
    rapidjson::Document doc2;
    rapidjson::Value val("value", doc2.GetAllocator());
    ASSERT_NO_THROW(doc.set(field, val));
    ASSERT_EQ(val, doc.get(field));
    val = rapidjson::Value("other_value", doc2.GetAllocator());
    ASSERT_NE(val, doc.get(field));
}

TEST(JsonTest, SetMultiLevel)
{
    Document doc;
    string field = "/field/multi/level";
    rapidjson::Document doc2;
    rapidjson::Value val("value", doc2.GetAllocator());
    ASSERT_NO_THROW(doc.set(field, val));
    ASSERT_EQ(val, doc.get(field));
    val = rapidjson::Value("other_value", doc2.GetAllocator());
    ASSERT_NE(val, doc.get(field));
}

TEST(JsonTest, SetReference)
{
    Document doc{R"({"from": "value"})"};
    ASSERT_NO_THROW(doc.set("/to", "/from"));
    ASSERT_STREQ(doc.get("/to").GetString(), doc.get("/from").GetString());
}

TEST(JsonTest, SetReferenceOneLevel)
{
    Document doc{R"({"from":{"level": "value"}})"};
    ASSERT_NO_THROW(doc.set("/to/level", "/from/level"));
    ASSERT_STREQ(doc.get("/to/level").GetString(), doc.get("/from/level").GetString());
}

TEST(JsonTest, SetReferenceMultiLevel)
{
    Document doc{R"({"from": {"multi": {"level": "value"}}})"};
    ASSERT_NO_THROW(doc.set("/to/multi/level", "/from/multi/level"));
    ASSERT_STREQ(doc.get("/to/multi/level").GetString(), doc.get("/from/multi/level").GetString());
}

TEST(JsonTest, Equals)
{
    Document doc{R"({"from": 1})"};
    ASSERT_NO_THROW(doc.equals("/from", rapidjson::Value{1}));
    ASSERT_TRUE(doc.equals("/from", rapidjson::Value{1}));
}

TEST(JsonTest, Exists)
{
    Document doc{R"({"from": 1})"};
    ASSERT_NO_THROW(doc.exists("/from"));
    ASSERT_TRUE(doc.exists("/from"));
    ASSERT_FALSE(doc.exists("/frdom"));
}

TEST(JsonTest, Str)
{
    Document doc{R"({"from": 1})"};
    ASSERT_STREQ(R"({"from":1})", doc.str().c_str());
}

TEST(JsonTest, PrettyStr)
{
    Document doc{R"({"from": 1})"};
    ASSERT_STREQ(R"({
    "from": 1
})",
                 doc.prettyStr().c_str());
}

TEST(JsonTest, Lambdas)
{
    Document doc{R"({"from": 1})"};
    rapidjson::Document alld;
    rapidjson::Value val1{doc.get("/from"), alld.GetAllocator()};
    rapidjson::Value val2{val1, alld.GetAllocator()};
    auto l1 = [val = rapidjson::Value{val1, alld.GetAllocator()}]() { ASSERT_TRUE(val.IsNumber()); };
    ASSERT_TRUE(val1.IsNumber());
    val2 = val1;
    ASSERT_FALSE(val1.IsNumber());
    val2.IsString();
    l1();
}
