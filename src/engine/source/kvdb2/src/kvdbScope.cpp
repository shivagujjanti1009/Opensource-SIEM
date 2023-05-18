#include <kvdb2/kvdbScope.hpp>
#include <kvdb2/iKVDBHandlerManager.hpp>

namespace kvdbManager
{

KVDBHandler KVDBScope::getKVDBHandler(const std::string& dbName)
{
    return m_handlerManager->getKVDBHandler(dbName, m_name);
}

} // namespace kvdbManager
