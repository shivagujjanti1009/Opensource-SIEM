#include <any>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include <json/json.hpp>

#include <baseTypes.hpp>
#include <defs/mocks/failDef.hpp>
#include <kvdb/ikvdbmanager.hpp>
#include <kvdb/kvdbManager.hpp>
#include <opBuilderKVDB.hpp>
#include <testsCommon.hpp>

#include <mocks/fakeMetric.hpp>

namespace
{
using namespace base;
using namespace metricsManager;
using namespace builder::internals::builders;

static constexpr auto DB_NAME_1 = "TEST_DB_1";
static constexpr auto DB_DIR = "/tmp/kvdbTestSuitePath/";
static constexpr auto DB_NAME = "kvdb";

template<typename T>
class KVDBGetHelper : public ::testing::TestWithParam<T>
{
protected:
    std::shared_ptr<IMetricsManager> m_manager;
    std::shared_ptr<kvdbManager::IKVDBManager> m_kvdbManager;
    std::shared_ptr<defs::mocks::FailDef> m_failDef;
    builder::internals::HelperBuilder m_builder;
    std::string kvdbPath;

    void SetUp() override
    {
        logging::testInit();

        // cleaning directory in order to start without garbage.
        kvdbPath = generateRandomStringWithPrefix(6, DB_DIR) + "/";

        if (std::filesystem::exists(kvdbPath))
        {
            std::filesystem::remove_all(kvdbPath);
        }

        m_manager = std::make_shared<FakeMetricManager>();
        kvdbManager::KVDBManagerOptions kvdbManagerOptions {kvdbPath, DB_NAME};
        m_kvdbManager = std::make_shared<kvdbManager::KVDBManager>(kvdbManagerOptions, m_manager);

        m_kvdbManager->initialize();

        auto err = m_kvdbManager->createDB(DB_NAME_1);
        ASSERT_FALSE(err);

        m_failDef = std::make_shared<defs::mocks::FailDef>();
    }

    void TearDown() override
    {
        try
        {
            m_kvdbManager->finalize();
        }
        catch (std::exception& e)
        {
            FAIL() << "Exception: " << e.what();
        }

        if (std::filesystem::exists(kvdbPath))
        {
            std::filesystem::remove_all(kvdbPath);
        }
    }
};
} // namespace

using GetParamsT = std::tuple<std::vector<std::string>, bool>;
class GetParams : public KVDBGetHelper<GetParamsT>
{
    void SetUp() override
    {
        KVDBGetHelper<GetParamsT>::SetUp();
        m_builder = getOpBuilderKVDBGet(m_kvdbManager, "builder_test");
    }
};

// Test of build params
TEST_P(GetParams, builds)
{
    const std::string targetField = "/field";
    const std::string rawName = "kvdb_get";

    auto [parameters, shouldPass] = GetParam();

    if (shouldPass)
    {
        ASSERT_NO_THROW(m_builder(targetField, rawName, parameters, m_failDef));
    }
    else
    {
        ASSERT_THROW(m_builder(targetField, rawName, parameters, m_failDef), std::runtime_error);
    }
}

INSTANTIATE_TEST_SUITE_P(KVDBGet,
                         GetParams,
                         ::testing::Values(
                             // OK
                             GetParamsT({DB_NAME_1, "key"}, true),
                             GetParamsT({DB_NAME_1, "$key"}, true),
                             // NOK
                             GetParamsT({DB_NAME_1, "test", "test2"}, false),
                             GetParamsT({DB_NAME_1, "test", "$test2"}, false),
                             GetParamsT({DB_NAME_1}, false),
                             GetParamsT({}, false)));

using GetKeyT = std::tuple<std::vector<std::string>, bool, std::string, std::string>;
class GetKey : public KVDBGetHelper<GetKeyT>
{
protected:
    void SetUp() override
    {
        KVDBGetHelper<GetKeyT>::SetUp();

        // Insert initial state to DB
        auto handler = base::getResponse<std::shared_ptr<kvdbManager::IKVDBHandler>>(
            m_kvdbManager->getKVDBHandler(DB_NAME_1, "test"));

        ASSERT_FALSE(handler->set("keyString", R"("string_value")"));
        ASSERT_FALSE(handler->set("keyNumber", "123"));
        ASSERT_FALSE(handler->set("keyObject", R"({"field1": "value1", "field2": "value2"})"));
        ASSERT_FALSE(handler->set("keyArray", R"(["value1", "value2"])"));
        ASSERT_FALSE(handler->set("keyNull", "null"));

        m_builder = getOpBuilderKVDBGet(m_kvdbManager, "builder_test");
    }
};

// Test of get function
TEST_P(GetKey, getting)
{
    const std::string targetField = "/result";
    const std::string rawName = "kvdb_get";

    auto [parameters, shouldPass, rawEvent, rawExpected] = GetParam();
    auto event = std::make_shared<json::Json>(rawEvent.c_str());

    auto op = m_builder(targetField, rawName, parameters, m_failDef)->getPtr<base::Term<base::EngineOp>>()->getFn();

    result::Result<Event> resultEvent;
    ASSERT_NO_THROW(resultEvent = op(event));

    if (shouldPass)
    {
        auto jsonExpected = std::make_shared<json::Json>(rawExpected.c_str());
        ASSERT_TRUE(resultEvent.success());
        ASSERT_EQ(*resultEvent.payload(), *jsonExpected);
    }
    else
    {
        ASSERT_TRUE(resultEvent.failure());
    }
}

INSTANTIATE_TEST_SUITE_P(
    KVDBGet,
    GetKey,
    ::testing::Values(
        // OK
        GetKeyT({DB_NAME_1, "keyString"}, true, R"({})", R"({"result": "string_value"})"),
        GetKeyT({DB_NAME_1, "keyNumber"}, true, R"({})", R"({"result": 123})"),
        GetKeyT({DB_NAME_1, "keyObject"}, true, R"({})", R"({"result": {"field1": "value1", "field2": "value2"}})"),
        GetKeyT({DB_NAME_1, "keyArray"}, true, R"({})", R"({"result": ["value1", "value2"]})"),
        GetKeyT({DB_NAME_1, "keyNull"}, true, R"({})", R"({"result": null})"),
        GetKeyT({DB_NAME_1, "$keyString"}, true, R"({"keyString": "keyString"})", R"({"keyString": "keyString","result": "string_value"})"),
        GetKeyT({DB_NAME_1, "$keyNumber"}, true, R"({"keyNumber": "keyNumber"})", R"({"keyNumber": "keyNumber", "result": 123})"),
        GetKeyT({DB_NAME_1, "$keyObject"}, true, R"({"keyObject": "keyObject"})", R"({"keyObject": "keyObject", "result": {"field1": "value1", "field2": "value2"}})"),
        GetKeyT({DB_NAME_1, "$keyArray"}, true, R"({"keyArray": "keyArray"})", R"({"keyArray": "keyArray", "result": ["value1", "value2"]})"),
        GetKeyT({DB_NAME_1, "$keyNull"}, true, R"({"keyNull": "keyNull"})", R"({"keyNull": "keyNull", "result": null})"),
        // NOK
        GetKeyT({DB_NAME_1, "KEY2"}, false, R"({})", R"({})"),
        GetKeyT({DB_NAME_1, "key_"}, false, R"({})", R"({})"),
        GetKeyT({DB_NAME_1, "$key"}, false, R"({})", R"({})"),
        GetKeyT({DB_NAME_1, ""}, false, R"({})", R"({})")));
