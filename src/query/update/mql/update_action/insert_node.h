#pragma once

#include "query/id.h"
#include "update_action.h"

namespace MQL {

class InsertNode : public UpdateAction {
public:
    Id node;

    InsertNode(Id node) :
        node(node)
    { }

    void process(Binding& binding, UpdateContext& ctx) override
    {
        ObjectId node_ = node.is_var() ? binding[node.get_var()] : node.get_OID();

        if (node_.is_null()) {
            throw QueryExecutionException("cannot insert a null node");
        }

        if (node_.type() == ObjectType::DirectedEdge) {
            return;
        }

        auto node_id = transform_if_tmp(node_).id;
        ctx.insert_node(node_id);
    }

    void print(std::ostream& os, int indent) const override
    {
        os << std::string(indent, ' ') << "InsertNode(" << node << ")\n";
    }

    std::set<VarId> get_input_vars() const override
    {
        std::set<VarId> res;
        if (node.is_var()) {
            res.insert(node.get_var());
        }
        return res;
    }

    void accept_visitor(UpdateActionVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};
} // namespace MQL