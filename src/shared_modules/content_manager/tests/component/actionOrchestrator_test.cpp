/*
 * Wazuh content manager - Component Tests
 * Copyright (C) 2015, Wazuh Inc.
 * July 26, 2023.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "actionOrchestrator_test.hpp"
#include "actionOrchestrator.hpp"
#include "routerProvider.hpp"
#include "stringHelper.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Log
{
    std::function<void(
        const int, const std::string&, const std::string&, const int, const std::string&, const std::string&, va_list)>
        GLOBAL_LOG_FUNCTION;
};

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class
 */
TEST_F(ActionOrchestratorTest, TestInstantiation)
{
    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    EXPECT_NO_THROW(std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition));

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class without configData
 */
TEST_F(ActionOrchestratorTest, TestInstantiationWhitoutConfigData)
{
    // creates a copy of `m_parameters` because it's used in `TearDown` method
    auto parameters = m_parameters;

    const auto& topicName {parameters.at("topicName").get_ref<const std::string&>()};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    parameters.erase("configData");

    EXPECT_THROW(std::make_shared<ActionOrchestrator>(routerProvider, parameters, m_spStopActionCondition),
                 std::invalid_argument);

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class without contentSource in configData
 */
TEST_F(ActionOrchestratorTest, TestInstantiationWhitoutContentSourceInConfigData)
{
    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    m_parameters.at("configData").erase("contentSource");

    EXPECT_THROW(std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition),
                 std::invalid_argument);

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class without compressionType in configData
 */
TEST_F(ActionOrchestratorTest, TestInstantiationWhitoutCompressionTypeInConfigData)
{
    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    m_parameters.at("configData").erase("compressionType");

    EXPECT_THROW(std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition),
                 std::invalid_argument);

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class with xz compressionType
 */
TEST_F(ActionOrchestratorTest, TestInstantiationWhitXZCompressionType)
{
    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    m_parameters["configData"]["compressionType"] = "xz";

    EXPECT_NO_THROW(std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition));

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class without versionedContent in configData
 */
TEST_F(ActionOrchestratorTest, TestInstantiationWhitoutVersionedContentInConfigData)
{
    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    m_parameters.at("configData").erase("versionedContent");

    EXPECT_THROW(std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition),
                 std::invalid_argument);

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class without deleteDownloadedContent in configData
 */
TEST_F(ActionOrchestratorTest, TestInstantiationWhitoutDeleteDownloadedContentInConfigData)
{
    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    m_parameters.at("configData").erase("deleteDownloadedContent");

    EXPECT_THROW(std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition),
                 std::invalid_argument);

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class and execution of the run method for raw data
 */
TEST_F(ActionOrchestratorTest, TestInstantiationAndExecutionWhitRawCompressionType)
{
    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};
    const auto& fileName {m_parameters.at("configData").at("contentFileName").get_ref<const std::string&>()};
    const auto contentPath {outputFolder + "/" + CONTENTS_FOLDER + "/3-" + fileName};
    const auto downloadPath {outputFolder + "/" + DOWNLOAD_FOLDER + "/3-" + fileName};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    auto actionOrchestrator {
        std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition)};

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(actionOrchestrator->run());

    // This file shouldn't exist because it's a test for raw data
    EXPECT_FALSE(std::filesystem::exists(downloadPath));

    EXPECT_TRUE(std::filesystem::exists(contentPath));

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class and execution of the run method for compressed
 * data
 */
TEST_F(ActionOrchestratorTest, TestInstantiationAndExecutionWhitXZCompressionType)
{
    m_parameters["configData"]["url"] = "http://localhost:4444/xz/consumers";
    m_parameters["configData"]["compressionType"] = "xz";

    // Append XZ extension.
    auto& fileName {m_parameters.at("configData").at("contentFileName").get_ref<std::string&>()};
    fileName += ".xz";

    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};
    const auto downloadPath {outputFolder + "/" + DOWNLOAD_FOLDER + "/3-" + fileName};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    auto actionOrchestrator {
        std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition)};

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(actionOrchestrator->run());

    // This file should exist because deleteDownloadedContent is not enabled
    EXPECT_TRUE(std::filesystem::exists(downloadPath));

    const auto contentPath {outputFolder + "/" + CONTENTS_FOLDER + "/3-" + Utils::rightTrim(fileName, ".xz")};
    EXPECT_TRUE(std::filesystem::exists(contentPath));

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/*
 * @brief Tests the instantiation of the ActionOrchestratorTest class and execution of the run method for compressed
 * data with deleteDownloadedContent enabled
 */
