#include <any>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <json/json.hpp>

#include <baseTypes.hpp>
#include <defs/mocks/failDef.hpp>

#include <kvdb/kvdbManager.hpp>
#include <opBuilderKVDB.hpp>
#include <testsCommon.hpp>

#include <metrics/metricsManager.hpp>

using namespace metricsManager;

namespace
{
using namespace base;
using namespace builder::internals::builders;

using json::Json;
using std::string;
using std::vector;

class opBuilderKVDBSetTest : public ::testing::Test
{

protected:
    static constexpr auto DB_NAME_1 = "test_db";
    static constexpr auto DB_DIR = "/tmp/kvdbTestSuitePath/";
    static constexpr auto DB_NAME = "kvdb";

    std::shared_ptr<IMetricsManager> m_manager;
    std::shared_ptr<kvdbManager::KVDBManager> kvdbManager;
    std::string kvdbPath;

    void SetUp() override
    {
        initLogging();

        // cleaning directory in order to start without garbage.
        kvdbPath = generateRandomStringWithPrefix(6, DB_DIR) + "/";

        if (std::filesystem::exists(kvdbPath))
        {
            std::filesystem::remove_all(kvdbPath);
        }

        m_manager = std::make_shared<MetricsManager>();
        kvdbManager::KVDBManagerOptions kvdbManagerOptions {kvdbPath, DB_NAME};
        kvdbManager = std::make_shared<kvdbManager::KVDBManager>(kvdbManagerOptions, m_manager);

        kvdbManager->initialize();

        ASSERT_FALSE(kvdbManager->createDB("test_db"));
    }

    void TearDown() override
    {
        try
        {
            kvdbManager->finalize();
        }
        catch (const std::exception& e)
        {
            FAIL() << "Exception: " << e.what();
        }

        if (std::filesystem::exists(kvdbPath))
        {
            std::filesystem::remove_all(kvdbPath);
        }
    }
};

// Build ok
TEST_F(opBuilderKVDBSetTest, buildKVDBSetWithValues)
{
    ASSERT_NO_THROW(KVDBSet(kvdbManager,
                            "builder_test",
                            "/output",
                            "",
                            {DB_NAME_1, "key", "value"},
                            std::make_shared<defs::mocks::FailDef>()));
}

TEST_F(opBuilderKVDBSetTest, buildKVDBSetWithReferences)
{
    ASSERT_THROW(KVDBSet(kvdbManager,
                         "builder_test",
                         "/output",
                         "",
                         {"$SOME_DB_NAME_1", "$key", "$value"},
                         std::make_shared<defs::mocks::FailDef>()),
                 std::runtime_error);
}

TEST_F(opBuilderKVDBSetTest, buildKVDBSetWrongAmountOfParametersError)
{
    ASSERT_THROW(KVDBSet(kvdbManager, "builder_test", "/output", "", {}, std::make_shared<defs::mocks::FailDef>()),
                 std::runtime_error);
    ASSERT_THROW(
        KVDBSet(kvdbManager, "builder_test", "/output", "", {DB_NAME_1}, std::make_shared<defs::mocks::FailDef>()),
        std::runtime_error);
    ASSERT_THROW(
        KVDBSet(
            kvdbManager, "builder_test", "/output", "", {DB_NAME_1, "key"}, std::make_shared<defs::mocks::FailDef>()),
        std::runtime_error);
    ASSERT_THROW(KVDBSet(kvdbManager,
                         "builder_test",
                         "/output",
                         "",
                         {DB_NAME_1, "key", "value", "unexpected"},
                         std::make_shared<defs::mocks::FailDef>()),
                 std::runtime_error);
}

TEST_F(opBuilderKVDBSetTest, SetSuccessCases)
{
    Json eventTemplate {R"({
        "databaseName": "some_DB_NAME_1",
        "fieldString": "value",
        "fieldIntNumber": 1,
        "fieldDoubleNumber": 69.007,
        "fieldObject": {"field": "value"},
        "fieldArray": ["value"],
        "fieldNull": null,
        "fieldTrue": true,
        "fieldFalse": false
    })"};

    auto event = std::make_shared<Json>(eventTemplate);
    auto expectedEvent = std::make_shared<Json>(eventTemplate);
    expectedEvent->setBool(true, "/output");

