#pragma once

#include "graph_models/object_id.h"
#include "graph_models/quad_model/quad_model.h"
#include "query/exceptions.h"

#include <boost/unordered/unordered_flat_map.hpp>

namespace MQL {

class CreateHNSWIndex;
class CreateTextIndex;

class UpdateContext {

public:
    int64_t new_nodes = 0;
    int64_t new_edges = 0;
    int64_t new_node_labels = 0;
    int64_t new_node_properties = 0;
    int64_t new_edge_properties = 0;
    int64_t deleted_nodes = 0;
    int64_t deleted_edges = 0;
    int64_t deleted_node_labels = 0;
    int64_t deleted_node_properties = 0;
    int64_t deleted_edge_properties = 0;
    int64_t overwritten_node_properties = 0;
    int64_t overwritten_edge_properties = 0;
    int64_t overwritten_edges = 0;

    int64_t hnsw_index_inserts = 0;
    int64_t hnsw_index_deletes = 0;
    int64_t text_index_inserts = 0;
    int64_t text_index_deletes = 0;

    // TODO: add here new? delay writing in catalog till end?
    // TODO: maybe use map?
    std::vector<std::pair<std::string, uint64_t>> new_catalog_keys;
    std::vector<std::pair<std::string, uint64_t>> new_catalog_node_labels;
    std::vector<std::pair<std::string, uint64_t>> new_catalog_edge_labels;

    // IMPORTANT: stats may be negative, use int64_t
    boost::unordered_flat_map<ObjectId, int64_t, OIDHasher> node_label2total_diff;
    boost::unordered_flat_map<ObjectId, int64_t, OIDHasher> edge_label2total_diff;
    boost::unordered_flat_map<ObjectId, int64_t, OIDHasher> node_key2total_diff;
    boost::unordered_flat_map<ObjectId, int64_t, OIDHasher> edge_key2total_diff;
    boost::unordered_flat_map<ObjectId, int64_t, OIDHasher> edge_label2equal_from_to_diff;

    boost::unordered_flat_map<ObjectId, std::vector<std::string>, OIDHasher> indexed_keys;

    uint64_t current_anon;

    uint64_t current_edge;

    UpdateContext();

    void process_new_property(ObjectId obj, ObjectId key, ObjectId val);

    void process_deleted_property(ObjectId obj, ObjectId key, ObjectId val);

    void create_hnsw_index(CreateHNSWIndex&);

    void create_text_index(CreateTextIndex&);

    ObjectId get_edge_label_id(const std::string&);
    ObjectId get_node_label_id(const std::string&);
    ObjectId get_key_id(const std::string&);

    void insert_node(ObjectId node)
    {
        if (quad_model.nodes->insert({ node.id })) {
            new_nodes++;
        }
    }

    void insert_node_label(ObjectId node, ObjectId label)
    {
        if (quad_model.label_node->insert({ label.id, node.id })) {
            quad_model.node_label->insert({ node.id, label.id });

            new_node_labels++;
            node_label2total_diff[label]++;
        }
    }

    void insert_node_property(ObjectId node, ObjectId key, ObjectId val)
    {
        bool interruption = false;

        // Check if the node has a property with the same key
        Record<3> min_range = { node.id, key.id, 0 };
        Record<3> max_range = { node.id, key.id, UINT64_MAX };
        auto prop_iter = quad_model.node_key_value->get_range(&interruption, min_range, max_range);
        const auto existing_record = prop_iter.next();

        if (existing_record != nullptr) {
            ObjectId old_node((*existing_record)[0]);
            ObjectId old_key((*existing_record)[1]);
            ObjectId old_val((*existing_record)[2]);

            // The node has a property with the same key
            if (val == old_val) {
                // The exact same record, nothing to do
                return;
            }

            // Overwrite the old value
            quad_model.node_key_value->delete_record(*existing_record);
            quad_model.key_value_node->delete_record(*existing_record);
            quad_model.node_key_value->insert({ node.id, key.id, val.id });
            quad_model.key_value_node->insert({ key.id, val.id, node.id });

            overwritten_node_properties++;

            process_deleted_property(old_node, old_key, old_val);
            process_new_property(node, key, val);
        } else {
            // The node does not have a property with the same key, create a new one
            quad_model.node_key_value->insert({ node.id, key.id, val.id });
            quad_model.key_value_node->insert({ key.id, val.id, node.id });

            process_new_property(node, key, val);
            node_key2total_diff[key]++;
            new_node_properties++;
        }
    }

