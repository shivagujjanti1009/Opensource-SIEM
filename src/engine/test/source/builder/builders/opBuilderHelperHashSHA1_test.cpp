#include <any>
#include <gtest/gtest.h>
#include <vector>

#include <baseTypes.hpp>
#include <defs/failDef.hpp>

#include "opBuilderHelperMap.hpp"

using namespace base;
namespace bld = builder::internals::builders;

// SHA1 hash results
constexpr char wordWorldWorstHash[] {"6184625af204bbfc7e751d49cdc1d7c7ee15091a"};
constexpr char xxxHash[] {"a9674b19f8c56f785c91a555d0a144522bb318e6"};
constexpr char wordHash[] {"3cbcd90adc4b192a87a625850b7f231caddf0eb3"};
constexpr char abcdeHash[] {"7be07aaf460d593a323d0db33da05b64bfdcb3a5"};

TEST(opBuilderHelperHashSHA1, Builds)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {"begin"},
                                       std::make_shared<defs::mocks::FailDef>());

    ASSERT_NO_THROW(std::apply(bld::opBuilderHelperHashSHA1, tuple));
}

TEST(opBuilderHelperHashSHA1, Correct_execution_with_single_string)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {"XXX"},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"arrayField": ["A","B","C","D","E"]})");

    auto op = std::apply(bld::opBuilderHelperHashSHA1, tuple)->getPtr<Term<EngineOp>>()->getFn();
    result::Result<Event> result = op(event1);
    ASSERT_TRUE(result);
    ASSERT_EQ(xxxHash, result.payload()->getString("/field").value());
}

TEST(opBuilderHelperHashSHA1, Correct_execution_with_several_strings)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {"wordworldworst"},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"Field": "ABCDE"})");

    auto op = std::apply(bld::opBuilderHelperHashSHA1, tuple)->getPtr<Term<EngineOp>>()->getFn();
    result::Result<Event> result = op(event1);
    ASSERT_TRUE(result);
    ASSERT_EQ(wordWorldWorstHash, result.payload()->getString("/field").value());
}

TEST(opBuilderHelperHashSHA1, Correct_execution_with_reference)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {"$fieldReference"},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"fieldReference": "wordworldworst"})");

    auto op = std::apply(bld::opBuilderHelperHashSHA1, tuple)->getPtr<Term<EngineOp>>()->getFn();
    result::Result<Event> result = op(event1);
    ASSERT_TRUE(result);
    ASSERT_EQ(wordWorldWorstHash, result.payload()->getString("/field").value());
}

TEST(opBuilderHelperHashSHA1, Correct_execution_with_nested_reference)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {"$object.fieldReference3"},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"object":{"fieldReference3":"word"}})");

    auto op = std::apply(bld::opBuilderHelperHashSHA1, tuple)->getPtr<Term<EngineOp>>()->getFn();
    result::Result<Event> result = op(event1);
    ASSERT_TRUE(result);
    ASSERT_EQ(wordHash, result.payload()->getString("/field").value());
}

TEST(opBuilderHelperHashSHA1, No_parameters_error)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"arrayField": "A"})");

    ASSERT_THROW(std::apply(bld::opBuilderHelperHashSHA1, tuple), std::runtime_error);
}

TEST(opBuilderHelperHashSHA1, Empty_parameter)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {""},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"fieldReference": "word"})");

    auto op = std::apply(bld::opBuilderHelperHashSHA1, tuple)->getPtr<Term<EngineOp>>()->getFn();
    result::Result<Event> result = op(event1);
    ASSERT_TRUE(result);
}

TEST(opBuilderHelperHashSHA1, Empty_reference_parameter)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {"$fieldReference"},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"fieldReference": ""})");

    auto op = std::apply(bld::opBuilderHelperHashSHA1, tuple)->getPtr<Term<EngineOp>>()->getFn();
    result::Result<Event> result = op(event1);
    ASSERT_TRUE(result);
}

TEST(opBuilderHelperHashSHA1, Failed_execution_with_array_reference)
{
    const auto tuple = std::make_tuple(std::string {"/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {"$arrayField"},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"arrayField": ["A","B","C","D","E"]})");

    auto op = std::apply(bld::opBuilderHelperHashSHA1, tuple)->getPtr<Term<EngineOp>>()->getFn();
    result::Result<Event> result = op(event1);
    ASSERT_FALSE(result);
}

TEST(opBuilderHelperHashSHA1, Correct_execution_with_nested_result)
{
    const auto tuple = std::make_tuple(std::string {"/object/field"},
                                       std::string {"sha1"},
                                       std::vector<std::string> {"ABCDE"},
                                       std::make_shared<defs::mocks::FailDef>());

    const auto event1 = std::make_shared<json::Json>(R"({"object":[{"field":"worst"},{"field2":1}]})");

    auto op = std::apply(bld::opBuilderHelperHashSHA1, tuple)->getPtr<Term<EngineOp>>()->getFn();
    result::Result<Event> result = op(event1);
    ASSERT_TRUE(result);
    ASSERT_EQ(abcdeHash, result.payload()->getString("/object/field").value());
}
