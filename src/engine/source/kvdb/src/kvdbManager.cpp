#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <optional>

#include "rocksdb/db.h"
#include "rocksdb/options.h"

#include <kvdb/kvdbManager.hpp>
#include <logging/logging.hpp>
#include <metrics/metricsManager.hpp>

namespace kvdbManager
{

KVDBManager::KVDBManager(const KVDBManagerOptions& options,
                         const std::shared_ptr<metricsManager::IMetricsManager>& metricsManager)
{
    m_ManagerOptions = options;
    m_spMetricsScope = metricsManager->getMetricsScope("KVDB");
    m_kvdbHandlerCollection = std::make_shared<KVDBHandlerCollection>();
}

void KVDBManager::initialize()
{
    if (!m_isInitialized)
    {
        initializeOptions();
        initializeMainDB();
        m_isInitialized = true;
    }
}

void KVDBManager::finalize()
{
    if (m_isInitialized)
    {
        finalizeMainDB();
        m_isInitialized = false;
    }
}

void KVDBManager::initializeOptions()
{
    m_rocksDBOptions = rocksdb::Options();
    m_rocksDBOptions.IncreaseParallelism();
    m_rocksDBOptions.OptimizeLevelStyleCompaction();
    m_rocksDBOptions.create_if_missing = true;
}

void KVDBManager::initializeMainDB()
{
    auto dbStoragePath = m_ManagerOptions.dbStoragePath.string();

    std::filesystem::create_directories(dbStoragePath);

    const std::string dbNameFullPath {fmt::format("{}{}", dbStoragePath, m_ManagerOptions.dbName)};

    std::vector<std::string> columnNames;

    std::vector<rocksdb::ColumnFamilyDescriptor> cfDescriptors;
    std::vector<rocksdb::ColumnFamilyHandle*> cfHandles;

    bool hasDefaultCF = false;
    auto listStatus = rocksdb::DB::ListColumnFamilies(rocksdb::DBOptions(), dbNameFullPath, &columnNames);
    if (listStatus.ok())
    {
        for (const auto& cfName : columnNames)
        {
            if (rocksdb::kDefaultColumnFamilyName == cfName)
            {
                hasDefaultCF = true;
            }

            auto newDescriptor = rocksdb::ColumnFamilyDescriptor(cfName, rocksdb::ColumnFamilyOptions());
            cfDescriptors.push_back(newDescriptor);
        }
    }

    if (!hasDefaultCF)
    {
        auto newDescriptor =
            rocksdb::ColumnFamilyDescriptor(rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions());
        cfDescriptors.push_back(newDescriptor);
    }

    rocksdb::DB* rawRocksDBPtr {nullptr};
    auto statusOpen = rocksdb::DB::Open(m_rocksDBOptions, dbNameFullPath, cfDescriptors, &cfHandles, &rawRocksDBPtr);

    if (statusOpen.ok())
    {
        m_pRocksDB = std::shared_ptr<rocksdb::DB>(rawRocksDBPtr);

        // rocksdb::DB::Open returns two vectors.
        // One with the descriptors containing the names of the DBs. (cfDescriptors)
        // Plus one with the internal handles to the DB. (cfHandles)
        // In this procedure we join these vectors into a map.
        for (std::size_t cfDescriptorIndex = 0; cfDescriptorIndex < cfDescriptors.size(); cfDescriptorIndex++)
        {
            const auto& dbName = cfDescriptors[cfDescriptorIndex].name;
            if (rocksdb::kDefaultColumnFamilyName != dbName)
            {
                m_mapCFHandles.emplace(dbName, createSharedCFHandle(cfHandles[cfDescriptorIndex]));
            }
            else
            {
                m_pDefaultCFHandle = createSharedCFHandle(cfHandles[cfDescriptorIndex]);
            }
        }
    }
    else
    {
        throw std::runtime_error(
            fmt::format("An error occurred while trying to open the database: {}", statusOpen.ToString()));
    }
}

void KVDBManager::finalizeMainDB()
{
    m_mapCFHandles.clear();
    m_pDefaultCFHandle.reset();
    m_pRocksDB.reset();
}

std::variant<std::shared_ptr<IKVDBHandler>, base::Error> KVDBManager::getKVDBHandler(const std::string& dbName,
                                                                                     const std::string& scopeName)
{
    std::shared_ptr<rocksdb::ColumnFamilyHandle> cfHandle;

    if (m_mapCFHandles.count(dbName))
    {
        cfHandle = m_mapCFHandles[dbName];
    }
    else
    {
        return base::Error {fmt::format("The DB {} does not exists.", dbName)};
    }

    m_kvdbHandlerCollection->addKVDBHandler(dbName, scopeName);

    auto kvdbHandler = std::make_shared<KVDBHandler>(m_pRocksDB, cfHandle, m_kvdbHandlerCollection, dbName, scopeName);

    return kvdbHandler;
}

std::vector<std::string> KVDBManager::listDBs(const bool loaded)
{
    std::vector<std::string> spaces;
    spaces.reserve(m_mapCFHandles.size());

    for (const auto& cf : m_mapCFHandles)
    {
        spaces.push_back(cf.first);
    }

    return spaces;
}

std::optional<base::Error> KVDBManager::deleteDB(const std::string& name)
{
    auto handlersInfo = getKVDBHandlersInfo();

    auto refCount = handlersInfo.count(name);
    if (refCount)
    {
        return base::Error {fmt::format("Could not remove the DB {}. Usage Reference Count: {}.", name, refCount)};
    }

    auto it = m_mapCFHandles.find(name);
    if (it != m_mapCFHandles.end())
    {
        auto cfHandle = it->second;

        try
        {
            auto opStatus = m_pRocksDB->DropColumnFamily(cfHandle.get());
            if (opStatus.ok())
            {
                m_mapCFHandles.erase(it);
            }
            else
            {
                return base::Error {
                    fmt::format("Could not remove the DB {}: {}", name, opStatus.ToString())};
            }
        }
        catch (const std::runtime_error& e)
        {
            return base::Error {fmt::format("Could not remove the DB {}: {}", name, e.what())};
        }
    }
    else
    {
        return base::Error {fmt::format("The DB {} does not exists.", name)};
    }

    return std::nullopt;
}

std::optional<base::Error> KVDBManager::loadDBFromFile(const std::string& name, const std::string& path)
{
    std::vector<std::tuple<std::string, json::Json>> entries {};
    std::shared_ptr<rocksdb::ColumnFamilyHandle> cfHandle;

    if (m_mapCFHandles.count(name))
    {
        cfHandle = m_mapCFHandles[name];
    }

    if (!cfHandle)
    {
        return base::Error {fmt::format("The DB {} does not exists.", name)};
    }

    // TODO: to improve
    if (path.empty())
    {
        return base::Error {fmt::format("The path is empty.")};
    }

    // Open file and read content
    std::string contents;
    // TODO: No check the size, the location, the type of file, the permissions it's a
    // security issue. The API should be changed to receive a stream instead of a path
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
        return base::Error {fmt::format("An error occurred while opening the file '{}'", path.c_str())};
    }

