#include "ms_bfs_enum.h"

#include "system/path_manager.h"

using namespace Paths::Any;

template<bool MULTIPLE_FINAL>
void BFSMultiSource<MULTIPLE_FINAL>::_begin(Binding& _parent_binding)
{
    parent_binding = &_parent_binding;

    lhs->begin(_parent_binding);

    lhs_at_end = false;
    fill_next_lhs_batch();
}

template<bool MULTIPLE_FINAL>
void BFSMultiSource<MULTIPLE_FINAL>::_reset()
{
    // Empty open and visited
    std::queue<const MSSearchState*> empty;
    open.swap(empty);

    lhs->reset();
    lhs_at_end = false;
    ready_solutions.clear();

    fill_next_lhs_batch();
}

template<bool MULTIPLE_FINAL>
void BFSMultiSource<MULTIPLE_FINAL>::fill_next_lhs_batch()
{
    start_batch.clear();
    reached_final.clear();
    visited.clear();
    if (lhs_at_end) {
        return;
    }

    uint64_t i = 0;
    while (i < 64 && lhs->next()) {
        ObjectId start_node = (*parent_binding)[start];
        if (!start_node.is_null()) {
            auto state_inserted = visited.emplace(automaton.start_state, start_node).first.operator->();
            start_batch.push_back(start_node);
            open.push(state_inserted);
            state_inserted->set_previous(i, nullptr, ObjectId::get_null(), false);

            // Starting state is solution
            if (automaton.is_final_state[automaton.start_state]) {
                ready_solutions.emplace_back(start_batch[i], i, state_inserted);
            }
            i++;
        }
    }
    lhs_at_end = i == 0;

    iter = std::make_unique<NullIndexIterator>();
}

template<bool MULTIPLE_FINAL>
bool BFSMultiSource<MULTIPLE_FINAL>::_next()
{
next_begin:
    while (ready_solutions.size() > 0) {
        current_solution = ready_solutions.back();
        ready_solutions.pop_back();
        if constexpr (MULTIPLE_FINAL) {
            EndpointSolution s(current_solution.start_index, current_solution.state->node_id);
            if (!reached_final.insert(s).second) {
                continue;
            }
        }
        current_solution.start_node = start_batch[current_solution.start_index];

        auto path_id = path_manager.set_path(&current_solution, path_var);
        parent_binding->add(path_var, path_id);
        parent_binding->add(start, current_solution.start_node);
        parent_binding->add(end, current_solution.state->node_id);
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
bool BFSMultiSource<MULTIPLE_FINAL>::expand_neighbors(const MSSearchState& current_state)
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
            ObjectId reached_node(iter->get_reached_node());

            auto visited_state = visited.emplace(transition.to, reached_node);
            auto reached_state = visited_state.first.operator->();

            if (visited_state.second) {
                // reached_state is visited for the first time
                open.push(reached_state);

                // iterate over the starting nodes that reached the previous state
                for (auto&& [start_idx, _] : current_state.previous) {
                    reached_state
                        ->set_previous(start_idx, &current_state, transition.type_id, transition.inverse);
                }

                if (automaton.is_final_state[reached_state->automaton_state]) {
                    for (auto&& [start_idx, _] : current_state.previous) {
                        ready_solutions.emplace_back(start_batch[start_idx], start_idx, reached_state);
                    }
                    return true;
                }
            } else {
                // reached_state was present in visited before
                std::set<int> new_starts;

                for (auto&& [start_node_idx, _] : current_state.previous) {
                    if (reached_state->previous.count(start_node_idx)) {
                        continue;
                    }
                    reached_state->set_previous(
                        start_node_idx,
                        &current_state,
                        transition.type_id,
                        transition.inverse
                    );
                    new_starts.insert(start_node_idx);
                }

                // add to open if there are new paths that reach this state
                if (!new_starts.empty() && !reached_state->in_queue) {
                    open.push(reached_state);
                    reached_state->in_queue = true;
                }

                if (automaton.is_final_state[reached_state->automaton_state]) {
                    for (auto start_idx : new_starts) {
                        ready_solutions.emplace_back(start_batch[start_idx], start_idx, reached_state);
                    }
                    return true;
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
void BFSMultiSource<MULTIPLE_FINAL>::print(std::ostream& os, int indent, bool stats) const
{
    if (stats) {
        os << std::string(indent, ' ') << "[begin: " << stat_begin << " next: " << stat_next
           << " reset: " << stat_reset << " results: " << results << " idx_searches: " << idx_searches
           << "]\n";
    }
    os << std::string(indent, ' ') << "Paths::Any::BFSMultiSource(start: " << start << ", end: " << end
       << ")\n";
    lhs->print(os, indent + 2, stats);
}

template class Paths::Any::BFSMultiSource<true>;
template class Paths::Any::BFSMultiSource<false>;
