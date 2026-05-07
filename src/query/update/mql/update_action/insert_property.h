#pragma once

#include "query/id.h"
#include "update_action.h"

namespace MQL {

class InsertProperty : public UpdateAction {
public:
    Id obj;
    std::string key;
    Id val;

    InsertProperty(Id obj, std::string key, Id val) :
        obj(obj),
        key(key),
        val(val)
    { }

    void process(Binding& binding, UpdateContext& ctx) override
    {
        ObjectId obj_ = obj.is_var() ? binding[obj.get_var()] : obj.get_OID();
        ObjectId val_ = val.is_var() ? binding[val.get_var()] : val.get_OID();

        if (obj_.is_null()) {
            throw QueryExecutionException("cannot create a property with null node");
        }

        if (val_.is_null()) {
            throw QueryExecutionException("cannot create a property with null value");
        }

        auto obj_id = transform_if_tmp(obj_).id;
        auto value_id = transform_if_tmp(val_).id;
        if (ObjectId(obj_id).type() == ObjectType::DirectedEdge) {
            auto edge_key = ctx.get_key_id(key);
            ctx.insert_edge_property(obj_id, edge_key, value_id);
        } else {
            // TODO: validate is node? (named or anon)
            auto node_key = ctx.get_key_id(key);
            ctx.insert_node_property(obj_id, node_key, value_id);
        }
    }

    void print(std::ostream& os, int indent) const override
    {
        os << std::string(indent, ' ') << "InsertProperty(" << obj << "," << key << "," << val << ")\n";
    }

    std::set<VarId> get_input_vars() const override
    {
        std::set<VarId> res;
        if (obj.is_var()) {
            res.insert(obj.get_var());
        }
        if (val.is_var()) {
            res.insert(val.get_var());
        }
        return res;
    }

    void accept_visitor(UpdateActionVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};
} // namespace MQL