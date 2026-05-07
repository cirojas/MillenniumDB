#pragma once

#include "graph_models/common/conversions.h"
#include "graph_models/common/datatypes/datetime.h"
#include "graph_models/inliner.h"
#include "graph_models/quad_model/quad_catalog.h"
#include "import/disk_vector.h"
#include "import/external_helper.h"
#include "import/quad_model/lexer/state.h"
#include "import/quad_model/lexer/token.h"
#include "import/quad_model/lexer/tokenizer.h"
#include "misc/istream.h"
#include "misc/unicode_escape.h"
#include "storage/index/lists/list_encoder.h"

#include <cctype>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <iostream>

#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/unordered/unordered_flat_map.hpp>

namespace Import { namespace QuadModel {
class OnDiskImport {
public:
    static constexpr char PENDING_NODES_PREFIX[] = "/tmp_pending_nodes";
    static constexpr char PENDING_NODE_LABELS_PREFIX[] = "/tmp_pending_node_labels";
    static constexpr char PENDING_NODE_PROPERTIES_PREFIX[] = "/tmp_pending_node_properties";
    static constexpr char PENDING_EDGE_PROPERTIES_PREFIX[] = "/tmp_pending_edge_properties";
    static constexpr char PENDING_EDGES_PREFIX[] = "/tmp_pending_edges";

    OnDiskImport(const std::string& db_folder, uint64_t strings_buffer_size, uint64_t tensors_buffer_size) :
        strings_buffer_size(strings_buffer_size),
        tensors_buffer_size(tensors_buffer_size),
        db_folder(db_folder),
        catalog(QuadCatalog("catalog.dat")),
        nodes(db_folder + "/tmp_nodes"),
        node_labels(db_folder + "/tmp_node_labels"),
        node_properties(db_folder + "/tmp_node_properties"),
        edge_properties(db_folder + "/tmp_edge_properties"),
        edges(db_folder + "/tmp_edges"),
        equal_from_to(db_folder + "/tmp_equal_from_to")
    {
        state_transitions = new int[Token::TOTAL_TOKENS * State::TOTAL_STATES];
        create_automata();
        list_buffer = new char[StringManager::MAX_STRING_SIZE];
    }

    ~OnDiskImport()
    {
        delete[] (state_transitions);
        delete[] list_buffer;
    }

    void start_import(MDBIstream& in);

private:
    uint64_t strings_buffer_size;
    uint64_t tensors_buffer_size;

    int* state_transitions;
    std::function<void()> state_funcs[Token::TOTAL_TOKENS * State::TOTAL_STATES];
    MQLTokenizer lexer;
    int current_line;
    int current_state;

    // we use a stack to represent nested lists
    std::stack<std::vector<ObjectId>> lists_stack;

    // buffer used to encode lists
    char* list_buffer;

    uint64_t parsing_errors = 0;

    uint64_t id1;
    uint64_t id2;
    uint64_t edge_id;
    uint64_t key_id;
    uint64_t value_id;
    uint64_t label_id;
    uint64_t edge_count = 0;
    uint64_t max_anon_seen = 0;

    // true: right, false: left
    bool direction;

    std::string db_folder;
    QuadCatalog catalog;

    std::unique_ptr<DiskVector<1>> pending_nodes;
    std::unique_ptr<DiskVector<2>> pending_node_labels;
    std::unique_ptr<DiskVector<3>> pending_node_properties;
    std::unique_ptr<DiskVector<3>> pending_edge_properties;
    std::unique_ptr<DiskVector<4>> pending_edges;

    DiskVector<1> nodes;
    DiskVector<2> node_labels;
    DiskVector<3> node_properties;
    DiskVector<3> edge_properties;
    DiskVector<4> edges; // from, to, label, edge
    DiskVector<3> equal_from_to;

    boost::unordered_flat_map<std::string, uint64_t> node_labels2id;
    boost::unordered_flat_map<std::string, uint64_t> edge_labels2id;
    boost::unordered_flat_map<std::string, uint64_t> keys2id;

    // manager writing bytes to disk in a buffered manner
    std::unique_ptr<ExternalHelper> ext_helper;

    void do_nothing() { }

    void set_left_direction()
    {
        direction = false;
    }

    void set_right_direction()
    {
        direction = true;
    }

