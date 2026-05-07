#pragma once

#include "query/executor/binding_iter.h"
#include "query/parser/expr/mql/atom_expr/expr_var_property.h"

#include <memory>

namespace MQL {
class AssignProperties : public BindingIter {
public:
    AssignProperties(std::unique_ptr<BindingIter> child_iter, std::vector<ExprVarProperty> var_properties);

    void _begin(Binding& parent_binding) override;

    void _reset() override;

    bool _next() override;

    void assign_nulls() override;

    void print(std::ostream& os, int indent, bool stats) const override;

    std::unique_ptr<BindingIter> child_iter;

private:
    std::vector<VarId> property_vars;

    std::vector<ExprVarProperty> var_properties;

    Binding* parent_binding;
};
} // namespace MQL
