#include "update_executor.h"

using namespace MQL;

uint64_t UpdateExecutor::execute()
{
    Binding binding(get_query_ctx().get_var_size());

    iter->begin(binding);

    while (iter->next()) {
        for (auto& action : update_actions) {
            action->process(binding, *update_context);
        }
    }

    auto& catalog = quad_model.catalog;
    auto& ctx = *update_context;

    int64_t diff_node_labels = 0;
    int64_t diff_node_properties = 0;
    int64_t diff_edge_properties = 0;
    int64_t diff_equal_from_to = 0;

    for (auto&& [label, diff] : ctx.node_label2total_diff) {
        if (diff != 0) {
            diff_node_labels += diff;
            catalog.update_node_label_count(label, diff);
        }
    }
    for (auto&& [edge_label, diff] : ctx.edge_label2total_diff) {
        if (diff != 0) {
            diff_edge_properties += diff;
            catalog.update_edge_label_count(edge_label, diff);
        }
    }
    for (auto&& [edge_label, diff] : ctx.edge_label2equal_from_to_diff) {
        if (diff != 0) {
            diff_equal_from_to += diff;
            catalog.update_equal_from_to_label_count(edge_label, diff);
        }
    }
    for (auto&& [key, diff] : ctx.node_key2total_diff) {
        if (diff != 0) {
            diff_node_properties += diff;
            catalog.update_node_key_count(key, diff);
        }
    }
    for (auto&& [key, diff] : ctx.edge_key2total_diff) {
        if (diff != 0) {
            diff_edge_properties += diff;
            catalog.update_edge_key_count(key, diff);
        }
    }

    catalog.update_max_anon(ctx.current_anon);
    catalog.update_max_edge(ctx.current_edge);

    catalog.update_deleted_edges(update_context->deleted_edges);

    assert(diff_node_labels == ctx.new_node_labels - ctx.deleted_node_labels);
    assert(diff_node_properties == ctx.new_node_properties - ctx.deleted_node_properties);
    assert(diff_edge_properties == ctx.new_edge_properties - ctx.deleted_edge_properties);

    catalog.update_nodes_count(ctx.new_nodes - ctx.deleted_nodes);
    catalog.update_nodes_labels_count(diff_node_labels);
    catalog.update_nodes_properties_count(diff_node_properties);
    catalog.update_edge_properties_count(diff_edge_properties);
    catalog.update_equal_from_to_count(diff_equal_from_to);

    catalog.flush_changes();

    return 0;
}

void UpdateExecutor::analyze(std::ostream& os, bool print_stats, int) const
{
    if (!print_stats) {
        return;
    }

    char s[2] = "\0";
    os << "{";

    if (update_context->new_nodes) {
        os << s << "\"new_nodes\": " << update_context->new_nodes;
        s[0] = ',';
    }
    if (update_context->new_edges) {
        os << s << "\"new_edges\": " << update_context->new_edges;
        s[0] = ',';
    }
    if (update_context->new_node_labels) {
        os << s << "\"new_node_labels\": " << update_context->new_node_labels;
        s[0] = ',';
    }
    if (update_context->new_node_properties) {
        os << s << "\"new_node_properties\": " << update_context->new_node_properties;
        s[0] = ',';
    }
    if (update_context->new_edge_properties) {
        os << s << "\"new_edge_properties\": " << update_context->new_edge_properties;
        s[0] = ',';
    }
    if (update_context->deleted_nodes) {
        os << s << "\"deleted_nodes\": " << update_context->deleted_nodes;
        s[0] = ',';
    }
    if (update_context->deleted_edges) {
        os << s << "\"deleted_edges\": " << update_context->deleted_edges;
        s[0] = ',';
    }
    if (update_context->deleted_node_labels) {
        os << s << "\"deleted_node_labels\": " << update_context->deleted_node_labels;
        s[0] = ',';
    }
    if (update_context->deleted_node_properties) {
        os << s << "\"deleted_node_properties\": " << update_context->deleted_node_properties;
        s[0] = ',';
    }
    if (update_context->deleted_edge_properties) {
        os << s << "\"deleted_edge_properties\": " << update_context->deleted_edge_properties;
        s[0] = ',';
    }
    if (update_context->overwritten_node_properties) {
        os << s << "\"overwritten_node_properties\": " << update_context->overwritten_node_properties;
        s[0] = ',';
    }
    if (update_context->overwritten_edge_properties) {
        os << s << "\"overwritten_edge_properties\": " << update_context->overwritten_edge_properties;
        s[0] = ',';
    }
    os << "}";
}
