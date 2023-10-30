/*
 * Wazuh content manager - Unit Tests
 * Copyright (C) 2015, Wazuh Inc.
 * Jun 09, 2023.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "factoryDownloader_test.hpp"
#include "CtiApiDownloader.hpp"
#include "S3Downloader.hpp"
#include "chainOfResponsability.hpp"
#include "factoryDownloader.hpp"
#include "json.hpp"
#include "offlineDownloader.hpp"
#include "updaterContext.hpp"
#include "gtest/gtest.h"
#include <memory>

/*
 * @brief Check the creation of a CtiApiDownloader.
 */
TEST_F(FactoryDownloaderTest, CreateCtiApiDownloader)
{
    // Create the config
    nlohmann::json config = {{"contentSource", "cti-api"}};

    // Create the downloader
    std::shared_ptr<AbstractHandler<std::shared_ptr<UpdaterContext>>> spDownloader {};
    EXPECT_NO_THROW(spDownloader = FactoryDownloader::create(config));

    // Check if the downloader is a CtiApiDownloader
    EXPECT_TRUE(std::dynamic_pointer_cast<CtiApiDownloader>(spDownloader));
}

/*
 * @brief Check the creation of a S3Downloader.
 */
TEST_F(FactoryDownloaderTest, CreateS3Downloader)
{
    // Create the config
    nlohmann::json config = {{"contentSource", "s3"}};

    // Create the downloader
    std::shared_ptr<AbstractHandler<std::shared_ptr<UpdaterContext>>> spDownloader {};
    EXPECT_NO_THROW(spDownloader = FactoryDownloader::create(config));

    // Check if the downloader is a S3Downloader
    EXPECT_TRUE(std::dynamic_pointer_cast<S3Downloader>(spDownloader));
}

/**
 * @brief Check the creation of a OfflineDownloader for downloading a raw file.
 *
 */
TEST_F(FactoryDownloaderTest, CreateOfflineDownloaderRawFile)
{
    auto config = R"(
        {
            "contentSource": "offline",
            "url": "file:///home/user/file.txt",
            "compressionType": "ignored"
        }
    )"_json;

    const auto expectedConfig = R"(
        {
            "contentSource": "offline",
            "url": "file:///home/user/file.txt",
            "compressionType": "raw"
        }
    )"_json;

    // Create the downloader.
    std::shared_ptr<AbstractHandler<std::shared_ptr<UpdaterContext>>> spDownloader {};

    EXPECT_NO_THROW(spDownloader = FactoryDownloader::create(config));
    EXPECT_TRUE(std::dynamic_pointer_cast<OfflineDownloader>(spDownloader));
    EXPECT_EQ(config, expectedConfig);
}

/**
 * @brief Check the creation of a OfflineDownloader for downloading a compressed file.
 *
 */
TEST_F(FactoryDownloaderTest, CreateOfflineDownloaderCompressedFile)
{
    auto config = R"(
        {
            "contentSource": "offline",
            "url": "file:///home/user/file.txt.gz",
            "compressionType": "ignored"
        }
    )"_json;

    const auto expectedConfig = R"(
        {
            "contentSource": "offline",
            "url": "file:///home/user/file.txt.gz",
            "compressionType": "gzip"
        }
    )"_json;

    // Create the downloader.
    std::shared_ptr<AbstractHandler<std::shared_ptr<UpdaterContext>>> spDownloader {};

    EXPECT_NO_THROW(spDownloader = FactoryDownloader::create(config));
    EXPECT_TRUE(std::dynamic_pointer_cast<OfflineDownloader>(spDownloader));
    EXPECT_EQ(config, expectedConfig);
}

/**
 * @brief Check the creation of a OfflineDownloader for downloading a file with no extension.
 *
 */
TEST_F(FactoryDownloaderTest, CreateOfflineDownloaderNoExtension)
{
    auto config = R"(
        {
            "contentSource": "offline",
            "url": "file:///home/user/file_without_extension",
            "compressionType": "ignored"
        }
    )"_json;

    const auto expectedConfig = R"(
        {
            "contentSource": "offline",
            "url": "file:///home/user/file_without_extension",
            "compressionType": "raw"
        }
    )"_json;

    // Create the downloader.
    std::shared_ptr<AbstractHandler<std::shared_ptr<UpdaterContext>>> spDownloader {};

    EXPECT_NO_THROW(spDownloader = FactoryDownloader::create(config));
    EXPECT_TRUE(std::dynamic_pointer_cast<OfflineDownloader>(spDownloader));
    EXPECT_EQ(config, expectedConfig);
}

/*
 * @brief Check an invalid contentSource type.
 */
TEST_F(FactoryDownloaderTest, InvalidContentSource)
{
    // Create the config
    nlohmann::json config = {{"contentSource", "invalid"}};

    // Create the downloader
    EXPECT_THROW(FactoryDownloader::create(config), std::invalid_argument);
}