    void insert_edge_property(ObjectId edge, ObjectId key, ObjectId val)
    {
        bool interruption = false;

        // Check if the node has a property with the same key
        Record<3> min_range = { edge.id, key.id, 0 };
        Record<3> max_range = { edge.id, key.id, UINT64_MAX };
        auto prop_iter = quad_model.edge_key_value->get_range(&interruption, min_range, max_range);
        const auto existing_record = prop_iter.next();

        if (existing_record != nullptr) {
            ObjectId old_edge((*existing_record)[0]);
            ObjectId old_key((*existing_record)[1]);
            ObjectId old_val((*existing_record)[2]);

            // The node has a property with the same key
            if (val == old_val) {
                // The exact same record, nothing to do
                return;
            }

            // Overwrite the old value
            quad_model.edge_key_value->delete_record(*existing_record);
            quad_model.key_value_edge->delete_record(*existing_record);
            quad_model.edge_key_value->insert({ edge.id, key.id, val.id });
            quad_model.key_value_edge->insert({ key.id, val.id, edge.id });

            overwritten_edge_properties++;

            process_deleted_property(old_edge, old_key, old_val);
            process_new_property(edge, key, val);
        } else {
            // The node does not have a property with the same key, create a new one
            quad_model.edge_key_value->insert({ edge.id, key.id, val.id });
            quad_model.key_value_edge->insert({ key.id, val.id, edge.id });

            process_new_property(edge, key, val);
            edge_key2total_diff[key]++;
            new_edge_properties++;
        }
    }

    void insert_edge(ObjectId from, ObjectId to, ObjectId label, ObjectId edge)
    {
        // edge is always new
        quad_model.from_to_label_edge->insert({ from.id, to.id, label.id, edge.id });
        quad_model.to_label_from_edge->insert({ to.id, label.id, from.id, edge.id });
        quad_model.label_from_to_edge->insert({ label.id, from.id, to.id, edge.id });
        quad_model.label_to_from_edge->insert({ label.id, to.id, from.id, edge.id });
        quad_model.edge_from_to_label->insert({ edge.id, from.id, to.id, label.id });

        new_edges++;
        edge_label2total_diff[label]++;

        if (from == to) {
            quad_model.equal_from_to->insert({ from.id, label.id, edge.id });
            quad_model.equal_from_to_inv->insert({ label.id, from.id, edge.id });
            edge_label2equal_from_to_diff[label]++;
        }
    }

