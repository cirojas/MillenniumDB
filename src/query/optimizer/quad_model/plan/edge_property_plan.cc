#include "edge_property_plan.h"

#include <cassert>

#include "graph_models/quad_model/quad_model.h"
#include "query/executor/binding_iter/index_scan.h"
#include "query/executor/binding_iter/scan_ranges/term.h"
#include "query/query_context.h"
#include "storage/index/leapfrog/leapfrog_bpt_iter.h"

using namespace std;

EdgePropertyPlan::EdgePropertyPlan(Id edge, ObjectId key, Id value) :
    edge(edge),
    key(key),
    value(value),
    edge_assigned(edge.is_OID()),
    value_assigned(value.is_OID())
{ }

void EdgePropertyPlan::print(std::ostream& os, int indent) const
{
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
    os << "EdgeProperty(";
    os << "edge: " << edge;
    os << ", key: " << key;
    os << ", value: " << value;
    os << ")";

    os << ",\n";
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
    os << "  ↳ Estimated factor: " << estimate_output_size();
}

double EdgePropertyPlan::estimate_cost() const
{
    return /*100.0 +*/ estimate_output_size();
}

double EdgePropertyPlan::estimate_output_size() const
{
    auto total_edges = static_cast<double>(quad_model.catalog.get_edges_count());
    auto total_properties = static_cast<double>(quad_model.catalog.get_edge_properties_count());
    auto properties_with_key = quad_model.catalog.get_edge_property_count(key);

    if (total_edges == 0 || total_properties == 0) {
         // To avoid division by 0
        return 0;
    }
    if (value_assigned) {
        if (edge_assigned) {
            return properties_with_key / (total_properties * total_edges);
        } else {
            return properties_with_key / total_properties;
        }
    } else {
        if (edge_assigned) {
            return properties_with_key / total_edges;
        } else {
            return properties_with_key;
        }
    }
}

void EdgePropertyPlan::set_input_vars(const std::set<VarId>& input_vars)
{
    set_input_var(input_vars, edge, &edge_assigned);
    set_input_var(input_vars, value, &value_assigned);
}

std::set<VarId> EdgePropertyPlan::get_vars() const
{
    std::set<VarId> result;
    if (edge.is_var() && !edge_assigned) {
        result.insert(edge.get_var());
    }
    if (value.is_var() && !value_assigned) {
        result.insert(value.get_var());
    }

    return result;
}

/**
 * ╔═╦══════════════╦═══════════════╦══════════════════╦═════════╗
 * ║ ║  KeyAssigned ║ ValueAssigned ║  ObjectAssigned  ║  Index  ║
 * ╠═╬══════════════╬═══════════════╬══════════════════╬═════════╣
 * ║1║      yes     ║      yes      ║        yes       ║   OKV   ║
 * ║2║      yes     ║      yes      ║        no        ║   KVO   ║
 * ║3║      yes     ║      no       ║        yes       ║   OKV   ║
 * ║4║      yes     ║      no       ║        no        ║   KVO   ║
 * ║5║      no      ║      yes      ║        yes       ║  ERROR  ║
 * ║6║      no      ║      yes      ║        no        ║  ERROR  ║
 * ║7║      no      ║      no       ║        yes       ║   OKV   ║
 * ║8║      no      ║      no       ║        no        ║   KVO   ║
 * ╚═╩══════════════╩═══════════════╩══════════════════╩═════════╝
 */
unique_ptr<BindingIter> EdgePropertyPlan::get_binding_iter() const
{
    array<unique_ptr<ScanRange>, 3> ranges;

    if (edge_assigned) {
        ranges[0] = ScanRange::get(edge, edge_assigned);
        ranges[1] = std::make_unique<Term>(key);
        ranges[2] = ScanRange::get(value, value_assigned);
        return make_unique<IndexScan<3>>(*quad_model.edge_key_value, std::move(ranges));
    } else {
        ranges[0] = std::make_unique<Term>(key);
        ranges[1] = ScanRange::get(value, value_assigned);
        ranges[2] = ScanRange::get(edge, edge_assigned);
        return make_unique<IndexScan<3>>(*quad_model.key_value_edge, std::move(ranges));
    }
}

bool EdgePropertyPlan::get_leapfrog_iter(
    std::vector<std::unique_ptr<LeapfrogIter>>& leapfrog_iters,
    vector<VarId>& var_order,
    uint_fast32_t& enumeration_level
) const
{
    vector<unique_ptr<ScanRange>> initial_ranges;
    vector<VarId> intersection_vars;
    vector<VarId> enumeration_vars;

    // index = INT32_MAX means enumeration, index = -1 means term
    int_fast32_t edge_index, key_index, value_index;

    // Assign edge_index
    if (edge.is_OID() || edge_assigned) {
        edge_index = -1;
    } else {
        edge_index = INT32_MAX;
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
        if (edge_index == INT32_MAX && edge.get_var() == var_order[i]) {
            edge_index = i;
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

    // edge_key_value
    if (edge_index <= key_index && key_index <= value_index) {
        assign(edge_index, edge);
        assign(key_index, key);
        assign(value_index, value);

        leapfrog_iters.push_back(
            make_unique<LeapfrogBptIter<3>>(
                &get_query_ctx().thread_info.interruption_requested,
                *quad_model.edge_key_value,
                std::move(initial_ranges),
                std::move(intersection_vars),
                std::move(enumeration_vars)
            )
        );
        return true;
    }
    // key_value_edge
    else if (key_index <= value_index && value_index <= edge_index)
    {
        assign(key_index, key);
        assign(value_index, value);
        assign(edge_index, edge);

        leapfrog_iters.push_back(
            make_unique<LeapfrogBptIter<3>>(
                &get_query_ctx().thread_info.interruption_requested,
                *quad_model.key_value_edge,
                std::move(initial_ranges),
                std::move(intersection_vars),
                std::move(enumeration_vars)
            )
        );
        return true;
    } else {
        return false;
    }
}
