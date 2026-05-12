#include "quad_catalog.h"

#include "query/exceptions.h"

#include <mutex>
// #include "storage/index/text_search/quad.h"

// using namespace std;

enum class CatalogInfo {
    node_label = 1, // <label_id, strlen, label_str>
    edge_label = 2, // <label_id, strlen, label_str>
    key = 3, //  <key_id, strlen, label_str>
    node_label_stat = 4, // <label_id, count>
    edge_label_stat = 5, // <label_id, count>
    equal_from_to_stat = 6, // <label_id, count>
    node_key_stat = 7, // <key_id, count>
    edge_key_stat = 8, // <key_id, count>
    hnsw_index = 9, // <idx_name_len, idx_name, norm_type, token_type, pred_id, pred_len, pred_str>
    text_index = 10, // <idx_name_len, idx_name, metric_type, pred_len, pred_str>
};

//         const auto text_index_name2metadata_size = read_uint64();
//         for (uint_fast32_t i = 0; i < text_index_name2metadata_size; ++i) {
//             const auto name = read_string();
//             TextSearch::TextIndexManager::TextIndexMetadata metadata;
//             metadata.normalization_type = static_cast<TextSearch::NORMALIZE_TYPE>(read_uint8());
//             metadata.tokenization_type = static_cast<TextSearch::TOKENIZE_TYPE>(read_uint8());
//             metadata.predicate_id = ObjectId(read_uint64());
//             metadata.predicate = read_string();
//             text_index_manager.load_text_index(name, metadata);
//         }

//         hnsw_index_manager.init();
//         const auto hnsw_index_name2metadata_size = read_uint64();
//         for (uint_fast32_t i = 0; i < hnsw_index_name2metadata_size; ++i) {
//             const auto name = read_string();
//             HNSW::HNSWIndexManager::HNSWIndexMetadata metadata;
//             metadata.metric_type = static_cast<HNSW::MetricType>(read_uint8());
//             metadata.predicate = read_string();
//             hnsw_index_manager.load_hnsw_index(name, metadata);
//         }

QuadCatalog::QuadCatalog(const std::string& filename) :
    Catalog(filename)
{
    if (is_empty()) {
        // TODO: allow empty catalog?
    } else {
        auto diff_minor_version = check_version("Quad", MODEL_ID, MAJOR_VERSION, MINOR_VERSION);

        if (diff_minor_version != 0) {
            throw LogicException("Undefined catalog recovery");
        }

        max_anon = read_uint64();
        max_edge = read_uint64();
        deleted_edges = read_uint64();

        nodes_count = read_uint64();
        node_labels_count = read_uint64();
        node_properties_count = read_uint64();
        edge_properties_count = read_uint64();
        equal_from_to_count = read_uint64();

        // TODO: reserve space in vectors

        char byte;
        while (file.get(byte)) {
            switch (CatalogInfo(byte)) {
            case CatalogInfo::node_label:
            case CatalogInfo::edge_label:
            case CatalogInfo::key:
            case CatalogInfo::node_label_stat:
            case CatalogInfo::edge_label_stat:
            case CatalogInfo::equal_from_to_stat:
            case CatalogInfo::node_key_stat:
            case CatalogInfo::edge_key_stat:
            case CatalogInfo::hnsw_index:
            case CatalogInfo::text_index:
                break;
            }
        }
    }
}
//     if (is_empty()) {
//         max_anon = 0;
//         max_edge = 0;
//         deleted_edges = 0;

//         nodes_count = 0;
//         label_count = 0;
//         properties_count = 0;

//         equal_from_to_count = 0;

//         has_changes = true;
//     } else {
//         auto diff_minor_version = check_version("Quad", MODEL_ID, MAJOR_VERSION, MINOR_VERSION);

//         if (diff_minor_version != 0) {
//             throw LogicException("Undefined catalog recovery");
//         }

//         max_anon = read_uint64();
//         max_edge = read_uint64();
//         deleted_edges = read_uint64();

//         nodes_count = read_uint64();
//         label_count = read_uint64();
//         properties_count = read_uint64();

//         equal_from_to_count = read_uint64();

//         const auto distinct_labels = read_uint64();
//         for (uint_fast32_t i = 0; i < distinct_labels; i++) {
//             auto label_id = read_uint64();
//             auto label_total_count = read_uint64();
//             label2total_count.insert({ label_id, label_total_count });
//         }

