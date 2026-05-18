#include "import.h"

#include "graph_models/inliner.h"
#include "import/import_helper.h"
#include "misc/fatal_error.h"
#include "misc/unicode_escape.h"
#include "storage/index/lists/list_encoder.h"

#include <cctype>
#include <unordered_set>

using namespace Import::QuadModel::CSV;

OnDiskImport::OnDiskImport(
    const std::string& db_folder,
    uint64_t strings_buffer_size,
    uint64_t tensors_buffer_size,
    char list_separator
) :
    strings_buffer_size(strings_buffer_size),
    tensors_buffer_size(tensors_buffer_size),
    db_folder(db_folder),
    catalog(QuadCatalog("catalog.dat")),
    nodes(db_folder + "/tmp_nodes"),
    node_labels(db_folder + "/tmp_node_labels"),
    node_properties(db_folder + "/tmp_node_properties"),
    edge_properties(db_folder + "/tmp_edge_properties"),
    equal_from_to(db_folder + "/tmp_equal_from_to"),
    edges(db_folder + "/tmp_edges")
{
    state_transitions = new int[Token::TOTAL_TOKENS * State::TOTAL_STATES];
    list_buffer = new char[StringManager::MAX_STRING_SIZE];
    create_automata();
    label_splitter = list_separator;
    list_splitter = list_separator;
}

OnDiskImport::~OnDiskImport()
{
    delete[] state_transitions;
    delete[] list_buffer;
}

