/*
 * Wazuh keystore
 * Copyright (C) 2015, Wazuh Inc.
 * January 24, 2024.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "keyStore.hpp"
#include "rsaHelper.hpp"
#include "loggerHelper.h"

constexpr auto KS_NAME {"keystore"};

void Keystore::put(const std::string& columnFamily, const std::string& key, const std::string& value)
{
    std::string encryptedValue;

    // Encrypt value
    try
    {
        auto encrypted_len = Utils::rsaEncrypt(CERTIFICATE_FILE, value, encryptedValue, true);
    }
    catch (std::exception& e)
    {
        logError(KS_NAME, "%s", e.what());
        throw std::runtime_error(e.what());
    }

    // Insert to DB
    try
    {
        Utils::RocksDBWrapper keystoreDB = Utils::RocksDBWrapper(DATABASE_PATH, false);

        if (!keystoreDB.columnExists(columnFamily)) {
            keystoreDB.createColumn(columnFamily);
        }

        keystoreDB.put(key, rocksdb::Slice(encryptedValue), columnFamily);
    }
    catch (std::exception& e)
    {
        logError(KS_NAME, "%s", e.what());
        throw std::runtime_error(e.what());
    }
}

void Keystore::get(const std::string& columnFamily, const std::string& key, rocksdb::PinnableSlice& value)
{
}
