#include "only_endpoint.h"

using namespace Paths::Any;

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::_begin(Binding& _parent_binding)
{
    parent_binding = &_parent_binding;

    lhs->begin(_parent_binding);

    lhs_at_end = false;
    fill_next_lhs_batch();
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::_reset()
{
    // Empty open and visited
    std::queue<const EndpointSearchState*> empty;
    open.swap(empty);

    visited.clear();
    if (MULTIPLE_FINAL) {
        reached_final.clear();
    }

    lhs->reset();
    lhs_at_end = false;

    // TODO: make ready_solutions empty?
    fill_next_lhs_batch();
}

template<bool MULTIPLE_FINAL>
bool BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::fill_next_lhs_batch()
{
    if (lhs_at_end) {
        return false;
    }

    start_batch.clear();
    // TODO: take at max 64 items
    uint64_t i = 0;
    while (i < 64 && lhs->next()) {
        ObjectId start_node = (*parent_binding)[start];
        assert(i < 64);
        if (!start_node.is_null()) {
            auto state_inserted = visited.emplace(automaton.start_state, start_node, 1ULL >> i);
            start_batch.push_back(start_node);
            open.push(state_inserted.first.operator->());
            i++;
        }
    }
    // TODO: remember if lhs is at end?

    // Starting state is solution
    if (automaton.is_final_state[automaton.start_state]) {
        for (auto ii = i; ii < i; ii++) {
            ready_solutions.emplace();
        }
    }
    iter = std::make_unique<NullIndexIterator>();
    return i > 0;
}

template<bool MULTIPLE_FINAL>
bool BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::_next()
{
next_begin:
    if (!ready_solutions.size() > 0) {
        ready_solutions.pop();
        if constexpr (MULTIPLE_FINAL) {
            reached_final.insert(...);
        }
        parent_binding->add(start, ...);
        parent_binding->add(end, ...);
        return true;
    }

    while (open.size() > 0) {
        auto current_state = open.front();

        // TODO: maybe return true false only
        auto reached_final_state = expand_neighbors(*current_state);

        // Enumerate reached solutions
        if (reached_final_state != nullptr) {
            goto next_begin;
        } else {
            // Pop and visit next state
            open.pop();
        }
    }
    if (fill_next_lhs_batch()) {
        goto next_begin;
    }

    return false;
}

template<bool MULTIPLE_FINAL>
const EndpointSearchState*
    BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::expand_neighbors(const EndpointSearchState& current_state)
{
    // Check if this is the first time that current_state is explored
    if (iter->at_end()) {
        current_transition = 0;
        // Check if automaton state has transitions
        if (automaton.from_to_connections[current_state.automaton_state].size() == 0) {
            return nullptr;
        }
        set_iter(current_state);
    }

    // Iterate over the remaining transitions of current_state
    // Don't start from the beginning, resume where it left thanks to current_transition and iter (pipeline)
    while (current_transition < automaton.from_to_connections[current_state.automaton_state].size()) {
        auto& transition = automaton.from_to_connections[current_state.automaton_state][current_transition];

        // Iterate over records until a final state is reached
        while (iter->next()) {
            EndpointSearchState next_state(transition.to, ObjectId(iter->get_reached_node()));
            auto visited_state = visited.insert(next_state);

            // If next state was visited for the first time
            if (visited_state.second) {
                auto reached_state = visited_state.first;
                open.push(reached_state.operator->());

                // Check if new path is solution
                if (automaton.is_final_state[reached_state->automaton_state]) {
                    if (MULTIPLE_FINAL) {
                        auto node_reached_final = reached_final.find(reached_state->node_id.id);
                        if (node_reached_final == reached_final.end()) {
                            reached_final.insert(reached_state->node_id.id);
                            return reached_state.operator->();
                        }
                    } else {
                        return reached_state.operator->();
                    }
                }
            }
        }

        // Construct new iter with the next transition (if there exists one)
        current_transition++;
        if (current_transition < automaton.from_to_connections[current_state.automaton_state].size()) {
            set_iter(current_state);
        }
    }
    return nullptr;
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::print(std::ostream& os, int indent, bool stats) const
{
    if (stats) {
        os << std::string(indent, ' ') << "[begin: " << stat_begin << " next: " << stat_next
           << " reset: " << stat_reset << " results: " << results << " idx_searches: " << idx_searches
           << "]\n";
    }
    os << std::string(indent, ' ') << "Paths::Any::BFSMultipleStartsOnlyEndpoint(start: " << start
       << ", end: " << end << ")";
}

template class Paths::Any::BFSMultipleStartsOnlyEndpoint<true>;
template class Paths::Any::BFSMultipleStartsOnlyEndpoint<false>;