void OnDiskImport::start_import(
    std::vector<std::unique_ptr<MDBIstreamFile>>& in_nodes,
    std::vector<std::unique_ptr<MDBIstreamFile>>& in_edges
)
{
    auto start = std::chrono::system_clock::now();
    auto import_start = start;

    pending_nodes = std::make_unique<DiskVector<1>>(db_folder + PENDING_NODES_PREFIX);
    pending_node_labels = std::make_unique<DiskVector<2>>(db_folder + PENDING_NODE_LABELS_PREFIX);
    pending_node_properties = std::make_unique<DiskVector<3>>(db_folder + PENDING_NODE_PROPERTIES_PREFIX);
    pending_edge_properties = std::make_unique<DiskVector<3>>(db_folder + PENDING_EDGE_PROPERTIES_PREFIX);
    pending_edges = std::make_unique<DiskVector<4>>(db_folder + PENDING_EDGES_PREFIX);

    // Initialize external helper
    ext_helper = std::make_unique<ExternalHelper>(db_folder, strings_buffer_size, tensors_buffer_size);

    // First, import nodes to the database. After that, import relationships
    parse_node_files(in_nodes);
    parse_edge_files(in_edges);

    print_duration("Parsing", start);

    // initial flush
    ext_helper->flush_to_disk();

    { // process pending files
        pending_nodes->finish_appends();
        pending_node_labels->finish_appends();
        pending_node_properties->finish_appends();
        pending_edge_properties->finish_appends();
        pending_edges->finish_appends();

        int i = 0;
        while (true) {
            const auto total_pending = pending_nodes->get_total_tuples()
                                     + pending_node_labels->get_total_tuples()
                                     + pending_node_properties->get_total_tuples()
                                     + pending_edge_properties->get_total_tuples()
                                     + pending_edges->get_total_tuples();
            if (total_pending == 0) {
                break;
            }
            std::cout << "total pending: " << total_pending << std::endl;

            auto old_pending_nodes = std::move(pending_nodes);
            auto old_pending_node_labels = std::move(pending_node_labels);
            auto old_pending_node_properties = std::move(pending_node_properties);
            auto old_pending_edge_properties = std::move(pending_edge_properties);
            auto old_pending_edges = std::move(pending_edges);

            pending_nodes = std::make_unique<DiskVector<1>>(
                db_folder + PENDING_NODES_PREFIX + std::to_string(i)
            );
            pending_node_labels = std::make_unique<DiskVector<2>>(
                db_folder + PENDING_NODE_LABELS_PREFIX + std::to_string(i)
            );
            pending_node_properties = std::make_unique<DiskVector<3>>(
                db_folder + PENDING_NODE_PROPERTIES_PREFIX + std::to_string(i)
            );
            pending_edge_properties = std::make_unique<DiskVector<3>>(
                db_folder + PENDING_EDGE_PROPERTIES_PREFIX + std::to_string(i)
            );
            pending_edges = std::make_unique<DiskVector<4>>(
                db_folder + PENDING_EDGES_PREFIX + std::to_string(i)
            );
            ++i;

            // advance pending variables for current iteration
            ext_helper->advance_pending();
            ext_helper->clear_sets();

            old_pending_nodes->begin_tuple_iter();
            while (old_pending_nodes->has_next_tuple()) {
                const auto& pending_tuple = old_pending_nodes->next_tuple();
                auto id1 = ext_helper->resolve_id(pending_tuple[0]);
                try_save_declared_node(id1);
            }

            old_pending_node_labels->begin_tuple_iter();
            while (old_pending_node_labels->has_next_tuple()) {
                const auto& pending_tuple = old_pending_node_labels->next_tuple();
                auto id1 = ext_helper->resolve_id(pending_tuple[0]);
                auto label_id = pending_tuple[1]; // always inlined
                try_save_node_label(id1, label_id);
            }

            old_pending_node_properties->begin_tuple_iter();
            while (old_pending_node_properties->has_next_tuple()) {
                const auto& pending_tuple = old_pending_node_properties->next_tuple();

                auto id1 = ext_helper->resolve_id(pending_tuple[0]);
                auto key_id = pending_tuple[1]; // always inlined
                auto value_id = ext_helper->resolve_id(pending_tuple[2]);

                try_save_node_property(id1, key_id, value_id);
            }

            old_pending_edge_properties->begin_tuple_iter();
            while (old_pending_edge_properties->has_next_tuple()) {
                const auto& pending_tuple = old_pending_edge_properties->next_tuple();

                auto id1 = ext_helper->resolve_id(pending_tuple[0]);
                auto key_id = pending_tuple[1]; // always inlined
                auto value_id = ext_helper->resolve_id(pending_tuple[2]);

                try_save_edge_property(id1, key_id, value_id);
            }

            old_pending_edges->begin_tuple_iter();
            while (old_pending_edges->has_next_tuple()) {
                const auto& pending_tuple = old_pending_edges->next_tuple();

                auto id1 = ext_helper->resolve_id(pending_tuple[0]);
                auto id2 = ext_helper->resolve_id(pending_tuple[1]);
                auto label_id = pending_tuple[2]; // always inlined
                auto edge_id = pending_tuple[3]; // always inlined

                try_save_quad(id1, id2, label_id, edge_id);
            }

            // write out new data
            ext_helper->flush_to_disk();
            // close and delete the old pending files
            ext_helper->clean_up_old();

            // close and delete old pending file
            pending_nodes->finish_appends();
            pending_node_labels->finish_appends();
            pending_node_properties->finish_appends();
            pending_edge_properties->finish_appends();
            pending_edges->finish_appends();

            old_pending_nodes->skip_indexing(); // will close and remove file
            old_pending_node_labels->skip_indexing(); // will close and remove file
            old_pending_node_properties->skip_indexing(); // will close and remove file
            old_pending_edge_properties->skip_indexing(); // will close and remove file
            old_pending_edges->skip_indexing(); // will close and remove file
        }

        // process pending finished, clean up the last pending file
        pending_nodes->skip_indexing();
        pending_node_labels->skip_indexing();
        pending_node_properties->skip_indexing();
        pending_edge_properties->skip_indexing();
        pending_edges->skip_indexing();
    }

    // delete all unnecessary files and free-up memory
    ext_helper->clean_up();

    print_duration("Process strings and tensors", start);

    ext_helper->build_disk_hash();

    print_duration("Write strings and tensors hashes", start);

    // we reuse the buffer for external strings in the B+trees creation
    char* const buffer = ext_helper->buffer;
    const auto buffer_size = ext_helper->buffer_size;

    nodes.finish_appends();
    node_labels.finish_appends();
    node_properties.finish_appends();
    edge_properties.finish_appends();
    edges.finish_appends();
    equal_from_to.finish_appends();

    catalog.process_import_keys_labels(keys2id, node_labels2id, edge_labels2id);

    { // Append undeclared nodes (being on an edge)
        std::unordered_set<uint64_t> nodes_set;

        nodes.begin_tuple_iter();
        while (nodes.has_next_tuple()) {
            auto& tuple = nodes.next_tuple();
            nodes_set.insert(tuple[0]);
        }

        edges.begin_tuple_iter();
        while (edges.has_next_tuple()) {
            auto& tuple = edges.next_tuple();
            if (nodes_set.insert(tuple[0]).second) {
                nodes.push_back({ tuple[0] });
            }
            if (nodes_set.insert(tuple[1]).second) {
                nodes.push_back({ tuple[1] });
            }
        }
        // declared_nodes.finish_appends() its called twice, no problem with that
        nodes.finish_appends();
        catalog.process_import_node(nodes_set.size(), current_anon_id);
    }
    print_duration("Write table", start);

    nodes.start_indexing(buffer, buffer_size, { 0 });
    node_labels.start_indexing(buffer, buffer_size, { 0, 1 });
    node_properties.start_indexing(buffer, buffer_size, { 0, 1, 2 });
    edge_properties.start_indexing(buffer, buffer_size, { 0, 1, 2 });
    edges.start_indexing(buffer, buffer_size, { 0, 1, 2, 3 });
    equal_from_to.start_indexing(buffer, buffer_size, { 0, 1, 2 });

    { // Nodes B+Tree
        size_t C_NODE = 0;
        NoStat<1> no_stat;

        nodes.create_bpt(db_folder + "/nodes", { C_NODE }, no_stat);
    }

    { // Node Labels B+Tree
        size_t C_NODE = 0, C_LABEL = 1;

        NoStat<2> no_stat;
        DictCountStat<2> label_stat;

        node_labels.create_bpt(db_folder + "/node_label", { C_NODE, C_LABEL }, no_stat);
        node_labels.create_bpt(db_folder + "/label_node", { C_LABEL, C_NODE }, label_stat);
        label_stat.end();

        catalog.process_import_node_labels(label_stat.all, label_stat.dict);
    }

    { // Node Properties B+Tree
        size_t C_NODE = 0, C_KEY = 1, C_VALUE = 2;

        NoStat<3> no_stat;
        PropStat prop_stat;

        node_properties.create_bpt(db_folder + "/node_key_value", { C_NODE, C_KEY, C_VALUE }, no_stat);
        node_properties.create_bpt(db_folder + "/key_value_node", { C_KEY, C_VALUE, C_NODE }, prop_stat);
        prop_stat.end();

        catalog.process_import_node_keys(prop_stat.all, prop_stat.map_key_count);
    }

    { // Edge Properties B+Tree
        size_t C_EDGE = 0, C_KEY = 1, C_VALUE = 2;

        NoStat<3> no_stat;
        PropStat prop_stat;

        node_properties.create_bpt(db_folder + "/edge_key_value", { C_EDGE, C_KEY, C_VALUE }, no_stat);
        node_properties.create_bpt(db_folder + "/key_value_edge", { C_KEY, C_VALUE, C_EDGE }, prop_stat);
        prop_stat.end();

        catalog.process_import_edge_keys(prop_stat.all, prop_stat.map_key_count);
    }

    { // Quad B+Trees
        size_t C_FROM = 0, C_TO = 1, C_LABEL = 2, C_EDGE = 3;

        NoStat<4> no_stat;
        AllStat<4> all_stat;
        DictCountStat<4> dict_count;

        edges.create_bpt(db_folder + "/from_to_label_edge", { C_FROM, C_TO, C_LABEL, C_EDGE }, all_stat);
        edges.create_bpt(db_folder + "/to_label_from_edge", { C_TO, C_LABEL, C_FROM, C_EDGE }, no_stat);
        edges.create_bpt(db_folder + "/label_from_to_edge", { C_LABEL, C_FROM, C_TO, C_EDGE }, dict_count);
        edges.create_bpt(db_folder + "/label_to_from_edge", { C_LABEL, C_TO, C_FROM, C_EDGE }, no_stat);
        edges.create_bpt(db_folder + "/edge_from_to_label", { C_EDGE, C_FROM, C_TO, C_LABEL }, no_stat);
        dict_count.end();

        catalog.process_import_edges(all_stat.all, dict_count.dict);
    }

    { // FROM=TO LABEL EDGE
        size_t C_FROM_TO = 0, C_LABEL = 1, C_EDGE = 2;

        NoStat<3> no_stat;
        DictCountStat<3> stat;

        equal_from_to.create_bpt(db_folder + "/equal_from_to", { C_FROM_TO, C_LABEL, C_EDGE }, no_stat);
        equal_from_to.create_bpt(db_folder + "/equal_from_to_inverted", { C_LABEL, C_FROM_TO, C_EDGE }, stat);
        stat.end();

        catalog.process_import_equal_from_to(stat.all, stat.dict);
    }

    // calling finish_indexing() closes and removes the file.
    nodes.finish_indexing();
    node_labels.finish_indexing();
    node_properties.finish_indexing();
    edge_properties.finish_indexing();
    edges.finish_indexing();
    equal_from_to.finish_indexing();

    print_duration("Write B+tree indexes", start);
    catalog.print(std::cout);
    print_duration("Total Import", import_start);
}

