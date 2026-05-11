#include "node_property_plan.h"

#include <cassert>

#include "graph_models/quad_model/quad_model.h"
#include "query/executor/binding_iter/index_scan.h"
#include "query/executor/binding_iter/scan_ranges/term.h"
#include "query/query_context.h"
#include "storage/index/leapfrog/leapfrog_bpt_iter.h"

using namespace std;
using namespace MQL;

NodePropertyPlan::NodePropertyPlan(Id node, ObjectId key, Id value) :
    node(node),
    key(key),
    value(value),
    node_assigned(node.is_OID()),
    value_assigned(value.is_OID())
{ }

void NodePropertyPlan::print(std::ostream& os, int indent) const
{
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
    os << "NodeProperty(";
    os << "node: " << node;
    os << ", key: " << key;
    os << ", value: " << value;
    os << ")";

    os << ",\n";
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
    os << "  ↳ Estimated factor: " << estimate_output_size();
}

double NodePropertyPlan::estimate_cost() const
{
    return /*100.0 +*/ estimate_output_size();
}

double NodePropertyPlan::estimate_output_size() const
{
    auto total_edges = static_cast<double>(quad_model.catalog.get_nodes_count());
    auto total_properties = static_cast<double>(quad_model.catalog.get_node_properties_count());
    auto properties_with_key = quad_model.catalog.get_node_property_count(key);

    if (total_edges == 0 || total_properties == 0) {
         // To avoid division by 0
        return 0;
    }
    if (value_assigned) {
        if (node_assigned) {
            return properties_with_key / (total_properties * total_edges);
        } else {
            return properties_with_key / total_properties;
        }
    } else {
        if (node_assigned) {
            return properties_with_key / total_edges;
        } else {
            return properties_with_key;
        }
    }
}

void NodePropertyPlan::set_input_vars(const std::set<VarId>& input_vars)
{
    set_input_var(input_vars, node, &node_assigned);
    set_input_var(input_vars, value, &value_assigned);
}

std::set<VarId> NodePropertyPlan::get_vars() const
{
    std::set<VarId> result;
    if (node.is_var() && !node_assigned) {
        result.insert(node.get_var());
    }
    if (value.is_var() && !value_assigned) {
        result.insert(value.get_var());
    }

    return result;
}

/**
 * ╔═╦══════════════╦═══════════════╦══════════════════╦═════════╗
 * ║ ║  KeyAssigned ║ ValueAssigned ║   NodeAssigned   ║  Index  ║
 * ╠═╬══════════════╬═══════════════╬══════════════════╬═════════╣
 * ║1║      yes     ║      yes      ║        yes       ║   NKV   ║
 * ║2║      yes     ║      yes      ║        no        ║   KVN   ║
 * ║3║      yes     ║      no       ║        yes       ║   NKV   ║
 * ║4║      yes     ║      no       ║        no        ║   KVN   ║
 * ║5║      no      ║      yes      ║        yes       ║  ERROR  ║
 * ║6║      no      ║      yes      ║        no        ║  ERROR  ║
 * ║7║      no      ║      no       ║        yes       ║   NKV   ║
 * ║8║      no      ║      no       ║        no        ║   KVN   ║
 * ╚═╩══════════════╩═══════════════╩══════════════════╩═════════╝
 */
unique_ptr<BindingIter> NodePropertyPlan::get_binding_iter() const
{
    array<unique_ptr<ScanRange>, 3> ranges;

    if (node_assigned) {
        ranges[0] = ScanRange::get(node, node_assigned);
        ranges[1] = std::make_unique<Term>(key);
        ranges[2] = ScanRange::get(value, value_assigned);
        return make_unique<IndexScan<3>>(*quad_model.node_key_value, std::move(ranges));
    } else {
        ranges[0] = std::make_unique<Term>(key);
        ranges[1] = ScanRange::get(value, value_assigned);
        ranges[2] = ScanRange::get(node, node_assigned);
        return make_unique<IndexScan<3>>(*quad_model.key_value_node, std::move(ranges));
    }
}

bool NodePropertyPlan::get_leapfrog_iter(
    std::vector<std::unique_ptr<LeapfrogIter>>& leapfrog_iters,
    vector<VarId>& var_order,
    uint_fast32_t& enumeration_level
) const
{
    vector<unique_ptr<ScanRange>> initial_ranges;
    vector<VarId> intersection_vars;
    vector<VarId> enumeration_vars;

    // index = INT32_MAX means enumeration, index = -1 means term
    int_fast32_t node_index, key_index, value_index;

    // Assign node_index
    if (node.is_OID() || node_assigned) {
        node_index = -1;
    } else {
        node_index = INT32_MAX;
    }

    key_index = -1;

    // Assign value_index
    if (value.is_OID() || value_assigned) {
        value_index = -1;
    } else {
        value_index = INT32_MAX;
    }

    // search for vars marked as enumeration (INT32_MAX) that are intersection
    // and assign them the correct index
    for (size_t i = 0; i < enumeration_level; i++) {
        if (node_index == INT32_MAX && node.get_var() == var_order[i]) {
            node_index = i;
        }
        if (value_index == INT32_MAX && value.get_var() == var_order[i]) {
            value_index = i;
        }
    }

    auto assign =
        [&initial_ranges, &enumeration_vars, &intersection_vars](int_fast32_t& index, Id id) -> void {
        if (index == -1) {
            initial_ranges.push_back(ScanRange::get(id, true));
        } else if (index == INT32_MAX) {
            enumeration_vars.push_back(id.get_var());
        } else {
            intersection_vars.push_back(id.get_var());
        }
    };

    // node_key_value
    if (node_index <= key_index && key_index <= value_index) {
        assign(node_index, node);
        assign(key_index, key);
        assign(value_index, value);

        leapfrog_iters.push_back(make_unique<LeapfrogBptIter<3>>(
            &get_query_ctx().thread_info.interruption_requested,
            *quad_model.node_key_value,
            std::move(initial_ranges),
            std::move(intersection_vars),
            std::move(enumeration_vars)
        ));
        return true;
    }
    // key_value_node
    else if (key_index <= value_index && value_index <= node_index)
    {
        assign(key_index, key);
        assign(value_index, value);
        assign(node_index, node);

        leapfrog_iters.push_back(make_unique<LeapfrogBptIter<3>>(
            &get_query_ctx().thread_info.interruption_requested,
            *quad_model.key_value_node,
            std::move(initial_ranges),
            std::move(intersection_vars),
            std::move(enumeration_vars)
        ));
        return true;
    } else {
        return false;
    }
}
