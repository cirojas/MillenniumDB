#pragma once

#include "query/id.h"
#include "update_action.h"

namespace MQL {
class SetLabel : public UpdateAction {
public:
    Id obj;
    std::string label;

    SetLabel(Id obj, std::string label) :
        obj(obj),
        label(label)
    { }

    void process(Binding& binding, UpdateContext& ctx) override
    {
        ObjectId obj_ = obj.is_var() ? binding[obj.get_var()] : obj.get_OID();

        if (obj_.is_null()) {
            throw QueryExecutionException("cannot insert label to a null node");
        }

        // if (label.is_null()) {
        //     throw QueryExecutionException("cannot set label null label");
        // }

        auto obj_id = transform_if_tmp(obj_).id;
        if (obj_.type() == ObjectType::DirectedEdge) {
            auto edge_label = ctx.get_edge_label_id(label);
            ctx.set_edge_label(obj_id, edge_label);
        } else {
            auto node_label = ctx.get_node_label_id(label);
            ctx.insert_node_label(obj_id, node_label);
        }
    }

    void print(std::ostream& os, int indent) const override
    {
        os << std::string(indent, ' ') << "SetLabelOrType(" << obj << "," << label << ")\n";
    }

    std::set<VarId> get_input_vars() const override
    {
        std::set<VarId> res;
        if (obj.is_var()) {
            res.insert(obj.get_var());
        }
        return res;
    }

    void accept_visitor(UpdateActionVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};

} // namespace MQL