//         const auto distinct_keys = read_uint64();
//         for (uint_fast32_t i = 0; i < distinct_keys; i++) {
//             auto key_id = read_uint64();
//             auto key_total_count = read_uint64();
//             key2total_count.insert({ key_id, key_total_count });
//         }

//         const auto distinct_types = read_uint64();
//         for (uint_fast32_t i = 0; i < distinct_types; i++) {
//             auto type_id = read_uint64();
//             auto type_total_count = read_uint64();
//             type2total_count.insert({ type_id, type_total_count });
//         }

//         const auto type2equal_from_to_type_count_size = read_uint64();
//         for (uint_fast32_t i = 0; i < type2equal_from_to_type_count_size; i++) {
//             auto type = read_uint64();
//             auto count = read_uint64();
//             type2equal_from_to_type_count.insert({ type, count });
//         }

//         const auto type2equal_from_to_count_size = read_uint64();
//         for (uint_fast32_t i = 0; i < type2equal_from_to_count_size; i++) {
//             auto type = read_uint64();
//             auto count = read_uint64();
//             type2equal_from_to_count.insert({ type, count });
//         }

//         const auto type2equal_from_type_count_size = read_uint64();
//         for (uint_fast32_t i = 0; i < type2equal_from_type_count_size; i++) {
//             auto type = read_uint64();
//             auto count = read_uint64();
//             type2equal_from_type_count.insert({ type, count });
//         }

//         const auto type2equal_to_type_count_size = read_uint64();
//         for (uint_fast32_t i = 0; i < type2equal_to_type_count_size; i++) {
//             auto type = read_uint64();
//             auto count = read_uint64();
//             type2equal_to_type_count.insert({ type, count });
//         }

//         text_index_manager.init(
//             TextSearch::Quad::index_predicate,
//             TextSearch::Quad::index_single,
//             TextSearch::Quad::remove_single,
//             TextSearch::Quad::oid_to_string
//         );
//         const auto text_index_name2metadata_size = read_uint64();
//         for (uint_fast32_t i = 0; i < text_index_name2metadata_size; ++i) {
//             const auto name = read_string();
//             TextSearch::TextIndexManager::TextIndexMetadata metadata;
//             metadata.normalization_type = static_cast<TextSearch::NORMALIZE_TYPE>(read_uint8());
//             metadata.tokenization_type = static_cast<TextSearch::TOKENIZE_TYPE>(read_uint8());
//             metadata.predicate_id = ObjectId(read_uint64());
//             metadata.predicate = read_string();
//             text_index_manager.load_text_index(name, metadata);
//         }

//         hnsw_index_manager.init();
//         const auto hnsw_index_name2metadata_size = read_uint64();
//         for (uint_fast32_t i = 0; i < hnsw_index_name2metadata_size; ++i) {
//             const auto name = read_string();
//             HNSW::HNSWIndexManager::HNSWIndexMetadata metadata;
//             metadata.metric_type = static_cast<HNSW::MetricType>(read_uint8());
//             metadata.predicate = read_string();
//             hnsw_index_manager.load_hnsw_index(name, metadata);
//         }
//     }
// }

// QuadCatalog::~QuadCatalog()
// {
//     if (has_changes || text_index_manager.has_changes() || hnsw_index_manager.has_changes()) {
//         save();
//     }
// }

// void QuadCatalog::save()
// {
//     start_write(MODEL_ID, MAJOR_VERSION, MINOR_VERSION);

//     write_uint64(max_anon);
//     write_uint64(max_edge);
//     write_uint64(deleted_edges);

//     write_uint64(nodes_count);
//     write_uint64(label_count);
//     write_uint64(properties_count);

//     write_uint64(equal_from_to_count);

//     write_uint64(label2total_count.size());
//     for (auto&& [k, v] : label2total_count) {
//         write_uint64(k);
//         write_uint64(v);
//     }

//     write_uint64(key2total_count.size());
//     for (auto&& [k, v] : key2total_count) {
//         write_uint64(k);
//         write_uint64(v);
//     }

//     write_uint64(type2total_count.size());
//     for (auto&& [k, v] : type2total_count) {
//         write_uint64(k);
//         write_uint64(v);
//     }

