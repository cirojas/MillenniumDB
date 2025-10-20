#pragma once

#include <memory>
#include <queue>

#include <boost/unordered/unordered_node_set.hpp>
#include <boost/unordered/unordered_flat_set.hpp>

#include "query/executor/binding_iter.h"
#include "query/executor/binding_iter/paths/index_provider/path_index.h"
#include "query/parser/paths/automaton/rpq_automaton.h"

#include "endpoint_search_state.h"

namespace Paths { namespace Any {

template<bool MULTIPLE_FINAL>
class BFSMultipleStartsOnlyEndpoint : public BindingIter {
private:
    // Attributes determined in the constructor
    std::unique_ptr<BindingIter> lhs;
    VarId start;
    VarId end;
    const RPQ_DFA automaton;
    std::unique_ptr<IndexProvider> provider;

    // where the results will be written, determined in begin()
    Binding* parent_binding;

     // Set of visited SearchStates
    boost::unordered_node_set<EndpointSearchState, std::hash<EndpointSearchState>> visited;

    // Queue for BFS. Pointers point to the states in visited
    std::queue<const EndpointSearchState*> open;

    // Iterator for current node expansion
    std::unique_ptr<EdgeIter> iter;

    // The index of the transition being currently explored
    uint_fast32_t current_transition;

    typename std::conditional<
        MULTIPLE_FINAL,
        boost::unordered_flat_set<uint64_t>,
        DummySet>::type reached_final;

    bool lhs_at_end;

    std::vector<ObjectId> start_batch;

    bool fill_next_lhs_batch();

public:
    // Statistics
    uint_fast32_t idx_searches = 0;

    BFSMultipleStartsOnlyEndpoint(
        std::unique_ptr<BindingIter> lhs,
        VarId /*path_var*/,
        VarId start,
        VarId end,
        RPQ_DFA automaton,
        std::unique_ptr<IndexProvider> provider
    ) :
        lhs(std::move(lhs)),
        start(start),
        end(end),
        automaton(automaton),
        provider(std::move(provider))
    { }

    void _begin(Binding& parent_binding) override;
    void _reset() override;
    bool _next() override;
    void print(std::ostream& os, int indent, bool stats) const override;

    // Expand neighbors from current state
    const EndpointSearchState* expand_neighbors(const EndpointSearchState& current_state);

    void assign_nulls() override
    {
        parent_binding->add(end, ObjectId::get_null());
    }

    // Set iterator for current node + transition
    inline void set_iter(const EndpointSearchState& s)
    {
        // Get current transition object from automaton
        auto& transition = automaton.from_to_connections[s.automaton_state][current_transition];

        // Get iterator from custom index
        iter = provider->get_iter(transition.type_id.id, transition.inverse, s.node_id.id);
        idx_searches++;
    }
};

}} // namespace Paths::Any
