/*
 * Wazuh content manager
 * Copyright (C) 2015, Wazuh Inc.
 * April 26, 2023.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef _ACTION_ORCHESTRATOR_HPP
#define _ACTION_ORCHESTRATOR_HPP

#include "components/executionContext.hpp"
#include "components/factoryContentUpdater.hpp"
#include "components/updaterContext.hpp"
#include "componentsHelper.hpp"
#include "factoryOffsetUpdater.hpp"
#include "iRouterProvider.hpp"
#include "utils/rocksDBWrapper.hpp"
#include <memory>
#include <utility>

/**
 * @brief In charge of initializing the content updater orchestration.
 *
 */
class ActionOrchestrator final
{
public:
    /**
     * @brief Enum that represents the type of update that exists.
     *
     */
    enum UpdateType
    {
        CONTENT,
        OFFSET
    };

    /**
     * @brief Creates a new instance of ActionOrchestrator.
     *
     * @param channel Channel where the orchestration will publish the data.
     * @param parameters Parameters used to create the orchestration.
     * @param stopActionCondition Condition wrapper used to interrupt the orchestration stages.
     */
    explicit ActionOrchestrator(const std::shared_ptr<IRouterProvider> channel,
                                const nlohmann::json& parameters,
                                std::shared_ptr<ConditionSync> stopActionCondition)
    {
        try
        {
            // Create a context
            m_spBaseContext = std::make_shared<UpdaterBaseContext>(stopActionCondition);
            m_spBaseContext->topicName = parameters.at("topicName");
            m_spBaseContext->configData = parameters.at("configData");
            m_spBaseContext->spChannel = channel;

            logDebug1(
                WM_CONTENTUPDATER, "Creating '%s' Content Updater orchestration", m_spBaseContext->topicName.c_str());

            // Create and run the execution context
            auto executionContext {std::make_shared<ExecutionContext>()};
            executionContext->handleRequest(m_spBaseContext);

            // Create a updater chain
            m_spUpdaterOrchestration = FactoryContentUpdater::create(m_spBaseContext->configData);

            logDebug1(WM_CONTENTUPDATER, "Content updater orchestration created");
        }
        catch (const std::exception& e)
        {
            throw std::invalid_argument {"Orchestration creation failed. " + std::string {e.what()}};
        }
    }

    /**
     * @brief Run the content updater orchestration.
     *
     * @param offset Manually set current offset to process. Default -1
     * @param type Type of update the orchestrator should perform.
     */
    void run(const int offset = -1, const UpdateType type = UpdateType::CONTENT) const
    {
        // Create a updater context
        auto spUpdaterContext {std::make_shared<UpdaterContext>()};
        spUpdaterContext->spUpdaterBaseContext = m_spBaseContext;

        try
        {
            if (type == UpdateType::OFFSET)
            {
                return runOffsetUpdate(spUpdaterContext, offset);
            }

            return runContentUpdate(spUpdaterContext, offset);
        }
        catch (const std::exception& e)
        {
            cleanContext();
            throw std::invalid_argument {"Orchestration run failed: " + std::string {e.what()}};
        }
    }

private:
    /**
     * @brief Content updater orchestration.
     */
    std::shared_ptr<AbstractHandler<std::shared_ptr<UpdaterContext>>> m_spUpdaterOrchestration;

    /**
     * @brief Context used on the content updater orchestration.
     */
    std::shared_ptr<UpdaterBaseContext> m_spBaseContext;

    /**
     * @brief Clean ContentUpdater persistent data. Useful for cleaning the context when an exception is thrown.
     *
     */
    void cleanContext() const
    {
        m_spBaseContext->downloadedFileHash.clear();
    }

    /**
     * @brief Creates and triggers a new orchestration that updates the offset in the database.
     *
     * @param spUpdaterContext Updater context.
     * @param offset New value of the offset.
     */
    void runOffsetUpdate(std::shared_ptr<UpdaterContext> spUpdaterContext, int offset) const
    {
        logDebug2(WM_CONTENTUPDATER, "Running '%s' offset update", m_spBaseContext->topicName.c_str());

        if (0 > offset)
        {
            throw std::invalid_argument {"Invalid offset value: " + std::to_string(offset)};
        }

        spUpdaterContext->currentOffset = offset;

        FactoryOffsetUpdater::create(m_spBaseContext->configData)->handleRequest(spUpdaterContext);
    }

    /**
     * @brief Triggers a new orchestration that updates the content.
     *
     * @param spUpdaterContext Updater context.
     * @param offset If zero, resets the current offset.
     */
    void runContentUpdate(std::shared_ptr<UpdaterContext> spUpdaterContext, int offset) const
    {
        logDebug2(WM_CONTENTUPDATER, "Running '%s' content update", m_spBaseContext->topicName.c_str());

        // If the database exists, get the last offset
        if (m_spBaseContext->spRocksDB)
        {
            spUpdaterContext->currentOffset = std::stoi(
                m_spBaseContext->spRocksDB->getLastKeyValue(Components::Columns::CURRENT_OFFSET).second.ToString());
        }

        if (offset == 0)
        {
            spUpdaterContext->currentOffset = 0;
        }

        // If an offset download is requested and the current offset is '0', a snapshot will be downloaded with
        // the full content to avoid downloading many offsets at once.
        const auto& contentSource {m_spBaseContext->configData.at("contentSource").get_ref<const std::string&>()};
        if (0 == spUpdaterContext->currentOffset && "cti-offset" == contentSource)
        {
            runFullContentDownload(spUpdaterContext);
        }

        // Store last file hash.
        const auto lastDownloadedFileHash {m_spBaseContext->downloadedFileHash};

        // Run the updater chain
        m_spUpdaterOrchestration->handleRequest(spUpdaterContext);

        // Update filehash if it has changed.
        if (m_spBaseContext->spRocksDB && m_spBaseContext->downloadedFileHash != lastDownloadedFileHash)
        {
            m_spBaseContext->spRocksDB->put(Utils::getCompactTimestamp(std::time(nullptr)),
                                            m_spBaseContext->downloadedFileHash,
                                            Components::Columns::DOWNLOADED_FILE_HASH);
        }
    }

    /**
     * @brief Creates and triggers a new orchestration that downloads a snapshot from CTI.
     *
     * @param spUpdaterContext Updater context.
     */
    void runFullContentDownload(std::shared_ptr<UpdaterContext> spUpdaterContext) const
    {
        logDebug1(WM_CONTENTUPDATER, "Performing full-content download");

        // Set new configuration.
        auto fullContentConfig = spUpdaterContext->spUpdaterBaseContext->configData;
        fullContentConfig.at("contentSource") = "cti-snapshot";
        fullContentConfig.at("compressionType") = "zip";

        // Copy original data.
        auto originalData = spUpdaterContext->data;

        // Trigger orchestration.
        FactoryContentUpdater::create(fullContentConfig)->handleRequest(spUpdaterContext);

        // Restore original data.
        spUpdaterContext->data = std::move(originalData);
    }
};

#endif // _ACTION_ORCHESTRATOR_HPP