uint64_t OnDiskImport::get_str_id(char* str, uint64_t str_size)
{
    if (str_size < 8) {
        return Inliner::inline_string(str) | ObjectId::MASK_STR_INL;
    } else {
        return ext_helper->get_or_create_ext(str, str_size, ObjectId::MASK_STR_EXT);
    }
}

void OnDiskImport::print_error()
{
    parsing_errors++;
    WARN("ERROR on line ", current_line);
}

std::vector<std::string> OnDiskImport::split(const std::string& input, const std::string& delimiter)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = input.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(input.substr(start, end - start));
        start = end + delimiter.length();
        end = input.find(delimiter, start);
    }

    // Add the last substring
    tokens.push_back(input.substr(start));

    return tokens;
}

void OnDiskImport::parse_node_files(std::vector<std::unique_ptr<MDBIstreamFile>>& in_nodes)
{
    std::cout << "Importing nodes\n";
    for (auto& in_file : in_nodes) {
        current_state = State::START_HEADER_NODES;
        lexer.begin(*in_file);
        std::cout << "Reading file " << in_file->filename << std::endl;

        while (auto token = lexer.get_token()) {
            current_token = token;
            current_state = get_transition(current_state, token);
        }

        if (current_token != Token::ENDLINE && current_token != Token::END_OF_FILE) {
            process_node_line();
        }

        reset_automata();
    }
}

