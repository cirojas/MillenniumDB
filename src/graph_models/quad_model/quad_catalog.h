#pragma once

#include "storage/catalog/catalog.h"
#include "storage/index/hnsw/hnsw_index_manager.h"
#include "storage/index/text_search/text_index_manager.h"

#include <map>
#include <ostream>
#include <shared_mutex>
#include <string>

namespace Import { namespace QuadModel {
class OnDiskImport;
namespace CSV {
class OnDiskImport;
}
}} // namespace Import::QuadModel

class QuadCatalog : public Catalog {
    // friend class Import::QuadModel::OnDiskImport;
    // friend class Import::QuadModel::CSV::OnDiskImport;

    struct CountOffset {
        uint64_t count;
        uint64_t offset;
    };

private:
    mutable std::shared_mutex mutex;

    static constexpr size_t MAX_ANON_OFFSET = VERSION_HEADER_SIZE;
    static constexpr size_t MAX_EDGE_OFFSET = MAX_ANON_OFFSET + sizeof(uint64_t);
    static constexpr size_t DELETED_EDGES_OFFSET = MAX_EDGE_OFFSET + sizeof(uint64_t);
    static constexpr size_t NODES_COUNT_OFFSET = DELETED_EDGES_OFFSET + sizeof(uint64_t);
    static constexpr size_t NODE_LABELS_COUNT_OFFSET = NODES_COUNT_OFFSET + sizeof(uint64_t);
    static constexpr size_t NODE_PROPERTIES_COUNT_OFFSET = NODE_LABELS_COUNT_OFFSET + sizeof(uint64_t);
    static constexpr size_t EDGE_PROPERTIES_COUNT_OFFSET = NODE_PROPERTIES_COUNT_OFFSET + sizeof(uint64_t);
    static constexpr size_t EQUAL_FROM_TO_COUNT_OFFSET = EDGE_PROPERTIES_COUNT_OFFSET + sizeof(uint64_t);

    // there may be gaps in anons, this number is the upper bound
    // meaning each anon is strictly less this this number
    uint64_t max_anon;

    // there may be gaps in edges, this number is the upper bound
    // meaning each edge is strictly less this this number
    uint64_t max_edge;

    // existing edges is max_edge minus deleted_edges
    uint64_t deleted_edges;

    uint64_t nodes_count;

    // total of pairs <node,label> in the graph
    uint64_t node_labels_count;

    // total of tuples <node,key,val> in the graph
    uint64_t node_properties_count;

    // total of tuples <edge,key,val> in the graph
    uint64_t edge_properties_count;

    // total of tuples <n,n,label,edge> in the graph
    uint64_t equal_from_to_count;

    std::vector<std::string> node_labels_str;
    boost::unordered_flat_map<std::string, uint64_t> node_labels2id;

    std::vector<std::string> edge_labels_str;
    boost::unordered_flat_map<std::string, uint64_t> edge_labels2id;

    std::vector<std::string> keys_str;
    boost::unordered_flat_map<std::string, uint64_t> keys2id;

    boost::unordered_flat_map<ObjectId, CountOffset, OIDHasher> node_label2total_count;
    boost::unordered_flat_map<ObjectId, CountOffset, OIDHasher> node_key2total_count;
    boost::unordered_flat_map<ObjectId, CountOffset, OIDHasher> edge_key2total_count;
    boost::unordered_flat_map<ObjectId, CountOffset, OIDHasher> edge_label2total_count;
    boost::unordered_flat_map<ObjectId, CountOffset, OIDHasher> edge_label2equal_from_to_count;

public:
    static constexpr uint8_t MODEL_ID = 0;
    static constexpr uint8_t MAJOR_VERSION = 5;
    static constexpr uint8_t MINOR_VERSION = 0;

    TextSearch::TextIndexManager text_index_manager;
    HNSW::HNSWIndexManager hnsw_index_manager;

    QuadCatalog(const std::string& filename);

    void print(std::ostream&) const;

    bool index_name_exists(const std::string& index_name);

    // return how many edges are in the database
    uint64_t get_edges_count() const
    {
        return max_edge - deleted_edges;
    }

    // return how many nodes are in the database
    uint64_t get_nodes_count() const
    {
        return nodes_count;
    }

    uint64_t get_distinct_keys_count() const
    {
        return keys_str.size();
    }

    uint64_t get_distinct_edge_labels() const
    {
        return edge_labels_str.size();
    }

    uint64_t get_distinct_node_labels() const
    {
        return node_labels_str.size();
    }

    uint64_t get_max_anon() const
    {
        return max_anon;
    }

    uint64_t get_max_edge() const
    {
        return max_edge;
    }

    std::string get_key(uint64_t id);
    std::string get_node_label(uint64_t id);
    std::string get_edge_label(uint64_t id);

    ObjectId get_key_id(const std::string& str);
    ObjectId get_node_label_id(const std::string& str);
    ObjectId get_edge_label_id(const std::string& str);

    uint64_t get_edge_label_count(ObjectId label) const;
    uint64_t get_node_label_count(ObjectId label) const;
    uint64_t get_node_property_count(ObjectId key) const;
    uint64_t get_edge_property_count(ObjectId key) const;
    uint64_t get_equal_from_to_edge_label_count(ObjectId label) const;

    uint64_t get_node_labels_count() const;
    uint64_t get_edge_properties_count() const;
    uint64_t get_node_properties_count() const;
    uint64_t get_equal_from_to_edge_count() const;

    void create_new_edge_labels(const std::map<std::string, ObjectId>&);
    void create_new_node_labels(const std::map<std::string, ObjectId>&);
    void create_new_keys(const std::map<std::string, ObjectId>&);

    void update_node_key_count(ObjectId key, int diff);
    void update_edge_key_count(ObjectId key, int diff);
    void update_node_label_count(ObjectId label, int diff);
    void update_edge_label_count(ObjectId label, int diff);
    void update_equal_from_to_label_count(ObjectId label, int diff);

    void update_max_anon(uint64_t new_value);
    void update_max_edge(uint64_t new_value);
    void update_deleted_edges(int diff);

    void update_nodes_count(int diff);
    void update_nodes_labels_count(int diff);
    void update_nodes_properties_count(int diff);
    void update_edge_properties_count(int diff);
    void update_equal_from_to_count(int diff);

    void process_import_keys_labels(
        const boost::unordered_flat_map<std::string, uint64_t>& keys2id,
        const boost::unordered_flat_map<std::string, uint64_t>& node_labels2id,
        const boost::unordered_flat_map<std::string, uint64_t>& edge_labels2id
    );
    void process_import_node(uint64_t nodes_count, uint64_t max_anon);
    void process_import_node_labels(
        uint64_t node_labels_count,
        const boost::unordered_flat_map<uint64_t, uint64_t>& node_label2total_count
    );
    void process_import_node_keys(
        uint64_t node_properties_count,
        const boost::unordered_flat_map<uint64_t, uint64_t>& node_key2total_count
    );
    void process_import_edge_keys(
        uint64_t edge_properties_count,
        const boost::unordered_flat_map<uint64_t, uint64_t>& edge_key2total_count
    );
    void process_import_edges(
        uint64_t max_edge,
        const boost::unordered_flat_map<uint64_t, uint64_t>& edge_label2total_count
    );
    void process_import_equal_from_to(
        uint64_t equal_from_to_count,
        const boost::unordered_flat_map<uint64_t, uint64_t>& edge_label2equal_from_to_count
    );

    void flush_changes();

    // TODO: use something like this to create catalog at import
    static void create_new_catalog();
};
