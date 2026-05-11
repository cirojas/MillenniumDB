#pragma once

#include "query/optimizer/plan/plan.h"

class NodeLabelPlan : public Plan {
public:
    NodeLabelPlan(Id node, ObjectId label);

    NodeLabelPlan(const NodeLabelPlan& other) :
        node(other.node),
        label(other.label),
        node_assigned(other.node_assigned)
    { }

    std::unique_ptr<Plan> clone() const override
    {
        return std::make_unique<NodeLabelPlan>(*this);
    }

    int relation_size() const override
    {
        return 2;
    }

    double estimate_cost() const override;
    double estimate_output_size() const override;

    std::set<VarId> get_vars() const override;
    void set_input_vars(const std::set<VarId>& input_vars) override;

    std::unique_ptr<BindingIter> get_binding_iter() const override;

    bool get_leapfrog_iter(
        std::vector<std::unique_ptr<LeapfrogIter>>& leapfrog_iters,
        std::vector<VarId>& var_order,
        uint_fast32_t& enumeration_level
    ) const override;

    void print(std::ostream& os, int indent) const override;

private:
    Id node;
    ObjectId label;

    bool node_assigned;
};