    void save_first_id_identifier()
    {
        if (lexer.str_len < 8) {
            id1 = Inliner::inline_string(lexer.str) | ObjectId::MASK_NAMED_NODE_INL;
        } else {
            id1 = ext_helper->get_or_create_ext(lexer.str, lexer.str_len, ObjectId::MASK_NAMED_NODE_EXT);
        }
    }

    void save_first_id_anon()
    {
        uint64_t unmasked_id;
        // ignore first 2 characters: '_a'
        auto [ptr, ec] = std::from_chars(lexer.str + 2, lexer.str + lexer.str_len, unmasked_id);

        if (ec == std::errc()) {
            id1 = unmasked_id | ObjectId::MASK_ANON_INL;
            if (unmasked_id > max_anon_seen) {
                max_anon_seen = unmasked_id;
            }
        }
    }

    void save_first_id_string()
    {
        normalize_string_literal(lexer.str, &lexer.str_len);

        if (lexer.str_len < 8) {
            id1 = Inliner::inline_string(lexer.str) | ObjectId::MASK_NAMED_NODE_INL;
        } else {
            id1 = ext_helper->get_or_create_ext(lexer.str, lexer.str_len, ObjectId::MASK_NAMED_NODE_EXT);
        }
    }

    int64_t try_parse_int(char* c_str)
    {
        return Common::Conversions::pack_int(atoll(c_str)).id;
    }

    int64_t try_parse_float(char* c_str)
    {
        return Common::Conversions::pack_float(atof(c_str)).id;
    }

    // Packs a "0x..."-style hex token into an ObjectId using ext_helper.
    // Supports up to 510 hex digits (255 bytes). Values up to 7 bytes are inlined.
    // Leading zero bytes are stripped so "0x0032" and "0x32" produce the same ObjectId.
    uint64_t pack_hex_id()
    {
        const char* hex = lexer.str + 2; // skip "0x" or "0X"
        const size_t hex_len = lexer.str_len - 2;

        if (hex_len == 0 || hex_len > 510) {
            parsing_errors++;
            WARN("line ", current_line, ": invalid hex identifier ", lexer.str);
            return ObjectId::NULL_ID;
        }

        // Compress hex chars to binary bytes (up to 255 bytes)
        char bytes[256];
        size_t num_bytes = 0;

        // If odd length, the first nibble forms a single partial byte
        size_t i = 0;
        if (hex_len % 2 == 1) {
            char c = static_cast<char>(tolower(static_cast<unsigned char>(hex[0])));
            bytes[num_bytes++] = static_cast<char>((c >= 'a') ? (c - 'a' + 10) : (c - '0'));
            i = 1;
        }
        for (; i < hex_len; i += 2) {
            char hi = static_cast<char>(tolower(static_cast<unsigned char>(hex[i])));
            char lo = static_cast<char>(tolower(static_cast<unsigned char>(hex[i + 1])));
            uint8_t hi_val = (hi >= 'a') ? (hi - 'a' + 10) : (hi - '0');
            uint8_t lo_val = (lo >= 'a') ? (lo - 'a' + 10) : (lo - '0');
            bytes[num_bytes++] = static_cast<char>((hi_val << 4) | lo_val);
        }

        // Strip leading zero bytes; keep at least one byte (for "0x0" → [0x00])
        size_t start = 0;
        while (start < num_bytes - 1 && bytes[start] == 0) {
            start++;
        }
        const char* data = bytes + start;
        num_bytes -= start;

        // Fits in 7 bytes → inline as integer value
        if (num_bytes <= 7) {
            uint64_t value = 0;
            for (size_t j = 0; j < num_bytes; j++) {
                value = (value << 8) | static_cast<unsigned char>(data[j]);
            }
            return ObjectId::MASK_NAMED_NODE_HEX_INL | value;
        }

        return ext_helper->get_or_create_ext(data, num_bytes, ObjectId::MASK_NAMED_NODE_HEX_EXT);
    }