//     write_uint64(type2equal_from_to_type_count.size());
//     for (auto&& [k, v] : type2equal_from_to_type_count) {
//         write_uint64(k);
//         write_uint64(v);
//     }

//     write_uint64(type2equal_from_to_count.size());
//     for (auto&& [k, v] : type2equal_from_to_count) {
//         write_uint64(k);
//         write_uint64(v);
//     }

//     write_uint64(type2equal_from_type_count.size());
//     for (auto&& [k, v] : type2equal_from_type_count) {
//         write_uint64(k);
//         write_uint64(v);
//     }

//     write_uint64(type2equal_to_type_count.size());
//     for (auto&& [k, v] : type2equal_to_type_count) {
//         write_uint64(k);
//         write_uint64(v);
//     }

//     const auto& text_index_name2metadata = text_index_manager.get_name2metadata();
//     write_uint64(text_index_name2metadata.size());
//     for (const auto& [name, metadata] : text_index_name2metadata) {
//         write_string(name);
//         write_uint8(static_cast<uint8_t>(metadata.normalization_type));
//         write_uint8(static_cast<uint8_t>(metadata.tokenization_type));
//         write_uint64(metadata.predicate_id.id);
//         write_string(metadata.predicate);
//     }

//     const auto& hnsw_index_name2metadata = hnsw_index_manager.get_name2metadata();
//     write_uint64(hnsw_index_name2metadata.size());
//     for (const auto& [name, metadata] : hnsw_index_name2metadata) {
//         write_string(name);
//         write_uint8(static_cast<uint8_t>(metadata.metric_type));
//         write_string(metadata.predicate);
//     }
// }

void QuadCatalog::print(std::ostream& os) const
{
    os << "-------------------------------------\n";
    os << "Catalog:\n";
    os << "  nodes count:              " << nodes_count << "\n";
    os << "  edges count:              " << get_edges_count() << "\n";

    os << "  node labels count:        " << node_labels_count << "\n";
    os << "  node properties count:    " << node_properties_count << "\n";
    os << "  edge properties count:    " << edge_properties_count << "\n";

    os << "  distinct node labels:     " << node_label2total_count.size() << "\n";
    os << "  distinct edge labels:     " << edge_label2total_count.size() << "\n";
    os << "  distinct keys:            " << keys2id.size() << "\n";

    os << "  edges with self loop:     " << equal_from_to_count << "\n";

    const auto& text_index_name2metadata = text_index_manager.get_name2metadata();
    if (!text_index_name2metadata.empty()) {
        os << "  Text Indexes (" << text_index_name2metadata.size() << "):\n";
        for (const auto& [name, metadata] : text_index_name2metadata) {
            os << "    " << name << ": " << metadata << "\n";
        }
    }

    const auto& hnsw_index_name2metadata = hnsw_index_manager.get_name2metadata();
    if (!hnsw_index_name2metadata.empty()) {
        os << "  HNSW Indexes (" << hnsw_index_name2metadata.size() << "):\n";
        for (const auto& [name, metadata] : hnsw_index_name2metadata) {
            os << "    " << name << ": " << metadata << "\n";
        }
    }

    os << "-------------------------------------\n";
}

bool QuadCatalog::index_name_exists(const std::string& index_name)
{
    return text_index_manager.get_text_index(index_name) != nullptr
        || hnsw_index_manager.get_hnsw_index(index_name) != nullptr;
}

std::string QuadCatalog::get_key(uint64_t id)
{
    std::shared_lock lock(mutex);
    std::string res;
    if (id < keys_str.size()) {
        res = keys_str[id];
    }
    return res;
}

std::string QuadCatalog::get_node_label(uint64_t id)
{
    std::shared_lock lock(mutex);
    std::string res;
    if (id < node_labels_str.size()) {
        res = node_labels_str[id].str;
    }
    return res;
}

std::string QuadCatalog::get_edge_label(uint64_t id)
{
    std::shared_lock lock(mutex);
    std::string res;
    if (id < edge_labels_str.size()) {
        res = edge_labels_str[id];
    }
    return res;
}

ObjectId QuadCatalog::get_key_id(const std::string& str)
{
    std::shared_lock lock(mutex);
    auto it = keys2id.find(str);
    if (it != keys2id.end()) {
        return ObjectId(it->second | ObjectId::MASK_PROPERTY_KEY);
    } else {
        return ObjectId::get_not_found();
    }
}

