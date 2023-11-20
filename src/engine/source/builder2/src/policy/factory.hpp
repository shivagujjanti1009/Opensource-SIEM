#ifndef _BUILDER_POLICY_FACTORY_HPP
#define _BUILDER_POLICY_FACTORY_HPP

#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <fmt/format.h>

#include <expression.hpp>
#include <graph.hpp>
#include <store/istore.hpp>

#include "builders/iregistry.hpp"
#include "iassetBuilder.hpp"

namespace builder::policy::factory
{

/**
 * @brief Contains each of the assets for a given type and the default parents.
 *
 */
struct SubgraphData
{
    std::unordered_map<store::NamespaceId, base::Name> defaultParents;
    std::unordered_map<store::NamespaceId, std::unordered_set<base::Name>> assets;

    friend bool operator==(const SubgraphData& lhs, const SubgraphData& rhs)
    {
        return lhs.defaultParents == rhs.defaultParents && lhs.assets == rhs.assets;
    }
};

/**
 * @brief Contains the policy data needed to build the policy.
 *
 */
class PolicyData
{
public:
    enum class AssetType // Defines order
    {
        DECODER = 0,
        RULE,
        OUTPUT,
        FILTER // TODO implement filters as a separate asset type
    };

    static constexpr auto assetTypeStr(AssetType type)
    {
        switch (type)
        {
            case AssetType::DECODER: return "decoder";
            case AssetType::RULE: return "rule";
            case AssetType::OUTPUT: return "output";
            case AssetType::FILTER: return "filter";
            default: throw std::runtime_error("Invalid asset type");
        }
    }

private:
    base::Name m_name;                                       ///< Policy name
    std::string m_hash;                                      ///< Policy hash
    std::unordered_map<AssetType, SubgraphData> m_subgraphs; ///< Subgraph data by type

public:
    struct Params
    {
        base::Name name;
        std::string hash;
        std::unordered_map<store::NamespaceId, base::Name> defaultParents;
        std::unordered_map<AssetType, std::unordered_map<store::NamespaceId, std::unordered_set<base::Name>>> assets;
    };

    PolicyData() = default;
    PolicyData(const Params& params)
    {
        m_name = params.name;
        m_hash = params.hash;

        for (const auto& [ns, name] : params.defaultParents)
        {
            addDefaultParent(AssetType::DECODER, ns, name);
        }

        for (const auto& [assetType, subgraph] : params.assets)
        {
            for (const auto& [ns, assets] : subgraph)
            {
                for (const auto& name : assets)
                {
                    add(assetType, ns, name);
                }
            }
        }
    }

    /**
     * @brief Add asset to the policy data.
     *
     * @param assetType Type of the asset.
     * @param ns Namespace of the asset.
     * @param name Name of the asset.
     * @return true if the asset was added.
     * @return false if the asset was already present.
     */
    inline bool add(AssetType assetType, const store::NamespaceId& ns, const base::Name& name)
    {
        auto subgraphIt = m_subgraphs.find(assetType);
        if (subgraphIt == m_subgraphs.end())
        {
            m_subgraphs.emplace(assetType, SubgraphData {});
            subgraphIt = m_subgraphs.find(assetType);
        }
        auto& subgraph = subgraphIt->second;
        auto nsIt = subgraph.assets.find(ns);
        if (nsIt == subgraph.assets.end())
        {
            subgraph.assets.emplace(ns, std::unordered_set<base::Name> {name});
        }
        else
        {
            if (nsIt->second.find(name) != nsIt->second.end())
            {
                return false;
            }
            nsIt->second.emplace(name);
        }

        return true;
    }

    /**
     * @brief Add default parent to the policy data.
     *
     * @param assetType Type of the asset.
     * @param ns Namespace of the asset.
     * @param name Name of the asset.
     * @return true if the default parent was added.
     * @return false if the default parent was already present.
     */
    bool addDefaultParent(AssetType assetType, const store::NamespaceId& ns, const base::Name& name)
    {
        auto subgraphIt = m_subgraphs.find(assetType);
        if (subgraphIt == m_subgraphs.end())
        {
            m_subgraphs.emplace(assetType, SubgraphData {});
            subgraphIt = m_subgraphs.find(assetType);
        }
        auto& subgraph = subgraphIt->second;
        auto nsIt = subgraph.defaultParents.find(ns);
        if (nsIt == subgraph.defaultParents.end())
        {
            subgraph.defaultParents.emplace(ns, name);
            return true;
        }
        return false;
    }

    const base::Name& name() const { return m_name; }
    base::Name& name() { return m_name; }

    const std::string& hash() const { return m_hash; }
    std::string& hash() { return m_hash; }

