#pragma once

#include <ostream>
#include <string>

#include "storage/catalog/catalog.h"
#include "storage/index/hnsw/hnsw_index_manager.h"
#include "storage/index/text_search/text_index_manager.h"

class QuadCatalog : public Catalog {
public:
    static constexpr uint8_t MODEL_ID = 0;
    static constexpr uint8_t MAJOR_VERSION = 4;
    static constexpr uint8_t MINOR_VERSION = 0;

    QuadCatalog(const std::string& filename);

    ~QuadCatalog();

    void print(std::ostream&);
    void save();

    // uint64_t edges_with_label(uint64_t type_id) const;
    // uint64_t equal_from_to_with_label(uint64_t type_id) const;

    bool index_name_exists(const std::string& index_name);

    uint64_t edge_count() const
    {
        return max_edge - deleted_edges;
    }

    // there may be gaps in anons, this number is the upper bound
    // meaning each anon is strictly less this this number
    uint64_t max_anon;

    // there may be gaps in edges, this number is the upper bound
    // meaning each edge is strictly less this this number
    uint64_t max_edge;

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

    TextSearch::TextIndexManager text_index_manager;
    HNSW::HNSWIndexManager hnsw_index_manager;
};
