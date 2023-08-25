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

#ifndef _ACTION_ORCHESTRATOR_TEST_HPP
#define _ACTION_ORCHESTRATOR_TEST_HPP

#include "gtest/gtest.h"
#include <external/nlohmann/json.hpp>
#include <filesystem>
#include <string>

/**
 * @brief Runs unit tests for ActionOrchestrator
 */
class ActionOrchestratorTest : public ::testing::Test
{
protected:
    ActionOrchestratorTest() = default;
    ~ActionOrchestratorTest() override = default;

    nlohmann::json m_parameters;

    /**
     * @brief Sets initial conditions for each test case.
     */
    // cppcheck-suppress unusedFunction
    void SetUp() override
    {
        m_parameters = R"(
            {
                "topicName": "action-orchestrator-tests",
                "interval": 1,
                "ondemand": true,
                "configData": {
                    "contentSource": "api",
                    "compressionType": "raw",
                    "versionedContent": "false",
                    "deleteDownloadedContent": false,
                    "url": "https://swapi.dev/api/people/1",
                    "outputFolder": "/tmp/action-orchestrator-tests",
                    "dataFormat": "json",
                    "fileName": "sample1.json"
                }
            }
        )"_json;
    }
    /**
     * @brief Tear down routine for tests
     *
     */
    // cppcheck-suppress unusedFunction
    void TearDown() override
    {
        // Removes the directory if it exists
        const auto outputFolder = m_parameters.at("configData").at("outputFolder").get<const std::string>();
        if (std::filesystem::exists(outputFolder))
        {
            // Delete the output folder.
            std::filesystem::remove_all(outputFolder);
        }
    }
};

#endif //_ACTION_ORCHESTRATOR_TEST_HPP