TEST_F(ActionOrchestratorTest, TestInstantiationAndExecutionWhitXZCompressionTypeAndDeleteDownloadedContentEnabled)
{
    m_parameters["configData"]["url"] = "http://localhost:4444/xz/consumers";
    m_parameters["configData"]["compressionType"] = "xz";
    m_parameters["configData"]["deleteDownloadedContent"] = true;

    // Append XZ extension.
    auto& fileName {m_parameters.at("configData").at("contentFileName").get_ref<std::string&>()};
    fileName += ".xz";

    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};
    const auto downloadPath {outputFolder + "/" + DOWNLOAD_FOLDER + "/3-" + fileName};

    auto routerProvider {std::make_shared<RouterProvider>(topicName)};

    EXPECT_NO_THROW(routerProvider->start());

    auto actionOrchestrator {
        std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition)};

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(actionOrchestrator->run());

    // This file shouldn't exist because deleteDownloadedContent is enabled
    EXPECT_FALSE(std::filesystem::exists(downloadPath));

    const auto contentPath {outputFolder + "/" + CONTENTS_FOLDER + "/3-" + Utils::rightTrim(fileName, ".xz")};
    EXPECT_TRUE(std::filesystem::exists(contentPath));

    EXPECT_TRUE(std::filesystem::exists(outputFolder));

    EXPECT_NO_THROW(routerProvider->stop());
}

/**
 * @brief Tests the execution of the orchestration forcing the download of the snapshot before downloading the offsets.
 *
 */
TEST_F(ActionOrchestratorTest, RunWithFullContentDownload)
{
    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    auto routerProvider {std::make_shared<RouterProvider>(topicName)};
    routerProvider->start();

    m_parameters["configData"]["url"] = "http://localhost:4444/snapshot/consumers";
    auto actionOrchestrator {
        std::make_shared<ActionOrchestrator>(routerProvider, m_parameters, m_spStopActionCondition)};

    constexpr auto OFFSET {0};
    auto updateData {ActionOrchestrator::UpdateData()};
    updateData.offset = OFFSET;

    // Trigger orchestration with an offset of zero.
    ASSERT_NO_THROW(actionOrchestrator->run(updateData));

    const auto& outputFolder {m_parameters.at("configData").at("outputFolder").get_ref<const std::string&>()};

    // Check for snapshot existence.
    const auto snapshotPath {outputFolder + "/" + DOWNLOAD_FOLDER + "/" + SNAPSHOT_FILE_NAME};
    EXPECT_TRUE(std::filesystem::exists(snapshotPath));

    // Check for snapshot content existence.
    const auto contentPath {outputFolder + "/" + CONTENTS_FOLDER + "/" + SNAPSHOT_CONTENT_FILE_NAME};
    EXPECT_TRUE(std::filesystem::exists(contentPath));

    routerProvider->stop();
}

/**
 * @brief Tests the offset update process execution with a valid offset.
 *
 */
TEST_F(ActionOrchestratorTest, RunOffsetUpdate)
{
    constexpr auto OFFSET {1234};
    auto updateData {ActionOrchestrator::UpdateData()};
    updateData.type = ActionOrchestrator::UpdateType::OFFSET;
    updateData.offset = OFFSET;

    {
        // Trigger orchestrator in a reduced scope to avoid conflicts with the RocksDB connection below.
        ASSERT_NO_THROW(ActionOrchestrator(m_spMockRouterProvider, m_parameters, m_spStopActionCondition)
                            .run(updateData));
    }

    const auto& topicName {m_parameters.at("topicName").get_ref<const std::string&>()};
    const auto fullDatabasePath {DATABASE_PATH / ("updater_" + topicName + "_metadata")};
    auto wrapper {Utils::RocksDBWrapper(fullDatabasePath)};
    EXPECT_EQ(wrapper.getLastKeyValue(Components::Columns::CURRENT_OFFSET).second.ToString(), std::to_string(OFFSET));
}

/**
 * @brief Tests the offset update process execution with an invalid offset. An exception is expected.
 *
 */
TEST_F(ActionOrchestratorTest, RunOffsetUpdateInvalidOffsetThrows)
{
    constexpr auto OFFSET {-100};
    auto updateData {ActionOrchestrator::UpdateData()};
    updateData.type = ActionOrchestrator::UpdateType::OFFSET;
    updateData.offset = OFFSET;

    EXPECT_THROW(ActionOrchestrator(m_spMockRouterProvider, m_parameters, m_spStopActionCondition)
                     .run(updateData),
                 std::invalid_argument);
}
