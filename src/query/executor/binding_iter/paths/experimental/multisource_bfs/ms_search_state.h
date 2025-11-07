#pragma once

#include <functional>
#include <map>

#include "graph_models/object_id.h"

namespace Paths { namespace Any {

struct MSSearchState;

struct Transition {
    const MSSearchState* state;
    ObjectId type_id;
    bool inverse_direction;
};

struct MSSearchState {
    // The ID of the node the algorithm has reached
    const ObjectId node_id;

    // State of the automaton defining the path query
    const uint32_t automaton_state;

    mutable bool in_queue = true;

    //  start_index -> Transition
    mutable std::map<uint32_t, Transition> previous;

    MSSearchState(uint32_t automaton_state, ObjectId node_id) :
        node_id(node_id),
        automaton_state(automaton_state)
    { }

    MSSearchState(const MSSearchState& other) = delete;

    void set_previous(
        uint32_t start_idx,
        const MSSearchState* previous_state,
        ObjectId type_id,
        bool inverse_direction
    ) const
    {
        Transition transition { previous_state, type_id, inverse_direction };
        previous.emplace(start_idx, transition);
    }

    // For ordered set
    bool operator<(const MSSearchState& other) const
    {
        if (automaton_state < other.automaton_state) {
            return true;
        } else if (other.automaton_state < automaton_state) {
            return false;
        } else {
            return node_id < other.node_id;
        }
    }

    // For unordered set
    bool operator==(const MSSearchState& other) const
    {
        return (automaton_state == other.automaton_state) & (node_id == other.node_id);
    }
};

struct MSSearchStateSolution {
    // The start node of the reconstructed path
    ObjectId start_node;
    uint32_t start_index;

    const MSSearchState* state;

    MSSearchStateSolution() = default;

    MSSearchStateSolution(ObjectId start_node, uint32_t start_index, const MSSearchState* state) :
        start_node(start_node),
        start_index(start_index),
        state(state)
    { }

    void for_each(
        std::function<void(ObjectId)> node_func,
        std::function<void(ObjectId, bool)> edge_func,
        bool begin_at_left
    ) const;
};

}} // namespace Paths::Any

// For unordered set
template<>
struct std::hash<Paths::Any::MSSearchState> {
    std::size_t operator()(const Paths::Any::MSSearchState& s) const
    {
        return s.automaton_state ^ s.node_id.id;
    }
};