    string dbOp1 {DB_NAME_1};
    string keyOp1 {"some_key"};
    string valueOp1 {"some_value"};
    const auto op1 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp1, valueOp1}, std::make_shared<defs::mocks::FailDef>());

    string dbOp2 {DB_NAME_1};
    string keyOp2 {"fieldString"};
    string valueOp2 {"$fieldString"};
    const auto op2 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp2, valueOp2}, std::make_shared<defs::mocks::FailDef>());

    string dbOp3 {DB_NAME_1};
    string keyOp3 {"fieldIntNumber"};
    string valueOp3 {"$fieldIntNumber"};
    const auto op3 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp3, valueOp3}, std::make_shared<defs::mocks::FailDef>());

    string dbOp4 {DB_NAME_1};
    string keyOp4 {"fieldObject"};
    string valueOp4 {"$fieldObject"};
    const auto op4 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp4, valueOp4}, std::make_shared<defs::mocks::FailDef>());

    string dbOp5 {DB_NAME_1};
    string keyOp5 {"fieldArray"};
    string valueOp5 {"$fieldArray"};
    const auto op5 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp5, valueOp5}, std::make_shared<defs::mocks::FailDef>());

    string dbOp6 {DB_NAME_1};
    string keyOp6 {"fieldNull"};
    string valueOp6 {"$fieldNull"};
    const auto op6 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp6, valueOp6}, std::make_shared<defs::mocks::FailDef>());

    string dbOp7 {DB_NAME_1};
    string keyOp7 {"fieldDoubleNumber"};
    string valueOp7 {"$fieldDoubleNumber"};
    const auto op7 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp7, valueOp7}, std::make_shared<defs::mocks::FailDef>());

    string dbOp8 {DB_NAME_1};
    string keyOp8 {"fieldTrue"};
    string valueOp8 {"$fieldTrue"};
    const auto op8 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp8, valueOp8}, std::make_shared<defs::mocks::FailDef>());

    string dbOp9 {DB_NAME_1};
    string keyOp9 {"fieldFalse"};
    string valueOp9 {"$fieldFalse"};
    const auto op9 = getOpBuilderKVDBSet(kvdbManager, "builder_test")(
        "/output", "", {DB_NAME_1, keyOp9, valueOp9}, std::make_shared<defs::mocks::FailDef>());

    auto result = op1->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    auto res = kvdbManager->getKVDBHandler(DB_NAME_1, "builder_test");
    ASSERT_FALSE(std::holds_alternative<base::Error>(res));
    auto handler = std::move(std::get<std::shared_ptr<kvdbManager::IKVDBHandler>>(res));
    auto rawValue = handler->get(keyOp1);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(), "some_value");

    result = op2->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    rawValue = handler->get(keyOp2);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(),
                 expectedEvent->getJson(Json::formatJsonPath(keyOp2)).value().str().c_str());

    // TODO: fix this
    result = op3->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    rawValue = handler->get(keyOp3);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(),
                 expectedEvent->getJson(Json::formatJsonPath(keyOp3)).value().str().c_str());

    result = op4->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    rawValue = handler->get(keyOp4);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(),
                 expectedEvent->getJson(Json::formatJsonPath(keyOp4)).value().str().c_str());

    result = op5->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    rawValue = handler->get(keyOp5);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(),
                 expectedEvent->getJson(Json::formatJsonPath(keyOp5)).value().str().c_str());

    result = op6->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    rawValue = handler->get(keyOp6);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(), "null");

    // TODO: fix this
    result = op7->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    rawValue = handler->get(keyOp7);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(),
                 expectedEvent->getJson(Json::formatJsonPath(keyOp7)).value().str().c_str());

    result = op8->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    rawValue = handler->get(keyOp8);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(),
                 expectedEvent->getJson(Json::formatJsonPath(keyOp8)).value().str().c_str());

    result = op9->getPtr<Term<EngineOp>>()->getFn()(event);
    ASSERT_TRUE(result);
    ASSERT_EQ(*result.payload(), *expectedEvent);
    rawValue = handler->get(keyOp9);
    if (const auto err = std::get_if<base::Error>(&rawValue))
    {
        throw std::runtime_error(err->message);
    }
    ASSERT_STREQ(std::get<string>(rawValue).c_str(),
                 expectedEvent->getJson(Json::formatJsonPath(keyOp9)).value().str().c_str());
}

} // namespace
