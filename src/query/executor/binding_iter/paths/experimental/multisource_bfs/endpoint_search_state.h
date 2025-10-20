#pragma once

#include <functional>

#include "graph_models/object_id.h"

namespace Paths { namespace Any {

// Dummy structure for template usage
class DummySet {
public:
    static inline void clear() { }
    static inline int end()
    {
        return 0;
    }
    static inline int find(uint64_t)
    {
        return 0;
    }
    static inline void insert(uint64_t) { }
};


struct EndpointSearchState {
    // The ID of the node the algorithm has reached
    const ObjectId node_id;

    // State of the automaton defining the path query
    const uint32_t automaton_state;

    mutable uint64_t bitmap;

    mutable bool in_queue;

    EndpointSearchState(
        uint32_t automaton_state,
        ObjectId node_id,
        uint64_t bitmap
    ) :
        node_id(node_id),
        automaton_state(automaton_state),
        bitmap(bitmap),
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
        return automaton_state == other.automaton_state && node_id == other.node_id;
    }

    // SearchState clone() const
    // {
    //     return SearchState(automaton_state, node_id, previous, inverse_direction, type_id);
    // }

    // void print(
    //     std::ostream& os,
    //     std::function<void(std::ostream& os, ObjectId)> print_node,
    //     std::function<void(std::ostream& os, ObjectId, bool)> print_edge,
    //     bool begin_at_left
    // ) const;
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

