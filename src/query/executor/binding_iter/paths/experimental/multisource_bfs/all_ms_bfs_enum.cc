#include "all_ms_bfs_enum.h"

#include "system/path_manager.h"

using namespace Paths::AllShortest;

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
    while (!open.empty()) {
        open.pop();
    }

    lhs->reset();
    lhs_at_end = false;
    ready_solutions.clear();

    fill_next_lhs_batch();
}

template<bool MULTIPLE_FINAL>
void BFSMultiSource<MULTIPLE_FINAL>::fill_next_lhs_batch()
{
    start_batch.clear();
    optimal_distances.clear();
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
            open.push({ state_inserted, 0 });

            state_inserted->init_previous(i);

            if (automaton.is_final_state[automaton.start_state]) {
                ready_solutions.emplace_back(i, state_inserted);
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
    if (!current_solution.at_end) {
        if (current_solution.has_next()) {
            return true;
        }
    }
    while (ready_solutions.size() > 0) {
        current_solution = ready_solutions.back();
        ready_solutions.pop_back();

        // discard final states with non optimal distance
        if constexpr (MULTIPLE_FINAL) {
            EndpointSolution k(current_solution.start_idx, current_solution.state->node_id);
            auto current_solution_distance = current_solution.state->get_distance(current_solution.start_idx);
            auto previous_solution = optimal_distances.find(k);
            if (previous_solution != optimal_distances.end()) {
                if (previous_solution->second != current_solution_distance) {
                    continue;
                }
            } else {
                optimal_distances.insert({ k, current_solution_distance });
            }
        }

        current_solution.start_enumeration();

        auto path_id = path_manager.set_path(&current_solution, path_var);
        parent_binding->add(path_var, path_id);
        parent_binding->add(start, start_batch[current_solution.start_idx]);
        parent_binding->add(end, current_solution.state->node_id);

        return true;
    }

    while (open.size() > 0) {
        auto&& [current_state, distance] = open.front();

        if (expand_neighbors(*current_state, distance)) {
            // Enumerate reached solutions
            goto next_begin;
        } else {
            // Pop and visit next state
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
bool BFSMultiSource<MULTIPLE_FINAL>::expand_neighbors(const MSSearchState& current_state, uint64_t current_distance)
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
            auto reached_node = ObjectId(iter->get_reached_node());

            auto visited_state = visited.emplace(transition.to, reached_node);
            auto reached_state = visited_state.first.operator->();

            // If next state was visited for the first time
            if (visited_state.second) {
                open.push({ reached_state, current_distance + 1 });

                // iterate over the starting nodes that reached the previous state
                for (auto&& [start_idx, prev_info] : current_state.start2previous) {
                    if (prev_info.distance != current_distance) {
                        continue;
                    }
                    Transition path_transition(&current_state, transition.type_id, transition.inverse);
                    reached_state->add_new_previous(start_idx, path_transition, prev_info.distance + 1);
                }

                if (automaton.is_final_state[reached_state->automaton_state]) {
                    for (auto&& [start_idx, prev_info] : current_state.start2previous) {
                        if (prev_info.distance != current_distance) {
                            continue;
                        }
                        ready_solutions.emplace_back(start_idx, reached_state);
                    }
                    return true;
                }
            } else {
                std::set<int> new_starts;

                for (auto&& [start_idx, prev_info] : current_state.start2previous) {
                    if (prev_info.distance != current_distance) {
                        continue;
                    }
                    if (auto it = reached_state->start2previous.find(start_idx);
                        it != reached_state->start2previous.end())
                    {
                        if (prev_info.distance + 1 == it->second.distance) {
                            Transition path_transition(
                                &current_state,
                                transition.type_id,
                                transition.inverse
                            );

                            if (it->second.try_add_previous(path_transition)) {
                                new_starts.insert(start_idx);
                            }
                        }
                    } else {
                        Transition path_transition(&current_state, transition.type_id, transition.inverse);
                        reached_state->add_new_previous(start_idx, path_transition, prev_info.distance + 1);

                        new_starts.insert(start_idx);
                    }
                }

                // add to open if there are new paths that reach this state
                if (!new_starts.empty()) {
                    open.push({ reached_state, current_distance + 1 });
                }

                // add the state as solution for each of the start indices
                if (automaton.is_final_state[reached_state->automaton_state]) {
                    for (auto start_idx : new_starts) {
                        ready_solutions.emplace_back(start_idx, reached_state);
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
    os << std::string(indent, ' ') << "Paths::AllShortest::BFSMultiSource(start: " << start
       << ", end: " << end << ")\n";
    lhs->print(os, indent + 2, stats);
}

template class Paths::AllShortest::BFSMultiSource<true>;
template class Paths::AllShortest::BFSMultiSource<false>;
