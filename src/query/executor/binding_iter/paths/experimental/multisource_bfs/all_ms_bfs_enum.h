#pragma once

#include <memory>
#include <queue>

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <boost/unordered/unordered_node_set.hpp>

#include "query/executor/binding_iter.h"
#include "query/executor/binding_iter/paths/experimental/multisource_bfs/all_ms_search_state.h"
#include "query/executor/binding_iter/paths/experimental/multisource_bfs/endpoint_solution.h"
#include "query/executor/binding_iter/paths/index_provider/path_index.h"
#include "query/parser/paths/automaton/rpq_automaton.h"

namespace Paths { namespace AllShortest {

// Dummy structure for template usage
class DummyMSMap {
public:
    static inline void clear() { }
    static inline std::pair<uint64_t, size_t>* end()
    {
        return nullptr;
    }
    static inline std::pair<uint64_t, size_t>* find(uint64_t)
    {
        return nullptr;
    }
    static inline void insert(std::pair<uint64_t, size_t>) { }
};

template<bool MULTIPLE_FINAL>
class BFSMultiSource : public BindingIter {
private:
    // Attributes determined in the constructor
    std::unique_ptr<BindingIter> lhs;
    VarId start;
    VarId end;
    VarId path_var;
    const RPQ_DFA automaton;
    std::unique_ptr<IndexProvider> provider;

    // where the results will be written, determined in begin()
    Binding* parent_binding;

    // Queue for BFS. Pointers point to the states in visited
    std::queue<std::pair<const MSSearchState*, uint64_t>> open;

    // Iterator for current node expansion
    std::unique_ptr<EdgeIter> iter;

    // The index of the transition being currently explored
    uint_fast32_t current_transition;

    typename std::conditional<
        MULTIPLE_FINAL,
        boost::unordered_flat_map<EndpointSolution, size_t, EndpointSolution::Hasher>,
        DummyMSMap>::type optimal_distances;

    bool lhs_at_end;

    std::vector<ObjectId> start_batch;

    std::vector<MSSearchStateSolution> ready_solutions;

    MSSearchStateSolution current_solution;

    boost::unordered_node_set<MSSearchState, std::hash<MSSearchState>> visited;

    void fill_next_lhs_batch();

public:
    // Statistics
    uint_fast32_t idx_searches = 0;

    BFSMultiSource(
        std::unique_ptr<BindingIter> lhs,
        VarId path_var,
        VarId start,
        VarId end,
        RPQ_DFA automaton,
        std::unique_ptr<IndexProvider> provider
    ) :
        lhs(std::move(lhs)),
        start(start),
        end(end),
        path_var(path_var),
        automaton(automaton),
        provider(std::move(provider))
    { }

    void _begin(Binding& parent_binding) override;
    void _reset() override;
    bool _next() override;
    void print(std::ostream& os, int indent, bool stats) const override;

    // Expand neighbors from current state
    bool expand_neighbors(const MSSearchState& current_state, uint64_t distance);

    void assign_nulls() override
    {
        parent_binding->add(end, ObjectId::get_null());
    }

    // Set iterator for current node + transition
    inline void set_iter(const MSSearchState& s)
    {
        // Get current transition object from automaton
        auto& transition = automaton.from_to_connections[s.automaton_state][current_transition];

        // Get iterator from custom index
        iter = provider->get_iter(transition.type_id.id, transition.inverse, s.node_id.id);
        idx_searches++;
    }
};

}} // namespace Paths::AllShortest
