#ifndef _KVDB_MANAGER_H
#define _KVDB_MANAGER_H

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>

#include <rocksdb/db.h>
#include <rocksdb/options.h>

#include <error.hpp>

#include <kvdb/iKVDBManager.hpp>
#include <kvdb/kvdbHandler.hpp>
#include <kvdb/kvdbHandlerCollection.hpp>

namespace metricsManager
{
class IMetricsManager;
class IMetricsScope;
} // namespace metricsManager

namespace kvdbManager
{

constexpr static const char* DEFAULT_CF_NAME {"default"};

/**
 * @brief Options for the KVDBManager.
 *
 */
struct KVDBManagerOptions
{
    std::filesystem::path dbStoragePath;
    std::string dbName;
};

/**
 * @brief KVDBManager Entry Point class.
 *
 */
class KVDBManager final : public IKVDBManager
{
    WAZUH_DISABLE_COPY_ASSIGN(KVDBManager);

public:
    /**
     * @brief Construct a new KVDBManager object
     *
     * @param options Options for the KVDBManager.
     * @param metricsManager Pointer to the Metrics Manager.
     */
    KVDBManager(const KVDBManagerOptions& options,
                const std::shared_ptr<metricsManager::IMetricsManager>& metricsManager);

    /**
     * @brief Initialize the KVDBManager.
     * Setup options, filesystem, RocksDB internals, etc.
     *
     */
    void initialize();

    /**
     * @brief Finalize the KVDBManager.
     *
     */
    void finalize();

    /**
     * @copydoc IKVDBManager::getKVDBScopesInfo
     *
     */
    std::map<std::string, RefInfo> getKVDBScopesInfo() override;

    /**
     * @copydoc IKVDBManager::getKVDBHandlersInfo
     *
     */
    std::map<std::string, RefInfo> getKVDBHandlersInfo() const override;

    /**
     * @copydoc IKVDBManager::getKVDBHandlersCount
     *
     */
    uint32_t getKVDBHandlersCount(const std::string& dbName) const override;

    /**
     * @copydoc IKVDBManager::getKVDBHandler
     *
     */
    std::variant<std::shared_ptr<IKVDBHandler>, base::Error> getKVDBHandler(const std::string& dbName,
                                                                            const std::string& scopeName) override;

    /**
     * @copydoc IKVDBManager::listDBs
     *
     */
    std::vector<std::string> listDBs(const bool loaded) override;

    /**
     * @copydoc IKVDBManager::deleteDB
     *
     */
    std::optional<base::Error> deleteDB(const std::string& name) override;

    /**
     * @copydoc IKVDBManager::createDB
     *
     */
    std::optional<base::Error> createDB(const std::string& name) override;

    /**
     * @copydoc IKVDBManager::loadDBFromFile
     *
     */
    std::optional<base::Error> loadDBFromFile(const std::string& name, const std::string& path) override;

    /**
     * @copydoc IKVDBManager::existsDB
     *
     */
    bool existsDB(const std::string& name) override;

private:
    /**
     * @brief Setup RocksDB Options. Populate m_rocksDBOptions with the default values.
     *
     */
    void initializeOptions();

    /**
     * @brief Initialize the Main DB. Setup Filesystem, open RocksDB, create initial maps.
     *
     */
    void initializeMainDB();

    /**
     * @brief Finalize the Main DB. Close RocksDB, destroy maps.
     *
     */
    void finalizeMainDB();

    /**
     * @brief Create a Shared Column Family Shared Pointer with custom delete function.
     *
     * @param cfRawPtr Raw pointer to the Column Family Handle.
     * @return std::shared_ptr<rocksdb::ColumnFamilyHandle> Shared Pointer to the Column Family Handle.
     */
    std::shared_ptr<rocksdb::ColumnFamilyHandle> createSharedCFHandle(rocksdb::ColumnFamilyHandle* cfRawPtr);

    /**
     * @brief Custom Collection Object to wrap maps, searchs, references, related to handlers and scopes.
     *
     */
    std::shared_ptr<KVDBHandlerCollection> m_kvdbHandlerCollection;

    /**
     * @brief Pointer to the Metrics Manager through MetricsScope.
     *
     */
    std::shared_ptr<metricsManager::IMetricsScope> m_spMetricsScope;

    /**
     * @brief Create a Column Family object and store in map.
     *
     * @param name Name of the DB -> mapped to Column Family.
     * @return std::optional<base::Error> Specific error.
     */
    std::optional<base::Error> createColumnFamily(const std::string& name);

    /**
     * @brief Options the Manager was built with.
     *
     */
    KVDBManagerOptions m_ManagerOptions;

    /**
     * @brief Internal representation of RocksDB Options.
     *
     */
    rocksdb::Options m_rocksDBOptions;

    /**
     * @brief Internal rocksdb::DB object. This is the main object through which all operations are done.
     *
     */
    std::shared_ptr<rocksdb::DB> m_pRocksDB;
    /**
     * @brief Internal map of Column Family Handles.
     * This is the loaded CFs or KVDBs.
     *
     */
    std::map<std::string, std::shared_ptr<rocksdb::ColumnFamilyHandle>> m_mapCFHandles;

    /**
     * @brief Default Column Family Handle
     *
     */
    std::shared_ptr<rocksdb::ColumnFamilyHandle> m_pDefaultCFHandle;

    /**
     * @brief Syncronization object for Scopes Collection (m_mapScopes).
     *
     */
    std::mutex m_mutexScopes;

    // TODO: Check lock of functions where these states are changed/checked.
    /**
     * @brief Flag bool variable to indicate if the Manager is initialized.
     *
     */
    std::atomic<bool> m_isInitialized {false};
};

} // namespace kvdbManager

#endif // _KVDB_MANAGER_H
