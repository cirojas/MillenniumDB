#pragma once

#include "query/executor/binding_iter/binding_expr/binding_expr.h"
#include "query/id.h"
#include "query/parser/expr/mql/expr.h"
#include "update_action.h"

namespace MQL {

class InsertPropertyExpr : public UpdateAction {
public:
    Id obj;
    std::string key;
    std::unique_ptr<Expr> value;
    std::unique_ptr<BindingExpr> binding_expr;

    InsertPropertyExpr(Id obj, std::string key, std::unique_ptr<Expr> value) :
        obj(obj),
        key(key),
        value(std::move(value))
    { }

    void process(Binding& binding, UpdateContext& ctx) override
    {
        assert(binding_expr != nullptr);
        auto val_ = binding_expr->eval(binding);

        if (val_.is_null()) {
            throw QueryExecutionException("cannot create a property with null value");
        }

        ObjectId obj_ = obj.is_var() ? binding[obj.get_var()] : obj.get_OID();
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
        os << std::string(indent, ' ');
        os << "InsertPropertyExpr(" << obj << "," << key << "," << *value << ")\n";
    }

    std::set<VarId> get_input_vars() const override
    {
        std::set<VarId> res;
        if (obj.is_var()) {
            res.insert(obj.get_var());
        }
        for (auto& v : value->get_input_vars()) {
            res.insert(v);
        }
        return res;
    }

    void accept_visitor(UpdateActionVisitor& visitor) override {
        visitor.visit(*this);
    }
};
} // namespace MQL