    const std::unordered_map<AssetType, SubgraphData>& subgraphs() const { return m_subgraphs; }
};

/**
 * @brief Add the subgraph of the integration to the policy data.
 *
 * @param assetType Asset type of the subgraph.
 * @param path Path to the asset list in the integration document.
 * @param integrationDoc Integration document.
 * @param store The store interface to query namespaces.
 * @param integrationName Name of the integration.
 * @param integrationNs Namespace of the integration.
 * @param data Policy data to add the subgraph.
 *
 * @throw std::runtime_error If any error occurs.
 */
void addIntegrationSubgraph(PolicyData::AssetType assetType,
                            const std::string& path,
                            const store::Doc& integrationDoc,
                            const std::shared_ptr<store::IStoreReader>& store,
                            const base::Name& integrationName,
                            const store::NamespaceId& integrationNs,
                            PolicyData& data);

/**
 * @brief Query the store to add the assets of the integration to the policy data.
 *
 * @param integrationNs Namespace of the integration.
 * @param name Name of the integration.
 * @param data Policy data to add the assets.
 * @param store The store interface to query assets and namespaces.
 */
void addIntegrationAssets(const store::NamespaceId& integrationNs,
                          const base::Name& name,
                          PolicyData& data,
                          const std::shared_ptr<store::IStoreReader>& store);

/**
 * @brief Read the policy data from the policy document
 *
 * @param doc The policy document.
 * @param store The store interface to query asset namespace.
 * @return PolicyData The policy data.
 *
 * @throw std::runtime_error If the policy data is invalid.
 */
PolicyData readData(const store::Doc& doc, const std::shared_ptr<store::IStoreReader>& store);

/**
 * @brief This struct contains the built assets of the policy by type.
 *
 */
using BuiltAssets = std::unordered_map<PolicyData::AssetType, std::unordered_map<base::Name, Asset>>;

BuiltAssets buildAssets(const PolicyData& data,
                        const std::shared_ptr<store::IStoreReader> store,
                        const std::shared_ptr<IAssetBuilder>& assetBuilder);

struct PolicyGraph
{
    std::map<PolicyData::AssetType, Graph<base::Name, Asset>> subgraphs;
    // TODO: Implement
    inline std::string getGraphivzStr() const { throw std::runtime_error("Not implemented"); }

    friend bool operator==(const PolicyGraph& lhs, const PolicyGraph& rhs) { return lhs.subgraphs == rhs.subgraphs; }
};

Graph<base::Name, Asset> buildSubgraph(const std::string& subgraphName,
                                       const SubgraphData& subgraphData,
                                       const SubgraphData& filtersData,
                                       const std::unordered_map<base::Name, Asset>& assets,
                                       const std::unordered_map<base::Name, Asset>& filters);

PolicyGraph buildGraph(const BuiltAssets& assets, const PolicyData& data);

template<typename ChildOperator>
base::Expression buildSubgraphExpression(const Graph<base::Name, Asset>& subgraph)
{
    // Assert T is a valid operation
    static_assert(std::is_base_of_v<base::Operation, ChildOperator>, "ChildOperator must be a valid operation");

    auto root = ChildOperator::create(subgraph.node(subgraph.rootId()).name(), {});

    // Avoid duplicating nodes when multiple parents has the same child node
    std::map<std::string, base::Expression> builtNodes;

    // parentNode Expression is passed as filters need it.
    auto visit = [&](const std::string& current, const std::string& parent, auto& visitRef) -> base::Expression
    {
        // If node is already built, return it
        if (builtNodes.find(current) != builtNodes.end())
        {
            return builtNodes[current];
        }
        else
        {
            // Create node
            // If node has children, create an auxiliary Implication node, with
            // asset as condition and children as consequence, otherwise create an
            // asset node.
            auto asset = subgraph.node(current);
            std::shared_ptr<base::Operation> assetNode;

            if (subgraph.hasChildren(current))
            {
                auto assetChildren = ChildOperator::create(asset.name() + "Children", {});

                assetNode = base::Implication::create(asset.name() + "Node", asset.expression(), assetChildren);

                // Visit children and add them to the children node
                for (auto& child : subgraph.children(current))
                {
                    assetChildren->getOperands().push_back(visitRef(child, current, visitRef));
                }
            }
            else
            {
                // No children
                assetNode = asset.expression()->getPtr<base::Operation>();
            }

            // Add it to builtNodes
            if (asset.parents().size() > 1)
            {
                builtNodes.insert(std::make_pair(current, assetNode));
            }

            return assetNode;
        }
    };

    // Visit root childs and add them to the root expression
    for (auto& child : subgraph.children(subgraph.rootId()))
    {
        root->getOperands().push_back(visit(child, subgraph.rootId(), visit));
    }

    return root;
}

base::Expression buildExpression(const PolicyGraph& graph, const PolicyData& data);

} // namespace builder::policy::factory

#endif // _BUILDER_POLICY_FACTORY_HPP
