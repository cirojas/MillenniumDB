#pragma once

#include <functional>

#include "graph_models/object_id.h"

namespace Paths { namespace Any {

struct EndpointSearchState {
    // The ID of the node the algorithm has reached
    const ObjectId node_id;

    mutable uint64_t bitmap;

    // State of the automaton defining the path query
    const uint32_t automaton_state;

    mutable bool in_queue;

    EndpointSearchState(uint32_t automaton_state, ObjectId node_id, uint64_t bitmap) :
        node_id(node_id),
        bitmap(bitmap),
        automaton_state(automaton_state),
        in_queue(true)
    { }

    // For ordered set
    bool operator<(const EndpointSearchState& other) const
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
    bool operator==(const EndpointSearchState& other) const
    {
        return (automaton_state == other.automaton_state) & (node_id == other.node_id);
    }
};

}} // namespace Paths::Any

// For unordered set
template<>
struct std::hash<Paths::Any::EndpointSearchState> {
    std::size_t operator()(const Paths::Any::EndpointSearchState& s) const
    {
        return s.automaton_state ^ s.node_id.id;
    }
};
