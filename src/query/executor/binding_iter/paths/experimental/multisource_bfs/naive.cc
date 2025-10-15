#include "naive.h"

#include <boost/range/algorithm/set_algorithm.hpp>
#include <iostream>

#include "system/path_manager.h"

using namespace std;
using namespace Paths::Any;

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsNaive<MULTIPLE_FINAL>::_begin(Binding& _parent_binding)
{
    parent_binding = &_parent_binding;
    // first_next = true;

    //////////////////////////////////////////////////////////////
    // Override starting nodes using the nodes with label "start"
    //////////////////////////////////////////////////////////////
    //   bool interruption = false;
    //   std::string starting_label_str = get_starting_label_str();
    //   auto label_start = QuadObjectId::get_string(starting_label_str).id;
    //   auto it = quad_model.label_node->get_range(&interruption, {label_start, 0},
    //                                              {label_start, UINT64_MAX});

    //   std::cout << "starting nodes:\n";
    //   for (auto record = it.next(); record != nullptr; record = it.next()) {
    //     ObjectId node((*record)[1]);
    //     std::cout << node << std::endl;
    //     start_nodes.push_back(node);
    //   }
    //////////////////////////////////////////////////////////////

    lhs->begin(_parent_binding);
    while (lhs->next()) {
        ObjectId start_object_id = (*parent_binding)[start];
        if (!start_object_id.is_null()) {
            std::cout << "starting node: " << start_object_id << std::endl;
            start_nodes.push_back(start_object_id);
        }
    }

    for (auto node : start_nodes) {
        start_nodes_q.push(node);
    }
    set_new_start_node();
    auto state_inserted = visited.emplace(
        automaton.start_state,
        current_start_node,
        nullptr,
        true,
        ObjectId::get_null()
    );
    the_only_search_state = state_inserted.first.operator->();
    open.push(state_inserted.first.operator->());
    iter = make_unique<NullIndexIterator>();
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsNaive<MULTIPLE_FINAL>::set_new_start_node()
{
    assert(start_nodes_q.size() >= 1);
    current_start_node = start_nodes_q.front();
    start_nodes_q.pop();
}

template<bool MULTIPLE_FINAL>
bool BFSMultipleStartsNaive<MULTIPLE_FINAL>::single_next()
{
    // Check if first state is final
    if (first_next) {
        first_next = false;
        auto current_state = open.front();

        // Return false if node does not exist in the database
        if (!provider->node_exists(current_state->node_id.id)) {
            open.pop();
            return false;
        }

        // Starting state is solution
        if (automaton.is_final_state[automaton.start_state]) {
            auto reached_state = SearchState(
                automaton.start_state,
                current_state->node_id,
                nullptr,
                true,
                ObjectId::get_null()
            );
            if (MULTIPLE_FINAL) {
                reached_final.insert(current_state->node_id.id);
            }
            auto path_id = path_manager.set_path(visited.insert(reached_state).first.operator->(), path_var);
            parent_binding->add(path_var, path_id);
            parent_binding->add(end, current_state->node_id);
            return true;
        }
    }

    while (open.size() > 0) {
        auto current_state = open.front();
        auto reached_final_state = expand_neighbors(*current_state);

        // Enumerate reached solutions
        if (reached_final_state != nullptr) {
            if (the_only_search_state == nullptr) {
                the_only_search_state = reached_final_state;
            }
            // HERE IS THE LOGIC THAT OVERRIDES RETURNED PATH
            reached_final_state = the_only_search_state;
            auto path_id = path_manager.set_path(reached_final_state, path_var);
            parent_binding->add(path_var, path_id);
            parent_binding->add(end, reached_final_state->node_id);
            return true;
        } else {
            // Pop and visit next state
            open.pop();
        }
    }
    return false;
}

// template<bool MULTIPLE_FINAL>
// void BFSMultipleStartsNaive<MULTIPLE_FINAL>::_log_structures_sizes()
// {
//     if (!_should_log_memory) {
//         return;
//     }
//     size_t visited_size = estimate_unordered_node_set_memory(visited);
//     memory_size_logger.log_size("visited", visited_size);
//     _debug_mati() << "CALCULATE_MEMORY: current visited size " << visited_size << std::endl;
//     _debug_mati() << "CALCULATE_MEMORY: max size for visited"
//                   << StructureMemoryLogger::to_mb(memory_size_logger.get_max_size("visited").value_or(0))
//                   << std::endl;
// }

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsNaive<MULTIPLE_FINAL>::single_reset()
{
    // _log_structures_sizes();

    // Empty open and visited
    queue<const SearchState*> empty;
    open.swap(empty);
    visited.clear();
    if (MULTIPLE_FINAL) {
        reached_final.clear();
    }
    first_next = true;
    the_only_search_state = nullptr;

    auto state_inserted = visited.emplace(
        automaton.start_state,
        current_start_node,
        nullptr,
        true,
        ObjectId::get_null()
    );
    the_only_search_state = state_inserted.first.operator->();
    open.push(state_inserted.first.operator->());
    iter = make_unique<NullIndexIterator>();
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsNaive<MULTIPLE_FINAL>::_reset()
{
    // visited_nodes_counter.reset();
    for (auto node : start_nodes) {
        start_nodes_q.push(node);
    }
    set_new_start_node();
    single_reset();
}

template<bool MULTIPLE_FINAL>
bool BFSMultipleStartsNaive<MULTIPLE_FINAL>::_next()
{
    if (single_next()) {
        return true;
    }
    if (!start_nodes_q.empty()) {
        set_new_start_node();
        single_reset();
        return _next();
    } else {
        // _log_structures_sizes();
        // _log_mati() << "CALCULATE_MEMORY: FINAL max size: "
        //             << StructureMemoryLogger::to_mb(memory_size_logger.get_max_size("visited").value_or(0))
        //             << std::endl;
        // std::cout << "Finished bfs with counter: " << visited_nodes_counter << std::endl;
        return false;
    }
}

template<bool MULTIPLE_FINAL>
const SearchState* BFSMultipleStartsNaive<MULTIPLE_FINAL>::expand_neighbors(const SearchState& current_state)
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
    // Don't start from the beginning, resume where it left thanks to
    // current_transition and iter (pipeline)
    while (current_transition < automaton.from_to_connections[current_state.automaton_state].size()) {
        auto& transition = automaton.from_to_connections[current_state.automaton_state][current_transition];

        // Iterate over records until a final state is reached
        while (iter->next()) {
            SearchState next_state(
                transition.to,
                ObjectId(iter->get_reached_node()),
                &current_state,
                transition.inverse,
                transition.type_id
            );
            // visited_nodes_counter.increment();
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
void BFSMultipleStartsNaive<MULTIPLE_FINAL>::print(std::ostream& os, int indent, bool stats) const
{
    if (stats) {
        os << std::string(indent, ' ') << "[begin: " << stat_begin << " next: " << stat_next
           << " reset: " << stat_reset << " results: " << results << " idx_searches: " << idx_searches
           << "]\n";
    }
    os << std::string(indent, ' ') << "Paths::Any::BFSMultipleStartsNaive(path_var: " << path_var
       << ", start: " << start << ", end: " << end << ")\n";
    lhs->print(os, indent + 2, stats);
}

template class Paths::Any::BFSMultipleStartsNaive<true>;
template class Paths::Any::BFSMultipleStartsNaive<false>;
