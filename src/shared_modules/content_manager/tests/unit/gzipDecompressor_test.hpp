/*
 * Wazuh content manager - Unit Tests
 * Copyright (C) 2015, Wazuh Inc.
 * October 20, 2023.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#ifndef _GZIP_DECOMPRESSOR_TEST_HPP
#define _GZIP_DECOMPRESSOR_TEST_HPP

#include "updaterContext.hpp"
#include "gtest/gtest.h"
#include <filesystem>
#include <memory>

const auto INPUT_FILES_DIR {std::filesystem::current_path() / "input_files" / "gzipDecompressor"};
const auto OUTPUT_SAMPLE_A_FILE_PATH {INPUT_FILES_DIR / "sample_a.json"};
const auto OUTPUT_SAMPLE_B_FILE_PATH {INPUT_FILES_DIR / "sample_b.json"};

/**
 * @brief Runs unit tests for GzipDecompressor
 *
 */
class GzipDecompressorTest : public ::testing::Test
{
protected:
    GzipDecompressorTest() = default;
    ~GzipDecompressorTest() override = default;

    std::shared_ptr<UpdaterContext> m_spContext; ///< Context used on tests.

    /**
     * @brief Setup routine for each test fixture. Context initialization.
     *
     */
    void SetUp() override
    {
        m_spContext = std::make_shared<UpdaterContext>();
        m_spContext->spUpdaterBaseContext = std::make_shared<UpdaterBaseContext>();
        m_spContext->spUpdaterBaseContext->outputFolder = INPUT_FILES_DIR;
    }

    /**
     * @brief Teardown routine for each test fixture. Output files removal.
     *
     */
    void TearDown() override
    {
        std::filesystem::remove(OUTPUT_SAMPLE_A_FILE_PATH);
        std::filesystem::remove(OUTPUT_SAMPLE_B_FILE_PATH);
    }
};

#endif //_GZIP_DECOMPRESSOR_TEST_HPP