    // Packs a 36-char UUID token into 16 compressed bytes and stores externally.
    uint64_t pack_uuid_id()
    {
        // Normalize to lowercase and compress 36 UUID chars to 16 bytes
        char bytes[16];
        int out_pos = 0;
        int hex_idx = 0;
        char pair[3] = { 0, 0, '\0' };
        for (size_t i = 0; i < lexer.str_len; i++) {
            char c = static_cast<char>(tolower(static_cast<unsigned char>(lexer.str[i])));
            if (c == '-') {
                continue;
            }
            pair[hex_idx % 2] = c;
            if (hex_idx % 2 == 1) {
                bytes[out_pos++] = static_cast<char>(
                    static_cast<unsigned char>(strtol(pair, nullptr, 16))
                );
            }
            hex_idx++;
        }
        return ext_helper->get_or_create_ext(bytes, 16, ObjectId::MASK_NAMED_NODE_UUID_EXT);
    }

    void save_first_id_hex()
    {
        id1 = pack_hex_id();
    }

    void save_second_id_hex()
    {
        id2 = pack_hex_id();
    }

    void save_first_id_uuid()
    {
        id1 = pack_uuid_id();
    }

    void save_second_id_uuid()
    {
        id2 = pack_uuid_id();
    }

    void try_save_declared_node()
    {
        if ((id1 & ObjectId::MOD_MASK) == ObjectId::MOD_TMP) {
            pending_nodes->push_back({ id1 });
        } else {
            nodes.push_back({ id1 });
        }
    }

