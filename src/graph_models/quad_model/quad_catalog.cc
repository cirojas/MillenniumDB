#include "quad_catalog.h"

#include "query/exceptions.h"

#include <mutex>
// #include "storage/index/text_search/quad.h"

// using namespace std;

enum class CatalogInfo {
    node_label = 1, // <label_id:8, strlen:4, label_str:strlen>
    edge_label = 2, // <label_id:8, strlen:4, label_str:strlen>
    key = 3, //  <key_id:8, strlen:4, label_str:strlen>
    node_label_stat = 4, // <label_id:8, count:8>
    edge_label_stat = 5, // <label_id:8, count:8>
    equal_from_to_stat = 6, // <label_id:8, count:8>
    node_key_stat = 7, // <key_id:8, count:8>
    edge_key_stat = 8, // <key_id:8, count:8>

    // <idx_name_len:4, idx_name:idx_name_len, metric_type:1, pred_len:4,
    //  pred_str:pred_len>
    hnsw_index = 9,

    // <idx_name_len:4, idx_name:idx_name_len, norm_type:1, token_type:1,
    //  pred_id:8, pred_len:4, pred_str:pred_len>
    text_index = 10,
};

QuadCatalog::QuadCatalog(const std::string& filename) :
    Catalog(filename)
{
    if (is_empty()) {
        // TODO: allow empty catalog?
        max_anon = 0;
        max_edge = 0;
        deleted_edges = 0;
        nodes_count = 0;
        node_labels_count = 0;
        node_properties_count = 0;
        edge_properties_count = 0;
        equal_from_to_count = 0;
        start_write(MODEL_ID, MAJOR_VERSION, MINOR_VERSION);
        // TODO:
        write_uint64(0);
        write_uint64(0);
        write_uint64(0);
        write_uint64(0);
        write_uint64(0);
        write_uint64(0);
        write_uint64(0);
        write_uint64(0);
        return;
    }
    // not empty
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

    char byte;
    while (file.get(byte)) {
        switch (CatalogInfo(byte)) {
        case CatalogInfo::node_label: {
            auto label_id = read_uint64();
            auto label_str = read_string();
            if (label_id >= node_labels_str.size()) {
                node_labels_str.resize(label_id + 1);
            }
            std::cout << "node_label: " << label_str << ":" << label_id << "\n";
            node_labels_str[label_id] = label_str;
            node_labels2id.insert({ label_str, label_id });
            break;
        }
        case CatalogInfo::edge_label: {
            auto label_id = read_uint64();
            auto label_str = read_string();
            if (label_id >= edge_labels_str.size()) {
                edge_labels_str.resize(label_id + 1);
            }
            std::cout << "edge_label: " << label_str << ":" << label_id << "\n";
            edge_labels_str[label_id] = label_str;
            edge_labels2id.insert({ label_str, label_id });
            break;
        }
        case CatalogInfo::key: {
            auto key_id = read_uint64();
            auto key_str = read_string();
            if (key_id >= keys_str.size()) {
                keys_str.resize(key_id + 1);
            }
            std::cout << "key: " << key_id << ":" << key_str << "\n";
            keys_str[key_id] = key_str;
            keys2id.insert({ key_str, key_id });
            break;
        }
        case CatalogInfo::node_label_stat: {
            auto label_id = read_uint64();
            uint64_t offset = file.tellp();
            auto count = read_uint64();
            CountOffset data { count, offset };
            node_label2total_count.insert({ ObjectId(label_id), data });
            break;
        }
        case CatalogInfo::edge_label_stat: {
            auto label_id = read_uint64();
            uint64_t offset = file.tellp();
            auto count = read_uint64();
            CountOffset data { count, offset };
            edge_label2total_count.insert({ ObjectId(label_id), data });
            break;
        }
        case CatalogInfo::equal_from_to_stat: {
            auto label_id = read_uint64();
            uint64_t offset = file.tellp();
            auto count = read_uint64();
            CountOffset data { count, offset };
            edge_label2equal_from_to_count.insert({ ObjectId(label_id), data });
            break;
        }
        case CatalogInfo::node_key_stat: {
            auto key_id = read_uint64();
            uint64_t offset = file.tellp();
            auto count = read_uint64();
            CountOffset data { count, offset };
            node_key2total_count.insert({ ObjectId(key_id), data });
            break;
        }
        case CatalogInfo::edge_key_stat: {
            auto key_id = read_uint64();
            uint64_t offset = file.tellp();
            auto count = read_uint64();
            CountOffset data { count, offset };
            edge_key2total_count.insert({ ObjectId(key_id), data });
            break;
        }
        case CatalogInfo::hnsw_index: {
            auto idx_name = read_string();
            HNSW::HNSWIndexManager::HNSWIndexMetadata metadata;
            metadata.metric_type = static_cast<HNSW::MetricType>(read_uint8());
            metadata.predicate = read_string();
            hnsw_index_manager.load_hnsw_index(idx_name, metadata);
            break;
            // hnsw_index_manager.init();
            // const auto hnsw_index_name2metadata_size = read_uint64();
            // for (uint_fast32_t i = 0; i < hnsw_index_name2metadata_size; ++i) {
            //     const auto name = read_string();
            //     HNSW::HNSWIndexManager::HNSWIndexMetadata metadata;
            //     metadata.metric_type = static_cast<HNSW::MetricType>(read_uint8());
            //     metadata.predicate = read_string();
            //     hnsw_index_manager.load_hnsw_index(name, metadata);
            // }
        }
        case CatalogInfo::text_index: {
            auto idx_name = read_string();
            TextSearch::TextIndexManager::TextIndexMetadata metadata;
            metadata.normalization_type = static_cast<TextSearch::NORMALIZE_TYPE>(read_uint8());
            metadata.tokenization_type = static_cast<TextSearch::TOKENIZE_TYPE>(read_uint8());
            metadata.predicate_id = ObjectId(read_uint64());
            metadata.predicate = read_string();
            text_index_manager.load_text_index(idx_name, metadata);
            break;
            // const auto text_index_name2metadata_size = read_uint64();
            // for (uint_fast32_t i = 0; i < text_index_name2metadata_size; ++i) {
            //     const auto name = read_string();
            //     TextSearch::TextIndexManager::TextIndexMetadata metadata;
            //     metadata.normalization_type = static_cast<TextSearch::NORMALIZE_TYPE>(read_uint8());
            //     metadata.tokenization_type = static_cast<TextSearch::TOKENIZE_TYPE>(read_uint8());
            //     metadata.predicate_id = ObjectId(read_uint64());
            //     metadata.predicate = read_string();
            //     text_index_manager.load_text_index(name, metadata);
            // }
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
        res = node_labels_str[id];
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
    if (max_anon == new_value)
        return;
    max_anon = new_value;
    file.seekp(MAX_ANON_OFFSET);
    write_uint64(max_anon);
}

void QuadCatalog::update_max_edge(uint64_t new_value)
{
    if (max_edge == new_value)
        return;
    max_edge = new_value;
    file.seekp(MAX_EDGE_OFFSET);
    write_uint64(max_edge);
}

void QuadCatalog::update_deleted_edges(int diff)
{
    deleted_edges += diff;
    file.seekp(DELETED_EDGES_OFFSET);
    write_uint64(deleted_edges);
}

void QuadCatalog::update_nodes_count(int diff)
{
    nodes_count += diff;
    file.seekp(NODES_COUNT_OFFSET);
    write_uint64(nodes_count);
}

void QuadCatalog::update_nodes_labels_count(int diff)
{
    node_labels_count += diff;
    file.seekp(NODE_LABELS_COUNT_OFFSET);
    write_uint64(node_labels_count);
}

void QuadCatalog::update_nodes_properties_count(int diff)
{
    node_properties_count += diff;
    file.seekp(NODE_PROPERTIES_COUNT_OFFSET);
    write_uint64(node_properties_count);
}

void QuadCatalog::update_edge_properties_count(int diff)
{
    edge_properties_count += diff;
    file.seekp(EDGE_PROPERTIES_COUNT_OFFSET);
    write_uint64(edge_properties_count);
}

void QuadCatalog::update_equal_from_to_count(int diff)
{
    equal_from_to_count += diff;
    file.seekp(EQUAL_FROM_TO_COUNT_OFFSET);
    write_uint64(equal_from_to_count);
}

void QuadCatalog::update_node_key_count(ObjectId key, int diff)
{
    std::unique_lock lock(mutex);
    auto it = node_key2total_count.find(key);
    if (it != node_key2total_count.end()) {
        // write at the end of catalog file
        file.seekp(0, file.end);
        uint64_t offset = uint64_t(file.tellp()) + 9; // + 9 to skip info+key_id
        if (diff <= 0) {
            assert(false);
            return;
        }
        CountOffset data { static_cast<uint64_t>(diff), offset };
        node_key2total_count.insert({ key, data });
        write_uint8(uint8_t(CatalogInfo::node_key_stat));
        write_uint64(key.id);
        write_uint64(data.count);
    } else {
        it->second.count += diff;
        file.seekp(it->second.offset);
        write_uint64(it->second.count);
    }
}

void QuadCatalog::update_edge_key_count(ObjectId key, int diff)
{
    std::unique_lock lock(mutex);
    auto it = edge_key2total_count.find(key);
    if (it != edge_key2total_count.end()) {
        // write at the end of catalog file
        file.seekp(0, file.end);
        uint64_t offset = uint64_t(file.tellp()) + 9; // + 9 to skip info+key_id
        if (diff <= 0) {
            assert(false);
            return;
        }
        CountOffset data { static_cast<uint64_t>(diff), offset };
        edge_key2total_count.insert({ key, data });
        write_uint8(uint8_t(CatalogInfo::edge_key_stat));
        write_uint64(key.id);
        write_uint64(data.count);
    } else {
        it->second.count += diff;
        file.seekp(it->second.offset);
        write_uint64(it->second.count);
    }
}

void QuadCatalog::update_node_label_count(ObjectId label, int diff)
{
    std::unique_lock lock(mutex);
    auto it = node_label2total_count.find(label);
    if (it != node_label2total_count.end()) {
        // write at the end of catalog file
        file.seekp(0, file.end);
        uint64_t offset = uint64_t(file.tellp()) + 9; // + 9 to skip info+label_id
        if (diff <= 0) {
            assert(false);
            return;
        }
        CountOffset data { static_cast<uint64_t>(diff), offset };
        node_label2total_count.insert({ label, data });
        write_uint8(uint8_t(CatalogInfo::node_label_stat));
        write_uint64(label.id);
        write_uint64(data.count);
    } else {
        it->second.count += diff;
        file.seekp(it->second.offset);
        write_uint64(it->second.count);
    }
}

void QuadCatalog::update_edge_label_count(ObjectId label, int diff)
{
    std::unique_lock lock(mutex);
    auto it = edge_label2total_count.find(label);
    if (it != edge_label2total_count.end()) {
        // write at the end of catalog file
        file.seekp(0, file.end);
        uint64_t offset = uint64_t(file.tellp()) + 9; // + 9 to skip info+label_id
        if (diff <= 0) {
            assert(false);
            return;
        }
        CountOffset data { static_cast<uint64_t>(diff), offset };
        edge_label2total_count.insert({ label, data });
        write_uint8(uint8_t(CatalogInfo::edge_label_stat));
        write_uint64(label.id);
        write_uint64(data.count);
    } else {
        it->second.count += diff;
        file.seekp(it->second.offset);
        write_uint64(it->second.count);
    }
}

void QuadCatalog::update_equal_from_to_label_count(ObjectId label, int diff)
{
    std::unique_lock lock(mutex);
    auto it = edge_label2equal_from_to_count.find(label);
    if (it != edge_label2equal_from_to_count.end()) {
        // write at the end of catalog file
        file.seekp(0, file.end);
        uint64_t offset = uint64_t(file.tellp()) + 9; // + 9 to skip info+label_id
        if (diff <= 0) {
            assert(false);
            return;
        }
        CountOffset data { static_cast<uint64_t>(diff), offset };
        edge_label2equal_from_to_count.insert({ label, data });
        write_uint8(uint8_t(CatalogInfo::equal_from_to_stat));
        write_uint64(label.id);
        write_uint64(data.count);
    } else {
        it->second.count += diff;
        file.seekp(it->second.offset);
        write_uint64(it->second.count);
    }
}

void QuadCatalog::flush_changes()
{
    file.flush();
}

void QuadCatalog::create_new_keys(const std::map<std::string, ObjectId>& map)
{
    std::unique_lock lock(mutex);
    file.seekp(0, file.end);

    keys_str.resize(keys_str.size() + map.size());
    for (auto&& [str, oid] : map) {
        auto key_id = oid.get_value();
        write_uint8(uint8_t(CatalogInfo::key));
        write_uint64(key_id);
        write_string(str);

        keys_str[key_id] = str;
        keys2id.insert({ str, key_id });
    }
}

void QuadCatalog::create_new_edge_labels(const std::map<std::string, ObjectId>& map)
{
    std::unique_lock lock(mutex);
    file.seekp(0, file.end);

    edge_labels_str.resize(edge_labels_str.size() + map.size());
    for (auto&& [str, oid] : map) {
        auto label_id = oid.get_value();
        write_uint8(uint8_t(CatalogInfo::edge_label));
        write_uint64(label_id);
        write_string(str);

        edge_labels_str[label_id] = str;
        edge_labels2id.insert({ str, label_id });
    }
}

void QuadCatalog::create_new_node_labels(const std::map<std::string, ObjectId>& map)
{
    std::unique_lock lock(mutex);
    file.seekp(0, file.end);

    node_labels_str.resize(node_labels_str.size() + map.size());
    for (auto&& [str, oid] : map) {
        auto label_id = oid.get_value();
        write_uint8(uint8_t(CatalogInfo::node_label));
        write_uint64(label_id);
        write_string(str);

        node_labels_str[label_id] = str;
        node_labels2id.insert({ str, label_id });
    }
}

void QuadCatalog::process_import_keys_labels(
    const boost::unordered_flat_map<std::string, uint64_t>& _keys2id,
    const boost::unordered_flat_map<std::string, uint64_t>& _node_labels2id,
    const boost::unordered_flat_map<std::string, uint64_t>& _edge_labels2id
)
{
    file.seekp(0, file.end);

    // TODO: delete debug prints
    std::cout << "_keys2id:\n";
    for (auto&& [str, internal_id] : _keys2id) {
        std::cout << str << ":" << internal_id << "\n";
        write_uint8(uint8_t(CatalogInfo::key));
        write_uint64(internal_id);
        write_string(str);
    }
    std::cout << "_node_labels2id:\n";
    for (auto&& [str, internal_id] : _node_labels2id) {
        std::cout << str << ":" << internal_id << "\n";
        write_uint8(uint8_t(CatalogInfo::node_label));
        write_uint64(internal_id);
        write_string(str);
    }
    std::cout << "_edge_labels2id:\n";
    for (auto&& [str, internal_id] : _edge_labels2id) {
        std::cout << str << ":" << internal_id << "\n";
        write_uint8(uint8_t(CatalogInfo::edge_label));
        write_uint64(internal_id);
        write_string(str);
    }
}

void QuadCatalog::process_import_node(uint64_t nodes_count, uint64_t max_anon)
{
    // TODO: is it bad to seek to a pos that does not exist yet?
    file.seekp(MAX_ANON_OFFSET);
    write_uint64(max_anon);
    file.seekp(NODES_COUNT_OFFSET);
    write_uint64(nodes_count);
}

void QuadCatalog::process_import_node_labels(
    uint64_t node_labels_count,
    const boost::unordered_flat_map<uint64_t, uint64_t>& node_label2total_count
)
{
    file.seekp(NODE_LABELS_COUNT_OFFSET);
    write_uint64(node_labels_count);

    file.seekp(0, file.end);
    for (auto&& [internal_id, count] : node_label2total_count) {
        write_uint8(uint8_t(CatalogInfo::node_label_stat));
        write_uint64(internal_id);
        write_uint64(count);
    }
}

void QuadCatalog::process_import_node_keys(
    uint64_t node_properties_count,
    const boost::unordered_flat_map<uint64_t, uint64_t>& node_key2total_count
) {
    file.seekp(NODE_PROPERTIES_COUNT_OFFSET);
    write_uint64(node_properties_count);

    file.seekp(0, file.end);
    for (auto&& [internal_id, count] : node_key2total_count) {
        write_uint8(uint8_t(CatalogInfo::node_key_stat));
        write_uint64(internal_id);
        write_uint64(count);
    }
}

void QuadCatalog::process_import_edge_keys(
    uint64_t edge_properties_count,
    const boost::unordered_flat_map<uint64_t, uint64_t>& edge_key2total_count
)  {
    file.seekp(EDGE_PROPERTIES_COUNT_OFFSET);
    write_uint64(edge_properties_count);

    file.seekp(0, file.end);
    for (auto&& [internal_id, count] : edge_key2total_count) {
        write_uint8(uint8_t(CatalogInfo::edge_key_stat));
        write_uint64(internal_id);
        write_uint64(count);
    }
}

void QuadCatalog::process_import_edges(
    uint64_t max_edge,
    const boost::unordered_flat_map<uint64_t, uint64_t>& edge_label2total_count
) {
    file.seekp(MAX_EDGE_OFFSET);
    write_uint64(max_edge);

    file.seekp(0, file.end);
    for (auto&& [internal_id, count] : edge_label2total_count) {
        write_uint8(uint8_t(CatalogInfo::edge_label_stat));
        write_uint64(internal_id);
        write_uint64(count);
    }
}

void QuadCatalog::process_import_equal_from_to(
    uint64_t equal_from_to_count,
    const boost::unordered_flat_map<uint64_t, uint64_t>& edge_label2equal_from_to_count
)  {
    file.seekp(EQUAL_FROM_TO_COUNT_OFFSET);
    write_uint64(equal_from_to_count);

    file.seekp(0, file.end);
    for (auto&& [internal_id, count] : edge_label2equal_from_to_count) {
        write_uint8(uint8_t(CatalogInfo::equal_from_to_stat));
        write_uint64(internal_id);
        write_uint64(count);
    }
}
