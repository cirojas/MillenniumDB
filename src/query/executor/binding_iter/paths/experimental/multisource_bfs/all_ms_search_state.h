#pragma once

#include <cassert>
#include <functional>
#include <map>

#include "graph_models/object_id.h"

namespace Paths { namespace AllShortest {

struct MultiSourceSearchState;

class DummySet {
public:
    static inline void clear() { }
    static inline std::pair<uint64_t, uint64_t> end()
    {
        return { 0, 0 };
    }
    static inline std::pair<uint64_t, uint64_t> find(std::pair<uint64_t, uint64_t>)
    {
        return { 0, 0 };
    }
    static inline void insert(std::pair<uint64_t, uint64_t>) { }
};

struct Transition {
    const MultiSourceSearchState* state;
    const ObjectId type_id;
    const bool inverse_direction;

    Transition(const MultiSourceSearchState* state, ObjectId type_id, bool inverse_direction) :
        state(state),
        type_id(type_id),
        inverse_direction(inverse_direction)
    { }
};

struct PreviousInfo {
    uint64_t distance;
    std::vector<Transition> previous;

    PreviousInfo() = delete;

    PreviousInfo(uint64_t distance) :
        distance(distance)
    {
        assert(distance == 0);
        if (distance == 0) {
            previous.emplace_back(nullptr, ObjectId::get_null(), false);
        }
    }

    PreviousInfo(uint64_t distance, const Transition& transition) :
        distance(distance)
    {
        assert(distance != 0);
        previous.push_back(transition);
    }
};

struct MultiSourceSearchState {
    // The ID of the node the algorithm has reached
    const ObjectId node_id;

    // State of the automaton defining the path query
    const uint32_t automaton_state;

    mutable bool in_queue = true;

    // Map starting nodes to a vector of previous states
    mutable std::map<uint32_t, PreviousInfo> start2previous;

    MultiSourceSearchState(uint32_t automaton_state, ObjectId node_id) :
        node_id(node_id),
        automaton_state(automaton_state)
    { }

    MultiSourceSearchState(const MultiSourceSearchState& other) = delete;

    bool reached_by(uint32_t start_idx) const
    {
        auto it = start2previous.find(start_idx);
        return it != start2previous.end();
    }

    uint64_t get_distance(uint32_t start_idx) const
    {
        auto it = start2previous.find(start_idx);
        assert(it != start2previous.end());
        return it->second.distance;
    }

    void init_previous(uint32_t start_idx) const
    {
        start2previous.insert({ start_idx, PreviousInfo(0) });
    }

    void add_previous(uint32_t start_idx, Transition transition) const
    {
        auto it = start2previous.find(start_idx);
        assert(it != start2previous.end());
        it->second.previous.push_back(transition);
    }

    void add_new_previous(uint32_t start_idx, Transition transition, const uint64_t distance) const
    {
        start2previous.insert({ start_idx, PreviousInfo(distance, transition) });
    }

    // For ordered set
    bool operator<(const MultiSourceSearchState& other) const
    {
        if (automaton_state < other.automaton_state) {
            return true;
        } else if (other.automaton_state < automaton_state) {
            return false;
        } else {
            return node_id < other.node_id;
        }
    }

    // Overloading the ostream operator<<
    friend std::ostream& operator<<(std::ostream& os, const MultiSourceSearchState& state)
    {
        os << "MultiSourceSearchState:" << " automaton_state(" << state.automaton_state << "), node_id("
           << state.node_id << ")";
        return os;
    }
    // For unordered set
    bool operator==(const MultiSourceSearchState& other) const
    {
        return automaton_state == other.automaton_state && node_id == other.node_id;
    }
};

class MultiSourceSearchStateSolution {
public:
    MultiSourceSearchStateSolution() = default;

    MultiSourceSearchStateSolution(uint64_t start_idx, const MultiSourceSearchState* state) :
        start_idx(start_idx),
        state(state)
    { }

    // check if the indices are valid
    bool at_end = true;

    uint64_t start_idx;

    const MultiSourceSearchState* state;

    std::vector<std::vector<Transition>::iterator> iter_state_cur;
    std::vector<std::vector<Transition>::iterator> iter_state_end;

    bool has_next();

    void start_enumeration();

    void print(
        std::ostream& os,
        std::function<void(std::ostream& os, ObjectId)> print_node,
        std::function<void(std::ostream& os, ObjectId, bool)> print_edge,
        bool begin_at_left
    ) const;

    std::vector<ObjectId> current_path_nodes;
    std::vector<ObjectId> current_path_edges;
    std::vector<bool> inverse_directions;
};

}} // namespace Paths::AllShortest

// For unordered set
template<>
struct std::hash<Paths::AllShortest::MultiSourceSearchState> {
    std::size_t operator()(const Paths::AllShortest::MultiSourceSearchState& lhs) const
    {
        return lhs.automaton_state ^ lhs.node_id.id;
    }
};
