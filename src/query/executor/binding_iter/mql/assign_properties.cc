#include "assign_properties.h"

#include "graph_models/quad_model/quad_model.h"

#include <cassert>

using namespace MQL;

AssignProperties::AssignProperties(
    std::unique_ptr<BindingIter> child_iter,
    std::vector<ExprVarProperty> var_properties
) :
    child_iter(std::move(child_iter)),
    var_properties(std::move(var_properties))
{ }

void AssignProperties::_begin(Binding& _parent_binding)
{
    parent_binding = &_parent_binding;
    child_iter->begin(*parent_binding);
}

void AssignProperties::_reset()
{
    child_iter->reset();
}

bool AssignProperties::_next()
{
    if (!child_iter->next()) {
        return false;
    }
    for (auto& e : var_properties) {
        auto obj = (*parent_binding)[e.var_without_property];
        BptIter<3> it;
        Record<3> min = { obj.id, e.key.id, 0 };
        Record<3> max = { obj.id, e.key.id, UINT64_MAX };
        if (obj.type() == ObjectType::DirectedEdge) {
            it = quad_model.edge_key_value
                     ->get_range(&get_query_ctx().thread_info.interruption_requested, min, max);

        } else {
            it = quad_model.node_key_value
                     ->get_range(&get_query_ctx().thread_info.interruption_requested, min, max);
        }
        if (auto next = it.next()) {
            uint64_t id = (*next)[2];
            parent_binding->add(e.var_with_property, ObjectId(id));
        } else {
            parent_binding->add(e.var_with_property, ObjectId::get_null());
        }
    }

    return true;
}

void AssignProperties::assign_nulls()
{
    child_iter->assign_nulls();
    for (auto& e : var_properties) {
        parent_binding->add(e.var_with_property, ObjectId::get_null());
    }
}

void AssignProperties::print(std::ostream& os, int indent, bool stats) const
{
    if (stats) {
        print_generic_stats(os, indent);
    }

    os << std::string(indent, ' ') << "AssignProperties(";
    os << property_vars[0];
    for (std::size_t i = 1; i < property_vars.size(); ++i) {
        os << ", " << property_vars[i];
    }
    os << ")\n";

    child_iter->print(os, indent + 2, stats);
}
