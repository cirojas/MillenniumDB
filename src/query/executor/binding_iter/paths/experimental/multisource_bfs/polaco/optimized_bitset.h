
#pragma once

#include <memory>
#include <queue>
#include <type_traits>

// #include "boost/bitset.hpp"
#include "query/executor/binding_iter.h"
#include "query/executor/binding_iter/paths/any_walks/search_state.h"
#include "query/executor/binding_iter/paths/index_provider/path_index.h"
#include "query/parser/paths/automaton/rpq_automaton.h"
// #include "search_state.h"
// #include "search_state_multiple_starts.h"

#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <boost/unordered/unordered_node_set.hpp>

// #include "bfs_multiple_starts_common.h"
// #include "debug_mati.h"

namespace Paths { namespace Any {

struct searchnodeid_hash2 {
    std::size_t operator()(const std::pair<uint32_t, ObjectId>& p) const
    {
        return p.second.id ^ p.first;
    }
};

/*
BFSMultipleStartsOptimizedBitset returns a single path to all
reachable nodes from a starting node, using BFS.
*/
template<bool MULTIPLE_FINAL>
class BFSMultipleStartsOptimizedBitset :
    public BindingIter

{
    using SearchNodeId = std::pair<uint32_t, ObjectId>;

private:
    static constexpr int NUM_CONCURRENT_BFS = 256;
    using bfs_id_bit_set = std::bitset<NUM_CONCURRENT_BFS>;
    // Attributes determined in the constructor
    std::unique_ptr<BindingIter> lhs;
    VarId path_var;
    VarId start;
    VarId end;
    const RPQ_DFA automaton;
    std::unique_ptr<IndexProvider> provider;
    // Counter visited_nodes_counter;
    // StructureMemoryLogger memory_size_logger;
    // where the results will be written, determined in begin()
    Binding* parent_binding;


    // bool _should_log_memory = false;

    // Optimized structures
    SearchState* single_search_state;
    std::vector<ObjectId> bfss_ordered;
    int current_bfs_chunk = 0;
    int num_nodes_in_current_chunk = 0;
    int num_chunks = 0;
    bfs_id_bit_set bit_mask_for_current_chunk;
    // -------------

    // Maybe can tell to unroll loop:
    // https://stackoverflow.com/questions/16022362/how-to-tell-the-compiler-to-unroll-this-loop
    // TODO will have to do assignment using this
    // Try to optimize the for loop using openmp
    // https://chatgpt.com/share/67c6385f-179c-800b-9b8d-8d6ec423dcf2
    // TODO also think about memory overhead. how to avoid it?
    //
    // Maybe instead of array it can just be a map so that its smaller
    // Or maybe map:
    // unordered_node_map<SearchNodeId, SearchState>
    // search_states[NUM_CONCURRENT_BFSS]; boost::unordered_node_map<SearchNodeId,
    // std::array<SearchState, NUM_CONCURRENT_BFS>> seen_optimized;
    boost::unordered_node_map<SearchNodeId, SearchState, searchnodeid_hash2> search_states[NUM_CONCURRENT_BFS];
    // boost::unordered_node_map<
    //     ObjectId,
    //     boost::unordered_node_map<SearchNodeId, SearchState,
    //                               searchnodeid_hash>,
    //     objectid_hash>
    //     seen;

    boost::unordered_node_map<SearchNodeId, bfs_id_bit_set, searchnodeid_hash2> bfss_that_reached_given_node;

    // static void debug_print_bfs_id_bit_set(bfs_id_bit_set bfs_id_bit_set)
    // {
    //     _debug_mati_simple() << bfs_id_bit_set;
    // }

    // void _log_structures_sizes();
    // visit and visit_next. Maybe we can use set instead of map and the values
    // (bfs ids) can be taken from `bfss_that_reached_given_node`. hmm. are the
    // searchNodeIds useful at all?
    boost::unordered_node_map<SearchNodeId, bfs_id_bit_set, searchnodeid_hash2> bfses_to_be_visited;
    boost::unordered_node_map<SearchNodeId, bfs_id_bit_set, searchnodeid_hash2> bfses_to_be_visited_next;

    std::queue<SearchNodeId> visit_q;
    std::queue<std::pair<SearchNodeId, int>> first_visit_q;

    // Iterator for current node expansion
    std::unique_ptr<EdgeIter> iter;

    // The index of the transition being currently explored
    uint_fast32_t current_transition;

    std::queue<SearchState*> search_states_for_current_iteration;

    typename std::conditional<MULTIPLE_FINAL, boost::unordered_flat_set<uint64_t>, DummySet>::type
        reached_final[NUM_CONCURRENT_BFS];

public:
    // Statistics
    uint_fast32_t idx_searches = 0;

    BFSMultipleStartsOptimizedBitset(
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
        // _debug_mati() << "hello!" << std::endl;
    }

    void _begin(Binding& parent_binding) override;
    void _reset() override;
    bool _next() override;
    void print(std::ostream& os, int indent, bool stats) const override;

    void set_new_bfs_chunk_and_reset();
    void single_reset();
    bool single_next();
    void prepare_structures_for_next_chunk_processing();

    // Expand neighbors from current state
    const SearchState* expand_neighbors(const SearchNodeId& current_state);

    void assign_nulls() override
    {
        parent_binding->add(end, ObjectId::get_null());
        parent_binding->add(path_var, ObjectId::get_null());
    }

    // Set iterator for current node + transition
    inline void set_iter(const SearchNodeId& s)
    {
        // Get current transition object from automaton
        auto& transition = automaton.from_to_connections[s.first][current_transition];

        // Get iterator from custom index
        iter = provider->get_iter(transition.type_id.id, transition.inverse, s.second.id);
        idx_searches++;
    }
};

}} // namespace Paths::Any