void OnDiskImport::parse_edge_files(std::vector<std::unique_ptr<MDBIstreamFile>>& in_edges)
{
    std::cout << "Importing edges\n";
    for (auto& in_file : in_edges) {
        current_state = State::START_HEADER_EDGES;
        lexer.begin(*in_file);
        std::cout << "Reading file " << in_file->filename << std::endl;

        while (auto token = lexer.get_token()) {
            current_token = token;
            current_state = get_transition(current_state, token);
        }

        if (current_token != Token::ENDLINE && current_token != Token::END_OF_FILE) {
            save_edge_line();
        }

        reset_automata();
    }
}

void OnDiskImport::reset_automata()
{
    columns.clear();
    current_column = 0;
    current_line = 1;
    anonymous_nodes = true;
    global_ids = true;
    current_group.clear();
    current_group_from.clear();
    current_group_to.clear();
    current_group_idx_from = 0;
    current_group_idx_to = 0;
    column_with_edge_label = 0;
    column_with_id = 0;
}

void OnDiskImport::set_transition(int state, int token, int value, std::function<void()> func)
{
    state_funcs[State::TOTAL_STATES * token + state] = func;
    state_transitions[State::TOTAL_STATES * token + state] = value;
}

int OnDiskImport::get_transition(int state, int token)
{
    state_funcs[State::TOTAL_STATES * token + state]();
    return state_transitions[State::TOTAL_STATES * token + state];
}

