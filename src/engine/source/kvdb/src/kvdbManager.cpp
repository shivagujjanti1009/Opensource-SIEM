#include <kvdb/kvdbManager.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <set>
#include <unordered_map>

#include <fmt/format.h>

#include <kvdb/kvdb.hpp>
#include <logging/logging.hpp>
#include <utils/baseMacros.hpp>
#include <utils/stringUtils.hpp>

namespace kvdb_manager
{

namespace
{

constexpr bool ERROR_IF_EXISTS {true};
constexpr bool NO_ERROR_IF_EXISTS {false};
constexpr bool CREATE_IF_MISSING {true};
constexpr bool DONT_CREATE_IF_MISSING {false};

// TODO: Delete this
constexpr const char* LEGACY_DIRECTORY {"legacy"};

} // namespace

KVDBManager::KVDBManager(const std::filesystem::path& dbStoragePath)
{
    // TODO should we read and load all the dbs inside the folder?
    // shouldn't be better to just load the configured ones at start instead?

    // TODO Remove this when Engine is integrated in Wazuh installation
    std::filesystem::create_directories(dbStoragePath);
    m_dbStoragePath = dbStoragePath;

    auto legacyPath = m_dbStoragePath;
    legacyPath.append(LEGACY_DIRECTORY);
    if (std::filesystem::exists(legacyPath))
    {
        for (const auto& cdbfile : std::filesystem::directory_iterator(legacyPath))
        {
            // Read it from the config file?
            if (cdbfile.is_regular_file())
            {
                // TODO: check this method result
                createDBfromCDB(cdbfile.path());
            }
        }
    }
}

KVDBHandle KVDBManager::loadDB(const std::string& name, bool createIfMissing)
{
    if (isLoaded(name))
    {
        WAZUH_LOG_ERROR("Engine KVDB manager: \"{}\" method: Database with name \"{}\" "
                        "already loaded.",
                        __func__,
                        name);
        return nullptr;
    }

    WAZUH_LOG_DEBUG("Engine KVDB manager: \"{}\" method: Loading database \"{}\" to the "
                    "available databases list.",
                    __func__,
                    name);
    auto kvdb = std::make_shared<KVDB>(name, m_dbStoragePath);
    auto result = kvdb->init(createIfMissing);
    if (KVDB::CreationStatus::OkInitialized == result
        || KVDB::CreationStatus::OkCreated == result)
    {
        std::unique_lock lk(m_mtx);
        m_loadedDBs[name] = kvdb;
        return m_loadedDBs[name];
    }

    return nullptr;
}

bool KVDBManager::createDBfromCDB(const std::filesystem::path& path)
{
    std::ifstream CDBfile(path);
    if (!CDBfile.is_open())
    {
        WAZUH_LOG_ERROR(
            "Engine KVDB manager: \"{}\" method: CDB file \"{}\" could not be opened.",
            __func__,
            path.string());
        return false;
    }

    const std::string name {path.stem().string()};
    KVDBHandle kvdbHandle {loadDB(name, true)};
    if (!kvdbHandle)
    {
        WAZUH_LOG_ERROR("Engine KVDB manager: \"{}\" method: Failed to create database "
                        "\"{}\" from CDB file \"{}\".",
                        __func__,
                        name,
                        path.string());
        return false;
    }

    for (std::string line; getline(CDBfile, line);)
    {
        line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
        auto kv = utils::string::split(line, ':');
        if (kv.empty() || kv.size() > 2)
        {
            WAZUH_LOG_ERROR(
                "Engine KVDB manager: \"{}\" method: CDB file \"{}\" could not be read.",
                __func__,
                path.string());
            return false;
        }

        kvdbHandle->write(kv[0], kv.size() == 2 ? kv[1] : "");
    }

    return true;
}

bool KVDBManager::unloadDB(const std::string& name)
{
    if (!isLoaded(name))
    {
        WAZUH_LOG_ERROR("Engine KVDB manager: \"{}\" method: Database \"{}\" is not "
                        "present on the KVDB manager databases list.",
                        __func__,
                        name);
        return false;
    }

    std::unique_lock lk(m_mtx);
    m_loadedDBs.erase(name);

    return true;
}

KVDBHandle KVDBManager::getDB(const std::string& name)
{
    if (isLoaded(name))
    {
        auto db = m_loadedDBs[name];
        if (!db->isReady())
        {
            // In general it should never happen so we should consider just
            // removing this
            auto initResult = db->init(DONT_CREATE_IF_MISSING);
            if (initResult != KVDB::CreationStatus::OkCreated
                || initResult != KVDB::CreationStatus::OkInitialized)
            {
                WAZUH_LOG_ERROR("Engine KVDB manager: \"{}\" method: Error initializing "
                                "database \"{}\".",
                                __func__,
                                db->getName());
                return nullptr;
            }
        }

        // return handle
        return db;
    }

    return nullptr;
}

std::vector<std::string> KVDBManager::listDBs(bool loaded)
{
    std::vector<std::string> list;

    if (loaded)
    {
        if (m_loadedDBs.size() > 0)
        {
            for (const auto& var : m_loadedDBs)
            {
                list.emplace_back(var.first);
            }
        }
    }
    else
    {
        for (const auto& path : std::filesystem::directory_iterator(m_dbStoragePath))
        {
            if (path.is_directory())
            {
                std::string name {path.path().stem().string()};
                if (name != LEGACY_DIRECTORY)
                {
                    if (isDBOnPath(name))
                    {
                        list.emplace_back(name);
                    }
                }
            }
        }
    }

    return list;
}

std::variant<KVDBHandle, base::Error> KVDBManager::getHandler(const std::string& name)
{
    auto kvdb = getDB(name);
    if (!kvdb)
    {
        loadDB(name, false);
        kvdb = getDB(name);
        if (!kvdb)
        {
            return base::Error {
                fmt::format("Database '{}' not found or could not be loaded.", name)};
        }
    }
    return kvdb;
}

std::string KVDBManager::CreateAndFillDBfromFile(const std::string& dbName,
                                                 const std::filesystem::path& path)
{
    auto dbHandle = std::make_shared<KVDB>(dbName, m_dbStoragePath);

    // Initialize it only if it doesn't exist
    auto initResult = dbHandle->init(CREATE_IF_MISSING, ERROR_IF_EXISTS);

    if (initResult == KVDB::CreationStatus::ErrorDatabaseAlreadyExists)
    {
        return fmt::format("Database \"{}\" already exists", dbName);
    }
    else if (initResult == KVDB::CreationStatus::ErrorDatabaseBusy)
    {
        return fmt::format("Database \"{}\" is already in use", dbName);
    }
    else if (initResult != KVDB::CreationStatus::OkCreated)
    {
        return fmt::format("Database \"{}\" could not be created", dbName);
    }

    if (!path.empty())
    {
        // Open file and read content
        std::string contents;
        std::ifstream in(path, std::ios::in | std::ios::binary);
        if (in)
        {
            in.seekg(0, std::ios::end);
            contents.resize(in.tellg());
            in.seekg(0, std::ios::beg);
            in.read(&contents[0], contents.size());
            in.close();
        }
        else
        {
            deleteDB(dbName);
            return fmt::format("An error occurred while opening the file \"{}\"",
                               path.c_str());
        }

        json::Json jKv;
        try
        {
            jKv = json::Json {contents.c_str()};
        }
        catch (const std::exception& e)
        {
            deleteDB(dbName);
            return fmt::format("An error occurred while parsing the JSON file \"{}\"",
                               path.c_str());
        }

        if (!jKv.isObject())
        {
            deleteDB(dbName);
            return fmt::format("An error occurred while parsing the JSON file \"{}\": "
                               "JSON is not an object",
                               path.c_str());
        }

        auto entries = jKv.getObject();
        for (const auto& [key, value] : entries.value())
        {

            try
            {
                auto jsValue = value.str();
                dbHandle->write(key, jsValue);
            }
            catch (const std::exception& e)
            {
                deleteDB(dbName);
                return fmt::format("An error occurred while writing the key \"{}\" to "
                                   "the database \"{}\"",
                                   key.c_str(),
                                   dbName.c_str());
            }
        }
    }

    return "OK";
}

bool KVDBManager::getDBFromPath(const std::string& name, KVDBHandle& dbHandle)
{
    dbHandle = std::make_shared<KVDB>(name, m_dbStoragePath);
    // Initialize it only if it already exists
    auto result = dbHandle->init(DONT_CREATE_IF_MISSING, NO_ERROR_IF_EXISTS);
    return (KVDB::CreationStatus::OkInitialized == result
            || KVDB::CreationStatus::OkCreated == result);
}

std::variant<json::Json, base::Error> KVDBManager::jDumpDB(const std::string& name)
{
    auto handle = getHandler(name);
    if (std::holds_alternative<base::Error>(handle))
    {
        return std::get<base::Error>(handle);
    }
    auto& kvdb = std::get<KVDBHandle>(handle);

    return kvdb->jDump();
}

std::optional<base::Error> KVDBManager::writeRaw(const std::string& name,
                                                 const std::string& key,
                                                 const std::string value)
{
    auto handle = getHandler(name);
    if (std::holds_alternative<base::Error>(handle))
    {
        return std::get<base::Error>(handle);
    }
    auto& kvdb = std::get<KVDBHandle>(handle);

    if (kvdb->write(key, value))
    {
        return std::nullopt;
    }
    return base::Error {
        fmt::format("Could not write key '{}' to database '{}'", key, name)};
}

std::optional<base::Error> KVDBManager::writeKey(const std::string& name,
                                                 const std::string& key,
                                                 const std::string& value)
{
    json::Json jValue;
    try
    {
        jValue = json::Json {value.c_str()};
    }
    catch (const std::exception& e)
    {
        jValue.setString(value);
    }

    return writeKey(name, key, jValue);
}

std::variant<std::string, base::Error> KVDBManager::getRawValue(const std::string& name,
                                                                const std::string& key)
{
    auto handle = getHandler(name);
    if (std::holds_alternative<base::Error>(handle))
    {
        return std::get<base::Error>(handle);
    }
    auto& kvdb = std::get<KVDBHandle>(handle);

    return kvdb->read(key);
}

std::variant<json::Json, base::Error> KVDBManager::getJValue(const std::string& name,
                                                             const std::string& key)
{
    const auto result = getRawValue(name, key);
    if (std::holds_alternative<base::Error>(result))
    {
        return std::get<base::Error>(result);
    }
    const auto& value = std::get<std::string>(result);
    json::Json jValue;
    try
    {
        jValue = json::Json {value.c_str()};
    }
    catch (const std::exception& e)
    {
        return base::Error {fmt::format(
            "Could not parse value '{}' from database '{}' (corrupted value: '{}')",
            key,
            value.c_str(),
            name)};
    }

    return jValue;
}

bool KVDBManager::deleteKey(const std::string& name, const std::string& key)
{
    bool result {false};
    auto handle = getHandler(name);
    if (std::holds_alternative<base::Error>(handle))
    {
        return result;
    }
    auto& kvdb = std::get<KVDBHandle>(handle);

    result = kvdb->deleteKey(key); // TODO check if exist before delete
    return result; // TODO Change to variant
}

bool KVDBManager::isDBOnPath(const std::string& name)
{
    auto dbHandle = std::make_shared<KVDB>(name, m_dbStoragePath);
    auto result = dbHandle->init(DONT_CREATE_IF_MISSING, NO_ERROR_IF_EXISTS);
    return (result != KVDB::CreationStatus::ErrorUnknown);
}

// TODO: Delete this method.
bool KVDBManager::isLoaded(const std::string& name)
{
    auto it = m_loadedDBs.find(name);
    return (it != m_loadedDBs.end());
}

std::optional<std::string> KVDBManager::deleteDB(const std::string& name)
{
    const auto MAX_USE_COUNT = 2; // 1 for the map and 1 for getHandler

    auto res = getHandler(name);
    if (std::holds_alternative<base::Error>(res))
    {
        return std::get<base::Error>(res).message;
    }

    // Check if the database is loaded
    auto& handler = std::get<KVDBHandle>(res);
    if (handler.use_count() > MAX_USE_COUNT)
    {
        return fmt::format(
            "Database '{}' is already in use '{}' times", name, handler.use_count() - MAX_USE_COUNT);
    }

    // Delete the reference of the database list
    {
        auto lock = std::unique_lock {m_mtx};
        // Check again because it could have changed while waiting for the lock
        // Its more efficient to check again than to lock the mutex before checking the
        // first time
        if (handler.use_count() == MAX_USE_COUNT)
        {
            m_loadedDBs.erase(name);
        } else {
            return fmt::format(
                "Database '{}' is already in use '{}' times", name, handler.use_count() - MAX_USE_COUNT);
        }
    }
    // Mark for deletion
    handler->cleanupOnClose();

    return std::nullopt;
}

} // namespace kvdb_manager
