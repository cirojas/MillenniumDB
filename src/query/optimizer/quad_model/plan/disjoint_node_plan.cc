#include "disjoint_node_plan.h"

#include "graph_models/quad_model/quad_model.h"
#include "query/executor/binding_iter/index_scan.h"
#include "query/executor/binding_iter/scan_ranges/unassigned_var.h"

double DisjointNodePlan::estimate_cost() const
{
    return /*100.0 +*/ estimate_output_size();
}

void DisjointNodePlan::print(std::ostream& os, int indent) const
{
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
    os << "DisjointNode(" << get_query_ctx().get_var_name(object_var) << ")";

    os << ",\n";
    for (int i = 0; i < indent; ++i) {
        os << ' ';
    }
    os << "  ↳ Estimated factor: " << estimate_output_size();
}

double DisjointNodePlan::estimate_output_size() const
{
    return quad_model.catalog.edge_count() + quad_model.catalog.nodes_count;
}

std::set<VarId> DisjointNodePlan::get_vars() const
{
    std::set<VarId> result;
    result.insert(object_var);
    return result;
}

void DisjointNodePlan::set_input_vars(const std::set<VarId>& /*input_vars*/)
{
    // no need to do anything
}

std::unique_ptr<BindingIter> DisjointNodePlan::get_binding_iter() const
{
    std::array<std::unique_ptr<ScanRange>, 1> ranges;
    ranges[0] = std::make_unique<UnassignedVar>(object_var);
    return std::make_unique<IndexScan<1>>(*quad_model.nodes, std::move(ranges));
}
