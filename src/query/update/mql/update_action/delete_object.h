#pragma once

#include "query/id.h"
#include "update_action.h"

namespace MQL {

class DeleteObject : public UpdateAction {
public:
    Id obj;
    bool detach;

    DeleteObject(Id obj, bool detach) :
        obj(obj),
        detach(detach)
    { }

    void process(Binding& binding, UpdateContext& ctx) override
    {
        ObjectId obj_ = obj.is_var() ? binding[obj.get_var()] : obj.get_OID();

        if (obj_.is_null()) {
            return;
        }
        auto obj_id = transform_if_tmp(obj_);

        if (ObjectId(obj_id).type() == ObjectType::DirectedEdge) {
            ctx.delete_edge(obj_id);
        } else {
            // TODO: validate node?
            ctx.delete_node(obj_id, detach);
        }
    }

    void print(std::ostream& os, int indent) const override
    {
        os << std::string(indent, ' ') << "DeleteObject(" << obj
           << ", DETACH: " << (detach ? "true" : "false") << ")\n";
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