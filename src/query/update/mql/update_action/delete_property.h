#pragma once

#include "query/id.h"
#include "update_action.h"

namespace MQL {

class DeleteProperty : public UpdateAction {
public:
    Id obj;
    std::string key;

    DeleteProperty(Id obj, std::string key) :
        obj(obj),
        key(key)
    { }

    void process(Binding& binding, UpdateContext& ctx) override
    {
        ObjectId obj_ = obj.is_var() ? binding[obj.get_var()] : obj.get_OID();

        if (obj_.is_null()) {
            return;
        }

        auto obj_id = transform_if_tmp(obj_).id;
        if (ObjectId(obj_id).type() == ObjectType::DirectedEdge) {
            auto edge_key = ctx.get_key_id(key);
            ctx.delete_edge_property(obj_id, edge_key);
        } else {
            // TODO: validate is node? (named or anon)
            auto node_key = ctx.get_key_id(key);
            ctx.delete_node_property(obj_id, node_key);
        }
    }

    void print(std::ostream& os, int indent) const override
    {
        os << std::string(indent, ' ') << "DeleteProperty(" << obj << "," << key << ")\n";
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