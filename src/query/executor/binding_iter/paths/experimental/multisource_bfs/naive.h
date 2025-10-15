#pragma once

#include <memory>
#include <queue>
#include <type_traits>

#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/unordered/unordered_node_set.hpp>

#include "query/executor/binding_iter.h"
// #include "query/executor/binding_iter/paths/any_walks/bfs_multiple_starts.h"
#include "query/executor/binding_iter/paths/any_walks/search_state.h"
#include "query/executor/binding_iter/paths/index_provider/path_index.h"
#include "query/parser/paths/automaton/rpq_automaton.h"

// #include "debug_mati.h"

namespace Paths { namespace Any {

/*
BFSMultipleStartsNaive
*/
template<bool MULTIPLE_FINAL>
class BFSMultipleStartsNaive : public BindingIter {
private:
    bool single_next();
    void single_reset();
    void set_new_start_node();
    // Attributes determined in the constructor
    std::unique_ptr<BindingIter> lhs;
    VarId path_var;
    VarId start;
    VarId end;
    const RPQ_DFA automaton;
    std::unique_ptr<IndexProvider> provider;
    //   Counter visited_nodes_counter;
    //   StructureMemoryLogger memory_size_logger;

    std::vector<ObjectId> start_nodes;

    bool _should_log_memory = false;

    const SearchState* the_only_search_state;

    std::queue<ObjectId> start_nodes_q;
    ObjectId current_start_node;
    // where the results will be written, determined in begin()
    Binding* parent_binding;

    // Set of visited SearchStates
    boost::unordered_node_set<SearchState, std::hash<SearchState>> visited;

    // Queue for BFS. Pointers point to the states in visited
    std::queue<const SearchState*> open;

    // Iterator for current node expansion
    std::unique_ptr<EdgeIter> iter;

    // The index of the transition being currently explored
    uint_fast32_t current_transition;

    // true in the first call of next() and after a reset()
    bool first_next = true;

    // Template type for storing nodes reached with a final state
    typename std::conditional<MULTIPLE_FINAL, boost::unordered_flat_set<uint64_t>, DummySet>::type
        reached_final;

    // void _log_structures_sizes();

public:
    // Statistics
    uint_fast32_t idx_searches = 0;

    BFSMultipleStartsNaive(
        std::unique_ptr<BindingIter> lhs,
        VarId path_var,
        VarId start,
        VarId end,
        RPQ_DFA automaton,
        std::unique_ptr<IndexProvider> provider
    ) :
        lhs(std::move(lhs)),
        path_var(path_var),
        start(start),
        end(end),
        automaton(automaton),
        provider(std::move(provider))
    {
        // _should_log_memory = get_calculate_memory_flag_value();
        // _log_mati() << "should_log_memory: " << _should_log_memory << std::endl;
    }

    void _begin(Binding& parent_binding) override;
    void _reset() override;
    bool _next() override;
    void print(std::ostream& os, int indent, bool stats) const override;

    // Expand neighbors from current state
    const SearchState* expand_neighbors(const SearchState& current_state);

    void assign_nulls() override
    {
        parent_binding->add(end, ObjectId::get_null());
        parent_binding->add(path_var, ObjectId::get_null());
    }

    // Set iterator for current node + transition
    inline void set_iter(const SearchState& s)
    {
        // Get current transition object from automaton
        auto& transition = automaton.from_to_connections[s.automaton_state][current_transition];

        // Get iterator from custom index
        iter = provider->get_iter(transition.type_id.id, transition.inverse, s.node_id.id);
        idx_searches++;
    }
};
}} // namespace Paths::Any