void OnDiskImport::save_header_column()
{
    if (current_state == State::START_BODY_NODES)
        current_line++;

    CSVType new_column_type = CSVType::UNDEFINED;
    std::vector<std::string> split_new_col = split(lexer.str, ":");
    if (split_new_col.size() != 1) {
        if (split_new_col[1] == "ID") {
            new_column_type = CSVType::ID;
            if (split_new_col.size() == 3) {
                global_ids = false;
                current_group = split_new_col[2];
                if (!csvid_groups_index.contains(split_new_col[2])) {
                    csvid_groups.push_back(boost::unordered_flat_map<std::string, uint64_t>());
                    csvid_groups_index.insert({ split_new_col[2], group_count });
                    current_group_idx = group_count;
                    group_count++;
                } else {
                    current_group_idx = csvid_groups_index[split_new_col[2]];
                }
            }
            return;
        } else if (split_new_col[1] == "START_ID") {
            new_column_type = CSVType::START_ID;
            if (split_new_col.size() == 3) {
                if (csvid_groups_index.contains(split_new_col[2])) {
                    current_group_idx_from = csvid_groups_index[split_new_col[2]];
                } else {
                    FATAL_ERROR("ERROR reading csv header: Group \"", split_new_col[2], "\" does not exist");
                }
            } else {
                current_group_idx_from = -1;
            }
            return;
        } else if (split_new_col[1] == "END_ID") {
            new_column_type = CSVType::END_ID;
            if (split_new_col.size() == 3) {
                if (csvid_groups_index.contains(split_new_col[2])) {
                    current_group_idx_to = csvid_groups_index[split_new_col[2]];
                } else {
                    FATAL_ERROR("ERROR reading csv header: Group \"", split_new_col[2], "\" does not exist");
                }
            } else {
                current_group_idx_to = -1;
            }
            return;
        } else if (split_new_col[1] == "LABEL") {
            new_column_type = CSVType::LABEL;
            return;
        }

        else if (split_new_col[1] == "STR")
            new_column_type = CSVType::STR;
        else if (split_new_col[1] == "INT")
            new_column_type = CSVType::INT;
        else if (split_new_col[1] == "FLOAT")
            new_column_type = CSVType::DECIMAL;
        else if (split_new_col[1] == "DATE")
            new_column_type = CSVType::DATE;
        else if (split_new_col[1] == "DATETIME")
            new_column_type = CSVType::DATETIME;
        else if (split_new_col[1] == "LIST")
            new_column_type = CSVType::LIST;
        else
            new_column_type = CSVType::UNDEFINED;
    }
    std::string& key = split_new_col[0];
    uint64_t key_id;

    auto it = keys2id.find(key);
    if (it != keys2id.end()) {
        key_id = it->second | ObjectId::MASK_PROPERTY_KEY;
    } else {
        auto new_id = keys2id.size();
        keys2id.insert({ key, new_id });
        key_id = new_id | ObjectId::MASK_PROPERTY_KEY;
    }

    columns.emplace_back(new_column_type, key, key_id);
}

void OnDiskImport::verify_anon()
{
    for (size_t col_idx = 0; col_idx < columns.size(); col_idx++) {
        if (columns[col_idx].type == CSVType::ID) {
            anonymous_nodes = false;
            column_with_id = col_idx;
            break;
        }
    }
    current_line++;
}

void OnDiskImport::verify_edge_file_header()
{
    // For an edges file to be correct, in the columns we should find
    // one START_ID, one END_ID and one TYPE column. Having more or less than that
    // is a bad file and the import should stop (or at least the file should be skipped)

    bool has_start_id = false, has_end_id = false, has_label = false;
    for (int col_idx = 0; col_idx < (int) columns.size(); col_idx++) {
        if (columns[col_idx].type == CSVType::START_ID && !has_start_id) {
            has_start_id = true;
            column_with_id_from = col_idx;
        } else if (columns[col_idx].type == CSVType::START_ID && has_start_id)
            FATAL_ERROR("ERROR reading csv header: More than one START_ID column is present");
        else if (columns[col_idx].type == CSVType::END_ID && !has_end_id) {
            has_end_id = true;
            column_with_id_to = col_idx;
        } else if (columns[col_idx].type == CSVType::END_ID && has_end_id)
            FATAL_ERROR("ERROR reading csv header: More than one END_ID column is present");
        else if (columns[col_idx].type == CSVType::LABEL && !has_label) {
            has_label = true;
            column_with_edge_label = col_idx;
        } else if (columns[col_idx].type == CSVType::LABEL && has_label)
            FATAL_ERROR("ERROR reading csv header: More than one TYPE column is present");
    }

    if (!has_start_id || !has_end_id || !has_label) {
        std::string error = "The following column(s) are missing from the header:";
        if (!has_start_id)
            error += " START_ID";
        if (!has_end_id)
            error += " END_ID";
        if (!has_label)
            error += " LABEL";
        FATAL_ERROR(error);
    }
    current_line++;
}

void OnDiskImport::save_body_column_to_buffer()
{
    std::strcpy(columns[current_column].value_str, lexer.str);
    columns[current_column].value_size = lexer.str_len;

    if (columns[current_column].type == CSVType::UNDEFINED) {
        switch (current_token) {
        case Token::STRING:
            columns[current_column].type = CSVType::STR;
            break;
        case Token::UNQUOTED_STRING:
            columns[current_column].type = CSVType::STR;
            break;
        case Token::INTEGER:
            columns[current_column].type = CSVType::INT;
            break;
        case Token::FLOAT:
            columns[current_column].type = CSVType::DECIMAL;
            break;

        default:
            WARN("line ", current_line, ": Cannot detect type. Please specify in CSV header");
            parsing_errors++;
            break;
        }
    }
}

void OnDiskImport::save_empty_body_column_to_buffer()
{
    columns[current_column].value_size = 0;
    current_column++;
}

