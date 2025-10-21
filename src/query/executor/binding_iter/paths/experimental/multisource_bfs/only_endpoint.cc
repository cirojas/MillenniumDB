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

    lhs->reset();
    lhs_at_end = false;
    ready_solutions.clear();

    fill_next_lhs_batch();
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::fill_next_lhs_batch()
{
    start_batch.clear();
    reached_final.clear();
    if (lhs_at_end) {
        return;
    }

    uint64_t i = 0;
    while (i < 64 && lhs->next()) { // TODO:
        ObjectId start_node = (*parent_binding)[start];
        assert(i < 64);
        if (!start_node.is_null()) {
            auto state_inserted = visited.emplace(automaton.start_state, start_node, 1ULL << i);
            start_batch.push_back(start_node);
            open.push(state_inserted.first.operator->());
            i++;
        }
    }
    lhs_at_end = i == 0;

    // Starting state is solution
    if (automaton.is_final_state[automaton.start_state]) {
        for (auto ii = i; ii < i; ii++) {
            ready_solutions.emplace_back(ii, start_batch[ii]);
        }
    }
    iter = std::make_unique<NullIndexIterator>();
}

template<bool MULTIPLE_FINAL>
bool BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::_next()
{
next_begin:
    while (ready_solutions.size() > 0) {
        auto solution = ready_solutions.back();
        ready_solutions.pop_back();
        if constexpr (MULTIPLE_FINAL) {
            if (!reached_final.insert(solution).second) {
                continue;
            }
        }
        parent_binding->add(start, start_batch[solution.start_index]);
        parent_binding->add(end, solution.end);
        return true;
    }

    while (open.size() > 0) {
        auto current_state = open.front();

        if (expand_neighbors(*current_state)) {
            // Enumerate reached solutions
            goto next_begin;
        } else {
            // Pop and visit next state
            current_state->in_queue = false;
            open.pop();
        }
    }
    fill_next_lhs_batch();
    if (!start_batch.empty()) {
        goto next_begin;
    }

    return false;
}

template<bool MULTIPLE_FINAL>
bool BFSMultipleStartsOnlyEndpoint<MULTIPLE_FINAL>::expand_neighbors(const EndpointSearchState& current_state)
{
    // Check if this is the first time that current_state is explored
    if (iter->at_end()) {
        current_transition = 0;
        // Check if automaton state has transitions
        if (automaton.from_to_connections[current_state.automaton_state].size() == 0) {
            return false;
        }
        set_iter(current_state);
    }

    // Iterate over the remaining transitions of current_state
    // Don't start from the beginning, resume where it left thanks to current_transition and iter (pipeline)
    while (current_transition < automaton.from_to_connections[current_state.automaton_state].size()) {
        auto& transition = automaton.from_to_connections[current_state.automaton_state][current_transition];

        // Iterate over records until a final state is reached
        while (iter->next()) {
            EndpointSearchState next_state(
                transition.to,
                ObjectId(iter->get_reached_node()),
                current_state.bitmap
            );
            auto visited_state = visited.insert(next_state);

            // If next state was visited for the first time
            auto reached_state = visited_state.first;
            if (visited_state.second) {
                open.push(reached_state.operator->());

                // Check if new path is solution
                if (automaton.is_final_state[reached_state->automaton_state]) {
                    for (uint64_t i = 0; i < start_batch.size(); i++) {
                        if ((1ULL << i & reached_state->bitmap) != 0) {
                            ready_solutions.emplace_back(i, reached_state->node_id);
                        }
                    }
                    return true;
                }
            } else {
                auto old_bitmap = reached_state->bitmap;
                auto new_bitmap = current_state.bitmap | reached_state->bitmap;
                if (reached_state->bitmap != new_bitmap) {
                    reached_state->bitmap = new_bitmap;
                    if (!reached_state->in_queue) {
                        reached_state->in_queue = true;
                        open.push(reached_state.operator->());
                    }
                    // Check if new path is solution
                    if (automaton.is_final_state[reached_state->automaton_state]) {
                        for (uint64_t i = 0; i < start_batch.size(); i++) {
                            auto bit_shifted = 1ULL << i;
                            if ((bit_shifted & new_bitmap) != 0 && (bit_shifted & old_bitmap) == 0) {
                                ready_solutions.emplace_back(i, reached_state->node_id);
                            }
                        }
                        return true;
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
    return false;
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