    json::Json fileContentsJson;
    try
    {
        fileContentsJson = json::Json {contents.c_str()};
    }
    catch (const std::exception& e)
    {
        return base::Error {fmt::format("An error occurred while parsing the JSON file '{}'", path.c_str())};
    }

    if (!fileContentsJson.isObject())
    {
        return base::Error {
            fmt::format("An error occurred while parsing the JSON file '{}': JSON is not an object", path.c_str())};
    }

    entries = fileContentsJson.getObject().value();

    for (const auto& [key, value] : entries)
    {
        auto status = m_pRocksDB->Put(rocksdb::WriteOptions(), cfHandle.get(), key, value.str());
        if (!status.ok())
        {
            return base::Error {fmt::format(
                "An error occurred while inserting data key {}, value {}: ", key, value.str(), status.ToString())};
        }
    }

    return std::nullopt;
}

std::optional<base::Error> KVDBManager::createDB(const std::string& name)
{
    if (existsDB(name))
    {
        return std::nullopt;
    }

    return createColumnFamily(name);
}

bool KVDBManager::existsDB(const std::string& name)
{
    return m_mapCFHandles.count(name) > 0;
}

std::map<std::string, kvdbManager::RefInfo> KVDBManager::getKVDBScopesInfo()
{
    // List reverse lookup of getKVDBHandlersInfo. List of scopes and DBs that are using them.
    std::map<std::string, kvdbManager::RefInfo> retValue;

    // Retrieve the list of DBs and scopes that are using them.
    std::map<std::string, kvdbManager::RefInfo> handlersInfo = getKVDBHandlersInfo();

    // Create a temporal map with the reverse lookup indexed by Scope instead of Database.
    std::map<std::string, kvdbManager::RefCounter> refCounterMap;

    // Iterate over the map of DBs and scopes that are using them.
    for (const auto& [dbName, scopesUsingDB] : handlersInfo)
    {
        // Iterate over the scopes that are using thisDB.
        for (const auto& [scopeName, countDBsUsingScope] : scopesUsingDB)
        {
            // Get the current refCounter for this scope.
            auto& counterMap = refCounterMap[scopeName];

            // Insert number of used DBs in current scope.
            counterMap.addRef(dbName, countDBsUsingScope);

            // Update the refCounter for this scope.
            refCounterMap[scopeName] = counterMap;
        }
    }

    for (auto& entry : refCounterMap)
    {
        const auto& scopeName = entry.first;
        const auto& refCounter = entry.second;
        const auto& refInfo = refCounter.getRefMap();
        retValue.emplace(scopeName, refInfo);
    }

    return retValue;
}

std::map<std::string, kvdbManager::RefInfo> KVDBManager::getKVDBHandlersInfo() const
{
    // List of DBs and the scopes referencing them.
    std::map<std::string, kvdbManager::RefInfo> retValue;
    auto dbNames = m_kvdbHandlerCollection->getDBNames();
    for (const auto& dbName : dbNames)
    {
        auto refInfo = m_kvdbHandlerCollection->getRefMap(dbName);
        retValue.insert(std::make_pair(dbName, refInfo));
    }
    return retValue;
}

std::optional<base::Error> KVDBManager::createColumnFamily(const std::string& name)
{
    rocksdb::ColumnFamilyHandle* cfHandle {nullptr};
    rocksdb::Status s {m_pRocksDB->CreateColumnFamily(rocksdb::ColumnFamilyOptions(), name, &cfHandle)};

    if (s.ok())
    {
        m_mapCFHandles.emplace(name, createSharedCFHandle(cfHandle));
        return std::nullopt;
    }

    return base::Error {fmt::format("Could not create DB {}, RocksDB Status: {}", name, s.ToString())};
}

std::shared_ptr<rocksdb::ColumnFamilyHandle> KVDBManager::createSharedCFHandle(rocksdb::ColumnFamilyHandle* cfRawPtr)
{
    return std::shared_ptr<rocksdb::ColumnFamilyHandle>(
        cfRawPtr,
        [pRocksDB = m_pRocksDB](rocksdb::ColumnFamilyHandle* ptr)
        {
            auto opStatus = pRocksDB->DestroyColumnFamilyHandle(ptr);
            if (!opStatus.ok())
            {
                throw std::runtime_error(
                    fmt::format("An error occurred while trying to destroy CF: {}", opStatus.ToString()));
            }
        });
}

} // namespace kvdbManager