void OnDiskImport::process_node_line()
{
    CSVColumn& id_col = columns[column_with_id];
    uint64_t node_id;
    if (anonymous_nodes) {
        node_id = current_anon_id++ | ObjectId::MASK_ANON_INL;
    } else if (id_col.value_size == 0) {
        WARN("line ", current_line, ": No ID was given for the node. It will be saved as an anonymous node");
        parsing_errors++;
        node_id = current_anon_id++ | ObjectId::MASK_ANON_INL;
    } else if (global_ids) {
        // If a node with the same ID exists, do not save.
        if (csvid_global.contains(id_col.value_str)) {
            WARN(
                "line ",
                current_line,
                ": Duplicated ID \"",
                id_col.value_str,
                "\". Node will not be saved."
            );
            parsing_errors++;
            go_to_next_line();
            return;
        }
        // Using global IDs
        bool is_number = true;
        for (int char_idx = 0; char_idx < (int) id_col.value_size; char_idx++) {
            if (!std::isdigit(id_col.value_str[char_idx])) {
                is_number = false;
                break;
            }
        }

        if (is_number) {
            node_id = try_parse_int(id_col.value_str);
        } else {
            normalize_string_literal(id_col);
            if (id_col.value_size < 8)
                node_id = Inliner::inline_string(id_col.value_str) | ObjectId::MASK_NAMED_NODE_INL;
            else
                node_id = ext_helper->get_or_create_ext(
                    id_col.value_str,
                    id_col.value_size,
                    ObjectId::MASK_NAMED_NODE_EXT
                );
        }
        csvid_global.insert({ id_col.value_str, node_id });
    } else {
        // Using IDs inside the scope of a group
        node_id = current_anon_id++ | ObjectId::MASK_ANON_INL;
        if (csvid_groups[current_group_idx].contains(id_col.value_str)) {
            WARN(
                "line ",
                current_line,
                ": Duplicated ID \"",
                id_col.value_str,
                "\" inside group \"",
                current_group,
                "\". Node will not be saved."
            );
            parsing_errors++;
            go_to_next_line();
            return;
        }
        csvid_groups[current_group_idx].insert({ id_col.value_str, node_id });
    }
    try_save_declared_node(node_id);

    for (auto& col : columns) {
        if (col.value_size == 0)
            continue;
        switch (col.type) {
        case CSVType::ID: {
            // The ID column was already worked on. Nothing should be done.
            break;
        }
        case CSVType::LABEL: {
            std::vector<std::string> labels_vector = split(col.value_str, label_splitter);
            for (auto label : labels_vector) {
                uint64_t label_id;
                auto it = node_labels2id.find(label);
                if (it != node_labels2id.end()) {
                    label_id = it->second | ObjectId::MASK_NODE_LABEL;
                } else {
                    auto new_id = node_labels2id.size();
                    node_labels2id.insert({ label, new_id });
                    label_id = new_id | ObjectId::MASK_NODE_LABEL;
                }
                try_save_node_label(node_id, label_id);
            }
            break;
        }
        case CSVType::STR: {
            normalize_string_literal(col);
            uint64_t value_id = get_str_id(col.value_str, col.value_size);

            try_save_node_property(node_id, col.key_id, value_id);
            break;
        }
        case CSVType::INT: {
            uint64_t value_id = try_parse_int(col.value_str);
            try_save_node_property(node_id, col.key_id, value_id);
            break;
        }
        case CSVType::DECIMAL: {
            uint64_t value_id = try_parse_float(col.value_str);
            try_save_node_property(node_id, col.key_id, value_id);
            break;
        }
        case CSVType::DATE: {
            uint64_t value_id = DateTime::from_date(col.value_str);
            if (value_id == ObjectId::NULL_ID) {
                WARN("line ", current_line, ": Invalid date `", col.value_str, '`');
                parsing_errors++;
                break;
            }
            try_save_node_property(node_id, col.key_id, value_id);
            break;
        }
        case CSVType::DATETIME: {
            uint64_t value_id = DateTime::from_dateTime(col.value_str);
            if (value_id == ObjectId::NULL_ID) {
                WARN("line ", current_line, ": Invalid dateTime `", col.value_str, '`');
                parsing_errors++;
                break;
            }
            try_save_node_property(node_id, col.key_id, value_id);
            break;
        }
        case CSVType::LIST: {
            // TODO: asuming every sub item is string
            std::vector<std::string> str_list = split(col.value_str, list_splitter);
            std::vector<ObjectId> oid_list;

            for (auto& elem : str_list) {
                if (elem.size() == 0) {
                    continue;
                }
                uint64_t value_id = get_str_id(elem.data(), elem.size());
                oid_list.push_back(ObjectId(value_id));
            }

            uint64_t encoded_size = ListEncoder::encode(oid_list, list_buffer);
            auto list_id = ext_helper->get_or_create_ext(list_buffer, encoded_size, ObjectId::MASK_LIST_EXT);

            if ((list_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP) {
                pending_node_properties->push_back({ node_id, col.key_id, list_id });
            } else {
                node_properties.push_back({ node_id, col.key_id, list_id });
            }
            break;
        }

        default:
            WARN("line ", current_line, ": Unhandled type");
            parsing_errors++;
            break;
        }
    }

    go_to_next_line();
}

void OnDiskImport::save_edge_line()
{
    if (columns[column_with_edge_label].value_size == 0) {
        WARN(
            "line ",
            current_line,
            ": The edge does not have a type and it is required. The edge will not be saved"
        );
        parsing_errors++;
        go_to_next_line();
        return;
    }
    std::string label(columns[column_with_edge_label].value_str, columns[column_with_edge_label].value_size);
    uint64_t label_id;
    auto it = edge_labels2id.find(label);
    if (it != edge_labels2id.end()) {
        label_id = it->second | ObjectId::MASK_EDGE_LABEL;
    } else {
        auto new_id = edge_labels2id.size();
        edge_labels2id.insert({ label, new_id });
        label_id = new_id | ObjectId::MASK_EDGE_LABEL;
    }

    if (columns[column_with_id_from].value_size == 0 || columns[column_with_id_to].value_size == 0) {
        WARN(
            "line ",
            current_line,
            ": The edge has missing IDs and they are required. The edge will not be saved"
        );
        parsing_errors++;
        go_to_next_line();
        return;
    }

    uint64_t from_id, to_id;
    if (current_group_idx_from == -1) {
        // Using global scope of ids.
        if (!csvid_global.contains(columns[column_with_id_from].value_str)) {
            WARN(
                "line ",
                current_line,
                ": The ID ",
                columns[column_with_id_from].value_str,
                " does not exist in the global scope. Edge will not be saved."
            );
            parsing_errors++;
            go_to_next_line();
            return;
        }
        from_id = csvid_global[columns[column_with_id_from].value_str];
    } else {
        // Using a group of ids.
        if (!csvid_groups[current_group_idx_from].contains(columns[column_with_id_from].value_str)) {
            WARN(
                "line ",
                current_line,
                ": The ID ",
                columns[column_with_id_from].value_str,
                " does not exist in the group ",
                current_group_from,
                ". Edge will not be saved."
            );
            parsing_errors++;
            go_to_next_line();
            return;
        }
        from_id = csvid_groups[current_group_idx_from][columns[column_with_id_from].value_str];
    }

    if (current_group_idx_to == -1) {
        // Using global scope of ids.
        if (!csvid_global.contains(columns[column_with_id_to].value_str)) {
            WARN(
                "line ",
                current_line,
                ": The ID ",
                columns[column_with_id_to].value_str,
                " does not exist in the global scope. Edge will not be saved."
            );
            parsing_errors++;
            go_to_next_line();
            return;
        }
        to_id = csvid_global[columns[column_with_id_to].value_str];
    } else {
        // Using a group of ids.
        if (!csvid_groups[current_group_idx_to].contains(columns[column_with_id_to].value_str)) {
            WARN(
                "line ",
                current_line,
                ": The ID ",
                columns[column_with_id_to].value_str,
                " does not exist in the group ",
                current_group_to,
                ". Edge will not be saved."
            );
            parsing_errors++;
            go_to_next_line();
            return;
        }
        to_id = csvid_groups[current_group_idx_to][columns[column_with_id_to].value_str];
    }

    uint64_t edge_id = edge_count++ | ObjectId::MASK_DIRECTED_EDGE;

    try_save_quad(from_id, to_id, label_id, edge_id);

    for (auto& col : columns) {
        if (col.value_size == 0)
            continue;
        switch (col.type) {
        // START_ID, END_ID and LABEL should do nothing
        case CSVType::START_ID:
        case CSVType::END_ID:
        case CSVType::LABEL:
            break;

        case CSVType::STR: {
            normalize_string_literal(col);
            uint64_t value_id = get_str_id(col.value_str, col.value_size);

            try_save_edge_property(edge_id, col.key_id, value_id);
            break;
        }
        case CSVType::INT: {
            uint64_t value_id = try_parse_int(col.value_str);
            try_save_edge_property(edge_id, col.key_id, value_id);
            break;
        }
        case CSVType::DECIMAL: {
            uint64_t value_id = try_parse_float(col.value_str);
            try_save_edge_property(edge_id, col.key_id, value_id);
            break;
        }
        case CSVType::DATE: {
            uint64_t value_id = DateTime::from_date(col.value_str);
            if (value_id == ObjectId::NULL_ID) {
                WARN("line ", current_line, ": Invalid date `", col.value_str, '`');
                parsing_errors++;
                break;
            }
            try_save_edge_property(edge_id, col.key_id, value_id);
            break;
        }
        case CSVType::DATETIME: {
            uint64_t value_id = DateTime::from_dateTime(col.value_str);
            if (value_id == ObjectId::NULL_ID) {
                WARN("line ", current_line, ": Invalid dateTime `", col.value_str, '`');
                parsing_errors++;
                break;
            }
            try_save_edge_property(edge_id, col.key_id, value_id);
            break;
        }
        case CSVType::LIST: {
            // TODO: assuming every sub item is a string
            std::vector<std::string> str_list = split(col.value_str, list_splitter);
            std::vector<ObjectId> oid_list;

            for (auto& elem : str_list) {
                if (elem.size() == 0) {
                    continue;
                }
                uint64_t value_id = get_str_id(elem.data(), elem.size());
                oid_list.push_back(ObjectId(value_id));
            }

            uint64_t encoded_size = ListEncoder::encode(oid_list, list_buffer);
            auto list_id = ext_helper->get_or_create_ext(list_buffer, encoded_size, ObjectId::MASK_LIST_EXT);

            if ((list_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP) {
                pending_edge_properties->push_back({ edge_id, col.key_id, list_id });
            } else {
                edge_properties.push_back({ edge_id, col.key_id, list_id });
            }
            break;
        }

        default:
            WARN("line ", current_line, ": Unhandled type");
            parsing_errors++;
            break;
        }
    }

    go_to_next_line();
}

void OnDiskImport::go_to_next_line()
{
    current_column = 0;
    current_line++;
}

void OnDiskImport::normalize_string_literal(CSVColumn& col)
{
    char* write_ptr = col.value_str;
    char* read_ptr;
    char* end;
    if (write_ptr[0] == '"') {
        // Remove quotation marks
        read_ptr = write_ptr + 1;
        col.value_size -= 2;
        end = col.value_str + col.value_size + 1;
    } else {
        // Leave the string as it is
        read_ptr = write_ptr;
        end = col.value_str + col.value_size;
    }

    UnicodeEscape::normalize_string(read_ptr, write_ptr, end, col.value_size);
}

void OnDiskImport::try_save_declared_node(uint64_t node_id)
{
    if ((node_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP) {
        pending_nodes->push_back({ node_id });
    } else {
        nodes.push_back({ node_id });
    }
}

void OnDiskImport::try_save_node_label(uint64_t node_id, uint64_t label_id)
{
    if ((node_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP) {
        pending_node_labels->push_back({ node_id, label_id });
    } else {
        node_labels.push_back({ node_id, label_id });
    }
}

void OnDiskImport::try_save_node_property(uint64_t id1, uint64_t key_id, uint64_t value_id)
{
    if ((id1 & ObjectId::MOD_MASK) == ObjectId::MOD_TMP
        || (value_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
    {
        pending_node_properties->push_back({ id1, key_id, value_id });
    } else {
        node_properties.push_back({ id1, key_id, value_id });
    }
}

void OnDiskImport::try_save_edge_property(uint64_t id1, uint64_t key_id, uint64_t value_id)
{
    if ((id1 & ObjectId::MOD_MASK) == ObjectId::MOD_TMP
        || (value_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
    {
        pending_edge_properties->push_back({ id1, key_id, value_id });
    } else {
        edge_properties.push_back({ id1, key_id, value_id });
    }
}

void OnDiskImport::try_save_quad(uint64_t from_id, uint64_t to_id, uint64_t label_id, uint64_t edge_id)
{
    if ((from_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP
        || (to_id & ObjectId::MOD_MASK) == ObjectId::MOD_TMP)
    {
        pending_edges->push_back({ from_id, to_id, label_id, edge_id });
        return;
    }

    edges.push_back({ from_id, to_id, label_id, edge_id });

    if (from_id == to_id) {
        equal_from_to.push_back({ from_id, label_id, edge_id });
    }
}
