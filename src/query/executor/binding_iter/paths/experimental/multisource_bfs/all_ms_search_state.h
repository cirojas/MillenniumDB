#pragma once

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
    const MultiSourceSearchState* previous;
    const ObjectId type_id;
    const bool inverse_direction;
};

struct Struct { // TODO: rename
    uint64_t distance;
    std::vector<Transition> previous;

    Struct() = default;

    Struct(uint64_t distance) :
        distance(distance)
    { }
};

struct MultiSourceSearchState {
    // The ID of the node the algorithm has reached
    const ObjectId node_id;

    // State of the automaton defining the path query
    const uint32_t automaton_state;

    mutable bool in_queue;

    // Map starting nodes to a vector of previous states
    mutable std::map<uint32_t, Struct> previous;

    MultiSourceSearchState(uint32_t automaton_state, ObjectId node_id) :
        node_id(node_id),
        automaton_state(automaton_state)
    { }

    MultiSourceSearchState(const MultiSourceSearchState& other) = delete;

    bool reached_by(uint32_t start_node) const
    {
        return previous.count(start_node);
    }

    bool get_distance(uint32_t start_node) const
    {
        return previous[start_node].distance;
    }

    void add_new_previous(uint32_t start_node, Transition transition, const uint64_t distance) const
    {
        Struct st(distance);
        st.previous.push_back(transition);
        previous[start_node].previous.push_back(transition);
    }

    void add_previous(uint32_t start_node, Transition transition) const
    {
        previous[start_node].previous.push_back(transition);
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
    MultiSourceSearchStateSolution(uint64_t start_idx, const MultiSourceSearchState* state);

    // check if the indices are valid
    bool at_end = true;

    const MultiSourceSearchState* state;

    uint64_t start_idx;

    // TODO: falta idx de ultima transicion?

    std::vector<uint64_t> iter_state;

    bool has_next();

    void start_enumeration() {
        at_end = false;
    }

    void print(
        std::ostream& os,
        std::function<void(std::ostream& os, ObjectId)> print_node,
        std::function<void(std::ostream& os, ObjectId, bool)> print_edge,
        bool begin_at_left
    ) const;



    // TODO: do i need this? consider using the next function to extract the nodes and edges
    // immediately
    // std::vector<ObjectId> oids;
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