    void set_edge_label(ObjectId edge, ObjectId new_label)
    {
        bool interruption = false;

        // Check if the node has a property with the same key
        Record<4> min_range = { edge.id, 0, 0, 0 };
        Record<4> max_range = { edge.id, UINT64_MAX, UINT64_MAX, UINT64_MAX };
        auto iter = quad_model.edge_from_to_label->get_range(&interruption, min_range, max_range);

        if (auto existing_record = iter.next()) {
            ObjectId from((*existing_record)[1]);
            ObjectId to((*existing_record)[2]);
            ObjectId old_type((*existing_record)[3]);

            if (old_type == new_label) {
                return;
            }

            quad_model.edge_from_to_label->delete_record({ edge.id, from.id, to.id, old_type.id });
            quad_model.from_to_label_edge->delete_record({ from.id, to.id, old_type.id, edge.id });
            quad_model.to_label_from_edge->delete_record({ to.id, old_type.id, from.id, edge.id });
            quad_model.label_from_to_edge->delete_record({ old_type.id, from.id, to.id, edge.id });
            quad_model.label_to_from_edge->delete_record({ old_type.id, to.id, from.id, edge.id });

            quad_model.edge_from_to_label->insert({ edge.id, from.id, to.id, new_label.id });
            quad_model.from_to_label_edge->insert({ from.id, to.id, new_label.id, edge.id });
            quad_model.to_label_from_edge->insert({ to.id, new_label.id, from.id, edge.id });
            quad_model.label_from_to_edge->insert({ new_label.id, from.id, to.id, edge.id });
            quad_model.label_to_from_edge->insert({ new_label.id, to.id, from.id, edge.id });

            overwritten_edges++;

            // delete equal cases
            if (from == to) {
                quad_model.equal_from_to->delete_record({ from.id, old_type.id, edge.id });
                quad_model.equal_from_to_inv->delete_record({ old_type.id, from.id, edge.id });
                edge_label2equal_from_to_diff[old_type]--;
            }

            // insert equal cases
            if (from == to) {
                quad_model.equal_from_to->insert({ from.id, new_label.id, edge.id });
                quad_model.equal_from_to_inv->insert({ new_label.id, from.id, edge.id });
                edge_label2equal_from_to_diff[new_label]++;
            }
        }
    }

    ObjectId get_new_edge_id()
    {
        return ObjectId(ObjectId::MASK_DIRECTED_EDGE | current_edge++);
    }

    ObjectId get_anon_id()
    {
        return ObjectId(ObjectId::MASK_ANON_INL | current_edge++);
    }

    void delete_edge(ObjectId edge)
    {
        bool interruption = false;

        const Record<4> min_range = { edge.id, 0, 0, 0 };
        const Record<4> max_range = { edge.id, UINT64_MAX, UINT64_MAX, UINT64_MAX };

        auto iter = quad_model.edge_from_to_label->get_range(&interruption, min_range, max_range);

        if (auto existing_record = iter.next()) {
            deleted_edges++;

            ObjectId from((*existing_record)[1]);
            ObjectId to((*existing_record)[2]);
            ObjectId label((*existing_record)[3]);

            quad_model.edge_from_to_label->delete_record({ edge.id, from.id, to.id, label.id });
            quad_model.from_to_label_edge->delete_record({ from.id, to.id, label.id, edge.id });
            quad_model.to_label_from_edge->delete_record({ to.id, label.id, from.id, edge.id });
            quad_model.label_from_to_edge->delete_record({ label.id, from.id, to.id, edge.id });
            quad_model.label_to_from_edge->delete_record({ label.id, to.id, from.id, edge.id });

            // delete equal cases
            if (from == to) {
                quad_model.equal_from_to->delete_record({ from.id, label.id, edge.id });
                quad_model.equal_from_to_inv->delete_record({ label.id, from.id, edge.id });
                edge_label2equal_from_to_diff[label]--;
            }
        }

        // save here to delete later, because delete while iterating is a bad idea
        std::set<std::pair<ObjectId, ObjectId>> props_to_delete;

        auto prop_iter = quad_model.edge_key_value->get_range(
            &interruption,
            { edge.id, 0, 0 },
            { edge.id, UINT64_MAX, UINT64_MAX }
        );

        for (auto record = prop_iter.next(); record != nullptr; record = prop_iter.next()) {
            ObjectId key((*record)[1]);
            ObjectId value((*record)[2]);

            props_to_delete.emplace(key, value);
        }

        for (auto&& [k, v] : props_to_delete) {
            quad_model.edge_key_value->delete_record({ edge.id, k.id, v.id });
            quad_model.key_value_edge->delete_record({ k.id, v.id, edge.id });

            process_deleted_property(edge, k, v);
            deleted_edge_properties++;
            edge_key2total_diff[k]--;
        }
    }

