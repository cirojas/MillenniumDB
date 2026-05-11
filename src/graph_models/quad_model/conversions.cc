#include "conversions.h"

#include "graph_models/quad_model/quad_model.h"

ObjectId MQL::Conversions::get_key_id(const std::string& str)
{
    auto id = quad_model.catalog.get_key_id(str);
    if (id != ObjectId::MASK_NOT_FOUND) {
        id |= ObjectId::MASK_PROPERTY_KEY;
    }
    return ObjectId(id);
}

ObjectId MQL::Conversions::get_node_label_id(const std::string& str)
{
    auto id = quad_model.catalog.get_node_label_id(str);
    if (id != ObjectId::MASK_NOT_FOUND) {
        id |= ObjectId::MASK_NODE_LABEL;
    }
    return ObjectId(id);
}

ObjectId MQL::Conversions::get_edge_label_id(const std::string& str)
{
    auto id = quad_model.catalog.get_edge_label_id(str);
    if (id != ObjectId::MASK_NOT_FOUND) {
        id |= ObjectId::MASK_EDGE_LABEL;
    }
    return ObjectId(id);
}

std::string MQL::Conversions::get_key(ObjectId oid)
{
    return quad_model.catalog.get_key(oid.get_value());
}

std::string MQL::Conversions::get_node_label(ObjectId oid)
{
    return quad_model.catalog.get_node_label(oid.get_value());
}

std::string MQL::Conversions::get_edge_label(ObjectId oid)
{
    return quad_model.catalog.get_edge_label(oid.get_value());
}
