#include "node_label_plan.h"

#include "graph_models/quad_model/quad_model.h"
#include "query/executor/binding_iter/index_scan.h"
#include "query/executor/binding_iter/scan_ranges/term.h"
#include "query/query_context.h"
#include "storage/index/leapfrog/leapfrog_bpt_iter.h"

using namespace std;

NodeLabelPlan::NodeLabelPlan(Id node, ObjectId label) :
    node(node),
    label(label),
    node_assigned(node.is_OID())
{ }

void NodeLabelPlan::print(std::ostream& os, int indent) const
{
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
    os << "NodeLabel(";
    os << "node: " << node;
    os << ", label: " << label;
    os << ")";

    os << ",\n";
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
    os << "  ↳ Estimated factor: " << estimate_output_size();
}

double NodeLabelPlan::estimate_cost() const
{
    return /*100.0 +*/ estimate_output_size();
}

double NodeLabelPlan::estimate_output_size() const
{
    double total_nodes = quad_model.catalog.get_nodes_count();
    // double total_labels = quad_model.catalog.get_node_labels_count();

    if (total_nodes == 0) { // to avoid division by 0
        return 0;
    }

    // double label_count = 0;
    // auto it = quad_model.catalog.node_label2total_count.find(label.get_OID().id);
    // if (it != quad_model.catalog.node_label2total_count.end()) {
    //     label_count = static_cast<double>(it->second);
    // }
    double label_count = quad_model.catalog.get_node_label_count(label);

    if (node_assigned) {
        return label_count / total_nodes;
    } else {
        return label_count;
    }
}

void NodeLabelPlan::set_input_vars(const std::set<VarId>& input_vars)
{
    set_input_var(input_vars, node, &node_assigned);
}

// Must be consistent with the index scan returned in get_binding_iter()
std::set<VarId> NodeLabelPlan::get_vars() const
{
    std::set<VarId> result;
    if (node.is_var() && !node_assigned) {
        result.insert(node.get_var());
    }

    return result;
}

/**
 * ╔═╦═══════════════╦═════════════════╦═════════╗
 * ║ ║ Node Assigned ║  Label Assigned ║  Index  ║
 * ╠═╬═══════════════╬═════════════════╬═════════╣
 * ║1║       yes     ║       yes       ║    NL   ║
 * ║2║       yes     ║       no        ║    NL   ║
 * ║3║       no      ║       yes       ║    LN   ║
 * ║4║       no      ║       no        ║    LN   ║
 * ╚═╩═══════════════╩═════════════════╩═════════╝
 */
unique_ptr<BindingIter> NodeLabelPlan::get_binding_iter() const
{
    array<unique_ptr<ScanRange>, 2> ranges;
    // if (node_assigned) {
    //     ranges[0] = ScanRange::get(node, node_assigned);
    //     ranges[1] = std::make_unique<Term>(label);
    //     return make_unique<IndexScan<2>>(*quad_model.node_label, std::move(ranges));
    // } else {
    ranges[0] = std::make_unique<Term>(label);
    ranges[1] = ScanRange::get(node, node_assigned);
    return make_unique<IndexScan<2>>(*quad_model.label_node, std::move(ranges));
    // }
}

bool NodeLabelPlan::get_leapfrog_iter(
    std::vector<std::unique_ptr<LeapfrogIter>>& leapfrog_iters,
    vector<VarId>& var_order,
    uint_fast32_t& enumeration_level
) const
{
    vector<unique_ptr<ScanRange>> initial_ranges;
    vector<VarId> intersection_vars;
    vector<VarId> enumeration_vars;

    // index = INT32_MAX means enumeration, index = -1 means term
    int_fast32_t node_index, label_index;

    // Assign node_index
    if (node.is_OID() || node_assigned) {
        node_index = -1;
    } else {
        node_index = INT32_MAX;
    }

    // Assign label_index
    label_index = -1;

    // search for vars marked as enumeration (INT32_MAX) that are intersection
    // and assign them the correct index
    for (size_t i = 0; i < enumeration_level; i++) {
        if (node_index == INT32_MAX && node.get_var() == var_order[i]) {
            node_index = i;
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

    // node_label
    // if (node_index <= label_index) {
    //     assign(node_index, node);
    //     assign(label_index, label);

    //     leapfrog_iters.push_back(
    //         make_unique<LeapfrogBptIter<2>>(
    //             &get_query_ctx().thread_info.interruption_requested,
    //             *quad_model.node_label,
    //             std::move(initial_ranges),
    //             std::move(intersection_vars),
    //             std::move(enumeration_vars)
    //         )
    //     );
    //     return true;
    // }
    // // label_node
    // else {
    assign(label_index, label);
    assign(node_index, node);

    leapfrog_iters.push_back(
        make_unique<LeapfrogBptIter<2>>(
            &get_query_ctx().thread_info.interruption_requested,
            *quad_model.label_node,
            std::move(initial_ranges),
            std::move(intersection_vars),
            std::move(enumeration_vars)
        )
    );
    return true;
    // }
}