ObjectId QuadCatalog::get_node_label_id(const std::string& str)
{
    std::shared_lock lock(mutex);
    auto it = node_labels2id.find(str);
    if (it != node_labels2id.end()) {
        return ObjectId(it->second | ObjectId::MASK_NODE_LABEL);
    } else {
        return ObjectId::get_not_found();
    }
}

ObjectId QuadCatalog::get_edge_label_id(const std::string& str)
{
    std::shared_lock lock(mutex);
    auto it = edge_labels2id.find(str);
    if (it != edge_labels2id.end()) {
        return ObjectId(it->second | ObjectId::MASK_EDGE_LABEL);
    } else {
        return ObjectId::get_not_found();
    }
}

uint64_t QuadCatalog::get_edge_label_count(ObjectId label) const
{
    std::shared_lock lock(mutex);
    auto it = edge_label2total_count.find(label);
    if (it != edge_label2total_count.end()) {
        return it->second.count;
    } else {
        return 0;
    }
}

uint64_t QuadCatalog::get_node_label_count(ObjectId label) const
{
    std::shared_lock lock(mutex);
    auto it = node_label2total_count.find(label);
    if (it != node_label2total_count.end()) {
        return it->second.count;
    } else {
        return 0;
    }
}

uint64_t QuadCatalog::get_node_property_count(ObjectId key) const
{
    std::shared_lock lock(mutex);
    auto it = node_key2total_count.find(key);
    if (it != node_key2total_count.end()) {
        return it->second.count;
    } else {
        return 0;
    }
}

uint64_t QuadCatalog::get_edge_property_count(ObjectId key) const
{
    std::shared_lock lock(mutex);
    auto it = edge_key2total_count.find(key);
    if (it != edge_key2total_count.end()) {
        return it->second.count;
    } else {
        return 0;
    }
}

uint64_t QuadCatalog::get_equal_from_to_edge_label_count(ObjectId label) const
{
    std::shared_lock lock(mutex);
    auto it = edge_label2equal_from_to_count.find(label);
    if (it != edge_label2equal_from_to_count.end()) {
        return it->second.count;
    } else {
        return 0;
    }
}

uint64_t QuadCatalog::get_node_labels_count() const
{
    return node_labels_count;
}

uint64_t QuadCatalog::get_edge_properties_count() const
{
    return edge_properties_count;
}

uint64_t QuadCatalog::get_node_properties_count() const
{
    return node_properties_count;
}

uint64_t QuadCatalog::get_equal_from_to_edge_count() const
{
    return equal_from_to_count;
}

void QuadCatalog::update_max_anon(uint64_t new_value)
{
    max_anon = new_value;
    // file.seekp(MAX_ANON_POS); // TODO:
    file.write(reinterpret_cast<const char*>(&max_anon), sizeof(max_anon));
}

void QuadCatalog::update_max_edge(uint64_t new_value)
{
    max_edge = new_value;
    // file.seekp(MAX_EDGE_POS); // TODO:
    file.write(reinterpret_cast<const char*>(&max_edge), sizeof(max_edge));
}

void QuadCatalog::update_node_key_count(ObjectId key, int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
    auto it = node_key2total_count.find(key);
    if (it != node_key2total_count.end()) {
        file.seekp(0, file.end);
        uint64_t offset = file.tellp(); // TODO: sum something to point directly?
        if (diff <= 0) {
            assert(false);
            return;
        }
        CountOffset data{static_cast<uint64_t>(diff), offset};
        node_key2total_count.insert({key, data});

        // TODO: also add new key to keys_str and keys2id if not present
    } else {
        it->second.count += diff;
        file.seekp(it->second.offset);
        file.write(reinterpret_cast<const char*>(&it->second.count), sizeof(it->second.count));
    }
}

void QuadCatalog::update_edge_key_count(ObjectId key, int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_node_label_count(ObjectId label, int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_edge_label_count(ObjectId label, int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_equal_from_to_label_count(ObjectId label, int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_deleted_edges(int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_nodes_count(int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_nodes_labels_count(int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_nodes_properties_count(int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_edge_properties_count(int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::update_equal_from_to_count(int diff)
{
    std::unique_lock lock(mutex);
    // TODO:
}

void QuadCatalog::flush_changes()
{
    file.flush();
}