    void delete_node(ObjectId node, bool detach)
    {
        bool interruption = false;

        const Record<4> min_range = { node.id, 0, 0, 0 };
        const Record<4> max_range = { node.id, UINT64_MAX, UINT64_MAX, UINT64_MAX };
        if (!detach) {
            auto it1 = quad_model.from_to_label_edge->get_range(&interruption, min_range, max_range);
            if (it1.next() != nullptr) {
                throw QueryException(
                    "Trying to delete node with existing connections (use DETACH DELETE if intended)"
                );
            }
        }

        if (quad_model.nodes->delete_record({ node.id })) {
            deleted_nodes++;
        }

        // save here to delete later, because delete while iterating is a bad idea
        std::set<ObjectId> edges_to_delete;

        auto it1 = quad_model.from_to_label_edge->get_range(&interruption, min_range, max_range);
        for (auto record = it1.next(); record != nullptr; record = it1.next()) {
            edges_to_delete.insert(ObjectId((*record)[3]));
        }
        auto it2 = quad_model.to_label_from_edge->get_range(&interruption, min_range, max_range);
        for (auto record = it2.next(); record != nullptr; record = it2.next()) {
            edges_to_delete.insert(ObjectId((*record)[3]));
        }

        // save here to delete later, because delete while iterating is a bad idea
        std::set<std::pair<ObjectId, ObjectId>> props_to_delete;

        auto prop_iter = quad_model.node_key_value->get_range(
            &interruption,
            { node.id, 0, 0 },
            { node.id, UINT64_MAX, UINT64_MAX }
        );

        for (auto record = prop_iter.next(); record != nullptr; record = prop_iter.next()) {
            auto key = (*record)[1];
            auto value = (*record)[2];

            props_to_delete.insert({ ObjectId(key), ObjectId(value) });
        }

        for (auto edge : edges_to_delete) {
            delete_edge(edge);
        }

        for (auto&& [k, v] : props_to_delete) {
            quad_model.node_key_value->delete_record({ node.id, k.id, v.id });
            quad_model.key_value_node->delete_record({ k.id, v.id, node.id });

            process_deleted_property(node, k, v);
            deleted_node_properties++;
            node_key2total_diff[k]--;
        }
    }

    void delete_node_label(ObjectId node, ObjectId label)
    {
        if (quad_model.node_label->delete_record({ node.id, label.id })) {
            quad_model.label_node->delete_record({ label.id, node.id });

            node_label2total_diff[label]--;
            deleted_node_labels++;
        }
    }

    void delete_node_property(ObjectId node, ObjectId key)
    {
        bool interruption = false;
        Record<3> min_range = { node.id, key.id, 0 };
        Record<3> max_range = { node.id, key.id, UINT64_MAX };
        auto prop_iter = quad_model.node_key_value->get_range(&interruption, min_range, max_range);

        if (auto existing_record = prop_iter.next()) {
            ObjectId value((*existing_record)[2]);

            quad_model.node_key_value->delete_record({ node.id, key.id, value.id });
            quad_model.key_value_node->delete_record({ key.id, value.id, node.id });

            process_deleted_property(node, key, value);
            deleted_node_properties++;
            node_key2total_diff[key]--;
        }
    }

    void delete_edge_property(ObjectId edge, ObjectId key)
    {
        bool interruption = false;
        Record<3> min_range = { edge.id, key.id, 0 };
        Record<3> max_range = { edge.id, key.id, UINT64_MAX };
        auto prop_iter = quad_model.edge_key_value->get_range(&interruption, min_range, max_range);

        if (auto existing_record = prop_iter.next()) {
            ObjectId value((*existing_record)[2]);

            quad_model.edge_key_value->delete_record({ edge.id, key.id, value.id });
            quad_model.key_value_edge->delete_record({ key.id, value.id, edge.id });

            process_deleted_property(edge, key, value);
            deleted_edge_properties++;
            node_key2total_diff[key]--;
        }
    }
};
} // namespace MQL
