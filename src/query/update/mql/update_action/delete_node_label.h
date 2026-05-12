#pragma once

#include "query/id.h"
#include "update_action.h"

namespace MQL {

class DeleteNodeLabel : public UpdateAction {
public:
    Id node;
    std::string label;

    DeleteNodeLabel(Id node, std::string label) :
        node(node),
        label(label)
    { }

    void process(Binding& binding, UpdateContext& ctx) override
    {
        ObjectId node_ = node.is_var() ? binding[node.get_var()] : node.get_OID();

        // TODO: validar es nodo en vez de no es edge?
        if (node_.is_null() || node_.type() == ObjectType::DirectedEdge) {
            return;
        }

        auto node_id = transform_if_tmp(node_);
        auto label_id = ctx.get_node_label_id(label);

        ctx.delete_node_label(node_id, label_id);
    }

    void print(std::ostream& os, int indent) const override
    {
        os << std::string(indent, ' ') << "DeleteNodeLabel(" << node << "," << label << ")\n";
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