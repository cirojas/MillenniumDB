#pragma once

#include <ostream>
#include <string>

#include "storage/catalog/catalog.h"
#include "storage/index/hnsw/hnsw_index_manager.h"
#include "storage/index/text_search/text_index_manager.h"

namespace Import { namespace QuadModel {
class OnDiskImport;
namespace CSV {
class OnDiskImport;
}
}} // namespace Import::QuadModel

class QuadCatalog : public Catalog {
    friend class Import::QuadModel::OnDiskImport;
    friend class Import::QuadModel::CSV::OnDiskImport;

private:
    // existing edges is max_edge minus deleted_edges
    uint64_t deleted_edges;

    uint64_t nodes_count;
    uint64_t node_labels_count;
    uint64_t node_properties_count;
    uint64_t edge_properties_count;

    uint64_t equal_from_to_count;

    std::vector<std::string> node_labels_str;
    boost::unordered_flat_map<std::string, uint64_t> node_labels2id;

    std::vector<std::string> edge_labels_str;
    boost::unordered_flat_map<std::string, uint64_t> edge_labels2id;

    std::vector<std::string> keys_str;
    boost::unordered_flat_map<std::string, uint64_t> keys2id;

    boost::unordered_flat_map<uint64_t, uint64_t> node_label2total_count;

    boost::unordered_flat_map<uint64_t, uint64_t> node_key2total_count;
    boost::unordered_flat_map<uint64_t, uint64_t> edge_key2total_count;

    boost::unordered_flat_map<uint64_t, uint64_t> edge_label2total_count;
    boost::unordered_flat_map<uint64_t, uint64_t> edge_label2equal_from_to_count;

public:
    static constexpr uint8_t MODEL_ID = 0;
    static constexpr uint8_t MAJOR_VERSION = 4;
    static constexpr uint8_t MINOR_VERSION = 0;

    TextSearch::TextIndexManager text_index_manager;
    HNSW::HNSWIndexManager hnsw_index_manager;

        // there may be gaps in anons, this number is the upper bound
    // meaning each anon is strictly less this this number
    uint64_t max_anon;

    // there may be gaps in edges, this number is the upper bound
    // meaning each edge is strictly less this this number
    uint64_t max_edge;

    QuadCatalog(const std::string& filename);

    ~QuadCatalog();

    void print(std::ostream&) const;
    // void save();

    bool index_name_exists(const std::string& index_name) const;

    // return how many edges are in the database
    // TODO: is this completely accurate or an estimate that can be wrong if concurrent update
    // is occurring?
    uint64_t get_edges_count() const
    {
        return max_edge - deleted_edges;
    }

    // return how many nodes are in the database
    // TODO: is this completely accurate or an estimate that can be wrong if concurrent update
    // is occurring?
    uint64_t get_nodes_count() const
    {
        return nodes_count;
    }

    // auto it = quad_model.catalog.edge_label2equal_from_to_count.find(label.get_OID().id);
    // if (it != quad_model.catalog.edge_label2equal_from_to_count.end()) {
    //     auto count = static_cast<double>(it->second);
    //     return count / heuristic_divisor;
    // } else {
    //     return 0;
    // }
    // auto it = quad_model.catalog.edge_label2total_count.find(label.get_OID().id);
    // if (it != quad_model.catalog.edge_label2total_count.end()) {
    //     auto count = static_cast<double>(it->second);
    //     return count / heuristic_divisor;
    // } else {
    //     return 0;
    // }

    uint64_t get_edge_label_count(ObjectId label) const;
    uint64_t get_node_label_count(ObjectId label) const;
    uint64_t get_node_property_count(ObjectId key) const;
    uint64_t get_edge_property_count(ObjectId key) const;
    uint64_t get_equal_from_to_edge_label_count(ObjectId label) const;

    uint64_t get_node_labels_count() const;
    uint64_t get_edge_properties_count() const;
    uint64_t get_node_properties_count() const;
    uint64_t get_equal_from_to_edge_count() const;

    // TODO: use something like this to create catalog at import
    static void create_new_catalog();
};