    // templated to prevent branching while processing pending_edges
    template<bool is_right_direction>
    void try_save_quad()
    {
        if ((id1 & ObjectId::MOD_MASK) == ObjectId::MOD_TMP || (id2 & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
        {
            if constexpr (is_right_direction) {
                pending_edges->push_back({ id1, id2, label_id, edge_id });
            } else {
                pending_edges->push_back({ id2, id1, label_id, edge_id });
            }
            return;
        }

        if constexpr (is_right_direction) {
            edges.push_back({ id1, id2, label_id, edge_id });
        } else {
            edges.push_back({ id2, id1, label_id, edge_id });
        }

        if (id1 == id2) {
            equal_from_to.push_back({ id1, label_id, edge_id });
        }
    }

    void try_save_node_label()
    {
        if ((id1 & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
        {
            pending_node_labels->push_back({ id1, label_id });
        } else {
            node_labels.push_back({ id1, label_id });
        }
    }

    void try_save_node_property(uint64_t obj_id)
    {
        if ((obj_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP
            || (value_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
        {
            pending_node_properties->push_back({ obj_id, key_id, value_id });
        } else {
            node_properties.push_back({ obj_id, key_id, value_id });
        }
    }

    void try_save_edge_property(uint64_t obj_id)
    {
        if ((obj_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP
            || (value_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
        {
            pending_edge_properties->push_back({ obj_id, key_id, value_id });
        } else {
            edge_properties.push_back({ obj_id, key_id, value_id });
        }
    }

    void save_edge_label()
    {
        std::string label(lexer.str, lexer.str_len);
        auto it = edge_labels2id.find(label);
        if (it != edge_labels2id.end()) {
            label_id = it->second | ObjectId::MASK_EDGE_LABEL;
        } else {
            auto new_id = edge_labels2id.size();
            edge_labels2id.insert({label, new_id});
            label_id = new_id | ObjectId::MASK_EDGE_LABEL;
        }

        edge_id = edge_count++ | ObjectId::MASK_DIRECTED_EDGE;

        if (direction) {
            try_save_quad<true>();
        } else {
            try_save_quad<false>();
        }
    }

    void save_property_key()
    {
        std::string key(lexer.str, lexer.str_len);
        auto it = keys2id.find(key);
        if (it != keys2id.end()) {
            key_id = it->second | ObjectId::MASK_PROPERTY_KEY;
        } else {
            auto new_id = keys2id.size();
            keys2id.insert({key, new_id});
            key_id = new_id | ObjectId::MASK_PROPERTY_KEY;
        }
    }

    void save_second_id_identifier()
    {
        if (lexer.str_len < 8) {
            id2 = Inliner::inline_string(lexer.str) | ObjectId::MASK_NAMED_NODE_INL;
        } else {
            id2 = ext_helper->get_or_create_ext(lexer.str, lexer.str_len, ObjectId::MASK_NAMED_NODE_EXT);
        }
    }

    void save_second_id_anon()
    {
        uint64_t unmasked_id;
        // ignore first 2 characters: '_a'
        auto [ptr, ec] = std::from_chars(lexer.str + 2, lexer.str + lexer.str_len, unmasked_id);

        if (ec == std::errc()) {
            id2 = unmasked_id | ObjectId::MASK_ANON_INL;
            if (unmasked_id > max_anon_seen) {
                max_anon_seen = unmasked_id;
            }
        }
    }

    void save_second_id_string()
    {
        normalize_string_literal(lexer.str, &lexer.str_len);

        if (lexer.str_len < 8) {
            id2 = Inliner::inline_string(lexer.str) | ObjectId::MASK_NAMED_NODE_INL;
        } else {
            id2 = ext_helper->get_or_create_ext(lexer.str, lexer.str_len, ObjectId::MASK_NAMED_NODE_EXT);
        }
    }

    void add_node_label()
    {
        std::string label(lexer.str, lexer.str_len);
        auto it = node_labels2id.find(label);
        if (it != node_labels2id.end()) {
            label_id = it->second | ObjectId::MASK_NODE_LABEL;
        } else {
            auto new_id = node_labels2id.size();
            node_labels2id.insert({label, new_id});
            label_id = new_id | ObjectId::MASK_NODE_LABEL;
        }

        try_save_node_label();
    }

    void add_node_prop_datatype()
    {
        parse_prop_datatype(lexer.str, lexer.str_len);
        try_save_node_property(id1);
    }

    void add_edge_prop_datatype()
    {
        parse_prop_datatype(lexer.str, lexer.str_len);
        try_save_edge_property(edge_id);
    }

    // parse datatype and store it in value_id
    void parse_prop_datatype(char* typed_str, size_t typed_str_len)
    {
        // we have something like: `datatype("string")`
        // parse datatype name
        char* datatype_beg = typed_str;
        char* datatype_end = typed_str;
        while (isalpha(*datatype_end)) {
            datatype_end++;
        }
        *datatype_end = '\0';

        char* str_value_end = typed_str + (typed_str_len - 1);
        typed_str = datatype_end + 1;
        while (*typed_str != '"') {
            typed_str++;
        }

        // it may have whitespaces `datatype("string"  )` so we iterate
        while (*str_value_end != '"') {
            str_value_end--;
        }
        typed_str_len = (str_value_end - typed_str) + 1;

        // we edited typed_str_len and typed_str to point correctly at the datatype (considering quotes)
        normalize_string_literal(typed_str, &typed_str_len);

        if (strcmp(datatype_beg, "dateTime") == 0) {
            value_id = DateTime::from_dateTime(typed_str);
            if (value_id == ObjectId::NULL_ID) {
                parsing_errors++;
                WARN("line ", current_line, ": invalid dateTime ", typed_str);
                return;
            }
        } else if (strcmp(datatype_beg, "date") == 0) {
            value_id = DateTime::from_date(typed_str);
            if (value_id == ObjectId::NULL_ID) {
                parsing_errors++;
                WARN("line ", current_line, ": invalid date ", typed_str);
                return;
            }
        } else if (strcmp(datatype_beg, "time") == 0) {
            value_id = DateTime::from_time(typed_str);
            if (value_id == ObjectId::NULL_ID) {
                parsing_errors++;
                WARN("line ", current_line, ": invalid time ", typed_str);
                return;
            }
        } else if (strcmp(datatype_beg, "dateTimeStamp") == 0) {
            value_id = DateTime::from_dateTimeStamp(typed_str);
            if (value_id == ObjectId::NULL_ID) {
                parsing_errors++;
                WARN("line ", current_line, ": invalid dateTimeStamp ", typed_str);
                return;
            }
        } else if (strcmp(datatype_beg, "tensorFloat") == 0) {
            value_id = get_tensor_id<float>(typed_str);
            if (value_id == ObjectId::NULL_ID) {
                ++parsing_errors;
                WARN("line ", current_line, ": invalid tensorFloat ", typed_str);
            }
        } else if (strcmp(datatype_beg, "tensorDouble") == 0) {
            value_id = get_tensor_id<double>(typed_str);
            if (value_id == ObjectId::NULL_ID) {
                ++parsing_errors;
                WARN("line ", current_line, ": invalid tensorDouble ", typed_str);
            }
        } else {
            parsing_errors++;
            WARN("line ", current_line, ": unknown datatype  ", datatype_beg);
            return;
        }
    }

    template<typename T>
    uint64_t get_tensor_id(std::string_view str)
    {
        bool error;
        const auto tensor = tensor::Tensor<T>::from_literal(str, &error);
        if (error) {
            return ObjectId::NULL_ID;
        }

        const auto bytes = reinterpret_cast<const char*>(tensor.data());
        const auto num_bytes = sizeof(T) * tensor.size();
        return ext_helper->get_or_create_tensor(bytes, num_bytes, tensor::Tensor<T>::get_external_mask());
    }

    void add_node_prop_string()
    {
        normalize_string_literal(lexer.str, &lexer.str_len);

        if (lexer.str_len < 8) {
            value_id = Inliner::inline_string(lexer.str) | ObjectId::MASK_STR_INL;
        } else {
            value_id = ext_helper->get_or_create_ext(lexer.str, lexer.str_len, ObjectId::MASK_STR_EXT);
        }

        try_save_node_property(id1);
    }

    void add_node_prop_int()
    {
        value_id = try_parse_int(lexer.str);
        try_save_node_property(id1);
    }

    void add_node_prop_float()
    {
        value_id = try_parse_float(lexer.str);
        try_save_node_property(id1);
    }

    void add_node_prop_true()
    {
        value_id = ObjectId::MASK_BOOL | 0x01;
        try_save_node_property(id1);
    }

    void add_node_prop_false()
    {
        value_id = ObjectId::MASK_BOOL | 0x00;
        try_save_node_property(id1);
    }

    void add_edge_prop_string()
    {
        normalize_string_literal(lexer.str, &lexer.str_len);

        if (lexer.str_len < 8) {
            value_id = Inliner::inline_string(lexer.str) | ObjectId::MASK_STR_INL;
        } else {
            value_id = ext_helper->get_or_create_ext(lexer.str, lexer.str_len, ObjectId::MASK_STR_EXT);
        }

        try_save_edge_property(edge_id);
    }

    void add_edge_prop_int()
    {
        value_id = try_parse_int(lexer.str);
        try_save_edge_property(edge_id);
    }

    void add_edge_prop_float()
    {
        value_id = try_parse_float(lexer.str);
        try_save_edge_property(edge_id);
    }

    void add_edge_prop_true()
    {
        value_id = ObjectId::MASK_BOOL | 0x01;
        try_save_edge_property(edge_id);
    }

    void add_edge_prop_false()
    {
        value_id = ObjectId::MASK_BOOL | 0x00;
        try_save_edge_property(edge_id);
    }

    void init_list()
    {
        lists_stack.emplace();
    }

    void add_list_value_false()
    {
        ObjectId value = Common::Conversions::pack_bool(false);
        lists_stack.top().push_back(value);
    }

    void add_list_value_true()
    {
        ObjectId value = Common::Conversions::pack_bool(true);
        lists_stack.top().push_back(value);
    }

    void add_list_value_integer()
    {
        int64_t integer = try_parse_int(lexer.str);
        lists_stack.top().emplace_back(integer);
    }

    void add_list_value_float()
    {
        int64_t value = try_parse_float(lexer.str);
        lists_stack.top().emplace_back(value);
    }

    void add_list_value_string()
    {
        normalize_string_literal(lexer.str, &lexer.str_len);

        uint64_t str_id;
        if (lexer.str_len < 8) {
            str_id = Inliner::inline_string(lexer.str) | ObjectId::MASK_STR_INL;
        } else {
            str_id = ext_helper->get_or_create_ext(lexer.str, lexer.str_len, ObjectId::MASK_STR_EXT);
        }

        lists_stack.top().emplace_back(str_id);
    }

    void add_list_value_typed_string()
    {
        parse_prop_datatype(lexer.str, lexer.str_len);
        lists_stack.top().emplace_back(value_id);
    }

    void save_node_list()
    {
        std::vector<ObjectId> current_list = lists_stack.top();
        uint64_t encoded_size = ListEncoder::encode(current_list, list_buffer);
        lists_stack.pop();

        uint64_t list_id = ext_helper->get_or_create_ext(list_buffer, encoded_size, ObjectId::MASK_LIST_EXT);

        // if there is a list in the stack, then this list is nested and we do not store the property yet
        if (!lists_stack.empty()) {
            current_state = EXPECT_NODE_LIST_ELEMENT;
            lists_stack.top().emplace_back(list_id);
            return;
        }

        if ((id1 & ObjectId::MOD_MASK) == ObjectId::MOD_TMP
            || (list_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
        {
            pending_node_properties->push_back({ id1, key_id, list_id });
        } else {
            node_properties.push_back({ id1, key_id, list_id });
        }
    }

    void save_edge_list()
    {
        std::vector<ObjectId> current_list = lists_stack.top();
        uint64_t encoded_size = ListEncoder::encode(current_list, list_buffer);
        lists_stack.pop();

        uint64_t list_id = ext_helper->get_or_create_ext(list_buffer, encoded_size, ObjectId::MASK_LIST_EXT);

        // if there is a list in the stack, then this list is nested and we do not store the property yet
        if (!lists_stack.empty()) {
            current_state = EXPECT_EDGE_LIST_ELEMENT;
            lists_stack.top().emplace_back(list_id);
            return;
        }

        if ((edge_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP
            || (list_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
        {
            pending_edge_properties->push_back({ edge_id, key_id, list_id });
        } else {
            edge_properties.push_back({ edge_id, key_id, list_id });
        }
    }

    void finish_wrong_line()
    {
        current_line++;
    }

    void finish_node_line()
    {
        current_line++;

        try_save_declared_node();
    }

    void finish_edge_line()
    {
        current_line++;
    }

    void print_error()
    {
        parsing_errors++;
        WARN("ERROR on line ", current_line);
    }

    // processes a pending file by iterations, until no more pending tuples are available
    // the size of the tuples and a resolve+save function must be provided.
    // pending_vector is passed-by-reference in order to keep it valid when calling the
    // resolve_and_save_func
    template<std::size_t N, typename ResolveAndSaveFunc>
    inline void process_pending(
        std::unique_ptr<DiskVector<N>>& pending_vector,
        const std::string& name,
        const std::string& pending_filename_prefix,
        ResolveAndSaveFunc resolve_and_save_func
    )
    {
        pending_vector->finish_appends();
        int i = 0;
        while (true) {
            const auto total_pending = pending_vector->get_total_tuples();
            if (total_pending == 0) {
                break;
            }
            std::cout << "pending " + name + ": " << total_pending << std::endl;

            auto old_pending_vector = std::move(pending_vector);
            pending_vector = std::make_unique<DiskVector<N>>(
                db_folder + "/" + pending_filename_prefix + std::to_string(i)
            );
            ++i;

            // advance pending variables for current iteration
            ext_helper->advance_pending();
            ext_helper->clear_sets();

            old_pending_vector->begin_tuple_iter();
            while (old_pending_vector->has_next_tuple()) {
                const auto& pending_tuple = old_pending_vector->next_tuple();

                resolve_and_save_func(pending_tuple);
            }

            // write out new data
            ext_helper->flush_to_disk();
            // close and delete the old pending files
            ext_helper->clean_up_old();

            // close and delete old pending file
            pending_vector->finish_appends();
            old_pending_vector->skip_indexing(); // will close and remove file
        }

        // process pending finished, clean up the last pending file
        pending_vector->skip_indexing();
    }

private:
    void create_automata();

    void set_transition(int state, int token, int value, std::function<void()> func)
    {
        state_funcs[State::TOTAL_STATES * state + token] = func;
        state_transitions[State::TOTAL_STATES * state + token] = value;
    }

    void get_transition(int token)
    {
        auto& func = state_funcs[State::TOTAL_STATES * current_state + token];
        current_state = state_transitions[State::TOTAL_STATES * current_state + token];
        func();
    }

    // normalize str in place, the resulting size is written in str_len
    void normalize_string_literal(char* str, size_t* str_len)
    {
        char* write_ptr = str;
        char* read_ptr = write_ptr + 1; // skip first character: '"'

        *str_len -= 2;
        char* end = str + *str_len + 1;

        UnicodeEscape::normalize_string(read_ptr, write_ptr, end, *str_len);
    }
};
}} // namespace Import::QuadModel
