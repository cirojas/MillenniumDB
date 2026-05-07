#pragma once

#include "query/id.h"
#include "update_action.h"

namespace MQL {
class InsertEdge : public UpdateAction {
public:
    Id from;
    Id to;
    std::string label;
    VarId edge;

    InsertEdge(Id from, Id to, std::string label, VarId edge) :
        from(from),
        to(to),
        label(label),
        edge(edge)
    { }

    void process(Binding& binding, UpdateContext& ctx) override
    {
        ObjectId from_ = from.is_var() ? binding[from.get_var()] : from.get_OID();
        ObjectId to_ = to.is_var() ? binding[to.get_var()] : to.get_OID();

        if (from_.is_null() || to_.is_null()) {
            throw QueryExecutionException("cannot create an edge using a null node");
        }

        auto edge_id = ctx.get_new_edge_id().id;
        binding.add(edge, ObjectId(edge_id));

        auto from_id = transform_if_tmp(from_).id;
        auto to_id = transform_if_tmp(to_).id;
        auto label_id = ctx.get_edge_label_id(label);

        ctx.insert_edge(from_id, to_id, label_id, edge_id);
    }

    void print(std::ostream& os, int indent) const override
    {
        os << std::string(indent, ' ') << "InsertEdge(" << from << "," << to << "," << label << "," << edge
           << ")\n";
    }

    std::set<VarId> get_input_vars() const override
    {
        std::set<VarId> res;
        if (from.is_var()) {
            res.insert(from.get_var());
        }
        if (to.is_var()) {
            res.insert(to.get_var());
        }
        return res;
    }

    void accept_visitor(UpdateActionVisitor& visitor) override
    {
        visitor.visit(*this);
    }
};
} // namespace MQL