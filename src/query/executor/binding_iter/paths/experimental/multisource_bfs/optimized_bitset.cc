#include "optimized_bitset.h"

#include <algorithm>
#include <bitset>
#include <boost/range/algorithm/set_algorithm.hpp>
#include <boost/unordered_set.hpp>
#include <iostream>

#include "system/path_manager.h"

using namespace std;
using namespace Paths::Any;

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::_begin(Binding& _parent_binding)
{
    parent_binding = &_parent_binding;

    //////////////////////////////////////////////////////////////
    // Override starting nodes using the nodes with label "start"
    //////////////////////////////////////////////////////////////
    // bool interruption = false;
    // std::string starting_label_str = get_starting_label_str();
    // auto label_start = QuadObjectId::get_string(starting_label_str).id;
    // auto it = quad_model.label_node
    //               ->get_range(&interruption, { label_start, 0 }, { label_start, UINT64_MAX });

    // std::cout << "starting nodes:\n";
    // for (auto record = it.next(); record != nullptr; record = it.next()) {
    //     ObjectId node((*record)[1]);
    //     std::cout << node << std::endl;
    //     start_nodes.push_back(node);
    // }
    //////////////////////////////////////////////////////////////

    lhs->begin(_parent_binding);
    while (lhs->next()) {
        ObjectId start_object_id = (*parent_binding)[start];
        if (!start_object_id.is_null()) {
            std::cout << "starting node: " << start_object_id << std::endl;
            bfss_ordered.push_back(start_object_id);
        }
    }

    num_chunks = (bfss_ordered.size() - 1) / NUM_CONCURRENT_BFS + 1;

    current_bfs_chunk = 0;
    set_new_bfs_chunk_and_reset();
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::prepare_structures_for_next_chunk_processing()
{
    for (int bfs_index = 0; bfs_index < num_nodes_in_current_chunk; bfs_index++) {
        ObjectId start_object_id = bfss_ordered[bfs_index + current_bfs_chunk * NUM_CONCURRENT_BFS];

        SearchNodeId start_node_id { automaton.start_state, start_object_id };

        bfses_to_be_visited[start_node_id] = bfs_id_bit_set(1) << bfs_index;

        search_states[bfs_index] = {
            {start_node_id,
             SearchState(automaton.start_state, start_object_id, nullptr, true, ObjectId::get_null())}
        };
        // &search_states[bfs_index].find(start_node_id)->second; // TODO: what is the purpose of this line?
        single_search_state = &search_states[bfs_index].find(start_node_id)->second;
        bfss_that_reached_given_node[start_node_id] = bfs_id_bit_set(1) << bfs_index;
        first_visit_q.push(std::make_pair(start_node_id, bfs_index));
    }
    iter = make_unique<NullIndexIterator>();
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::set_new_bfs_chunk_and_reset()
{
    assert(current_bfs_chunk < num_chunks);
    num_nodes_in_current_chunk = std::min(
        NUM_CONCURRENT_BFS,
        (int) bfss_ordered.size() - current_bfs_chunk * NUM_CONCURRENT_BFS
    );
    if (num_nodes_in_current_chunk == NUM_CONCURRENT_BFS) {
        bit_mask_for_current_chunk.set();
    } else {
        bit_mask_for_current_chunk.set();
        bit_mask_for_current_chunk >>= (NUM_CONCURRENT_BFS - num_nodes_in_current_chunk);
    }
    assert(!bit_mask_for_current_chunk.none());
    single_reset();
}

// template<bool MULTIPLE_FINAL>
// void BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::_log_structures_sizes()
// {
//     if (!_should_log_memory) {
//         return;
//     }
//     size_t structures_sizes_sum = 0;
//     for (int i = 0; i < NUM_CONCURRENT_BFS; i++) {
//         size_t structure_size = estimate_unordered_node_map_memory(search_states[i]);
//         _debug_mati() << "CALCULATE_MEMORY: structure " << i << " size: " << structure_size << std::endl;
//         structures_sizes_sum += structure_size;
//     }
//     memory_size_logger.log_size("search-states", structures_sizes_sum);
//     _debug_mati() << "CALCULATE_MEMORY: current all structures size" << structures_sizes_sum << std::endl;
//     _debug_mati() << "CALCULATE_MEMORY: max size for search-states"
//                   << StructureMemoryLogger::to_mb(memory_size_logger.get_max_size("search-states").value_or(0)
//                      )
//                   << std::endl;
// }

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::single_reset()
{
    // _log_structures_sizes();

    for (int i = 0; i < NUM_CONCURRENT_BFS; i++) {
        search_states[i].clear();
    }
    bfss_that_reached_given_node.clear();
    bfses_to_be_visited.clear();
    bfses_to_be_visited_next.clear();

    visit_q = {};
    first_visit_q = {};

    if (MULTIPLE_FINAL) {
        for (int i = 0; i < NUM_CONCURRENT_BFS; i++) {
            reached_final[i].clear();
        }
    }

    prepare_structures_for_next_chunk_processing();
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::_reset()
{
    // Empty open and visited
    // visited_nodes_counter.reset();
    current_bfs_chunk = 0;
    set_new_bfs_chunk_and_reset();
}

template<bool MULTIPLE_FINAL>
bool BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::_next()
{
    if (single_next()) {
        return true;
    }
    current_bfs_chunk++;
    if (current_bfs_chunk < num_chunks) {
        set_new_bfs_chunk_and_reset();
        return _next();
    } else {
        // _log_structures_sizes();
        // _log_mati(
        // ) << "CALCULATE_MEMORY: FINAL max size: "
        //   << StructureMemoryLogger::to_mb(memory_size_logger.get_max_size("search-states").value_or(0))
        //   << std::endl;
        // std::cout << "Finished bfs with counter: " << visited_nodes_counter << std::endl;
        return false;
    }
}
template<bool MULTIPLE_FINAL>
bool BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::single_next()
{
    // Check if first state is final
    while (!first_visit_q.empty()) {
        auto first_visit_q_top = first_visit_q.front();
        first_visit_q.pop();
        SearchNodeId curr_first_node = first_visit_q_top.first;

        if (provider->node_exists(curr_first_node.second.id)) {
            visit_q.push(curr_first_node);
            if (automaton.is_final_state[automaton.start_state]) {
                if (MULTIPLE_FINAL) {
                    reached_final[first_visit_q_top.second].insert(curr_first_node.second.id);
                }
                auto pointer_to_reached_state = &search_states[first_visit_q_top.second]
                                                     .find(curr_first_node)
                                                     ->second;
                auto path_id = path_manager.set_path(pointer_to_reached_state, path_var);
                parent_binding->add(path_var, path_id);
                parent_binding->add(end, curr_first_node.second);
                parent_binding->add(start, curr_first_node.second);
                // _debug_mati() << "Returning first node " << curr_first_node.second << " "
                //               << curr_first_node.first << std::endl;
                return true;
            }
        }
    }

    while (visit_q.size() > 0) {
        while (visit_q.size() > 0) {
            auto current_node_id = visit_q.front();
            auto reached_final_state = expand_neighbors(current_node_id);

            // Enumerate reached solutions
            if (reached_final_state != nullptr) {
                // _debug_mati() << "Reached final state for bfs id " << *reached_final_state << std::endl;
                // HERE IS THE LOGIC THAT OVERRIDES RETURNED PATH
                // reached_final_state = single_search_state; // TODO:
                auto path_id = path_manager.set_path(reached_final_state, path_var);
                parent_binding->add(path_var, path_id);
                parent_binding->add(end, reached_final_state->node_id);
                parent_binding->add(start, current_node_id.second);
                return true;
            } else {
                visit_q.pop();
            }
        }
        for (const auto& it : bfses_to_be_visited_next) {
            if (!it.second.none()) {
                visit_q.push(it.first);
            }
        }
        // _debug_mati() << "bfses_to_be_visited_next.size()" << bfses_to_be_visited_next.size() << std::endl;
        bfses_to_be_visited = bfses_to_be_visited_next;
        bfses_to_be_visited_next.clear();
    }
    return false;
}

template<bool MULTIPLE_FINAL>
const SearchState*
    BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::expand_neighbors(const SearchNodeId& current_state_id)
{
    // Check if this is the first time that current_state is explored
    if (iter->at_end()) {
        // _debug_mati() << "state explored first time " << std::endl;
        current_transition = 0;
        // Check if automaton state has transitions
        if (automaton.from_to_connections[current_state_id.first].size() == 0) {
            // _debug_mati() << "automaton has no transitions for " << current_state_id.first << std::endl;
            return nullptr;
        }
        set_iter(current_state_id);
    }

    while (!search_states_for_current_iteration.empty()) {
        // _debug_mati() << "search states for current iteration not empty! "
        //                  "search states_for_current_iteration.size() = "
        //               << search_states_for_current_iteration.size() << std::endl;
        auto ret = search_states_for_current_iteration.front();
        search_states_for_current_iteration.pop();
        return ret;
    }

    auto bfses_to_be_visited_by_current_state = bfses_to_be_visited[current_state_id];
    // Iterate over the remaining transitions of current_state
    // Don't start from the beginning, resume where it left thanks to
    // current_transition and iter (pipeline)
    while (current_transition < automaton.from_to_connections[current_state_id.first].size()) {
        auto& transition = automaton.from_to_connections[current_state_id.first][current_transition];

        // Iterate over records until a final state is reached
        while (iter->next()) {
            // _debug_mati() << "current_state_id: " << current_state_id.first << ", " << current_state_id.second
            //               << std::endl;

            // _debug_mati() << "bfses_to_be_visited_by_current_state: ";
            // debug_print_bfs_id_bit_set(bfses_to_be_visited_by_current_state);
            // _debug_mati_simple() << std::endl;

            // visited_nodes_counter.increment();

            SearchNodeId new_node_id = { transition.to, ObjectId(iter->get_reached_node()) };
            // _debug_mati() << "new_node_id: " << new_node_id.first << ", " << new_node_id.second << std::endl;
            auto bfses_that_reached_the_new_state = bfss_that_reached_given_node[new_node_id];

            // _debug_mati() << "bfses_that_reached_the_new_state ";
            // debug_print_bfs_id_bit_set(bfses_that_reached_the_new_state);
            // _debug_mati_simple() << std::endl;

            // _debug_mati() << "bfses_that_reached_the_new_state: ";
            // debug_print_container(bfses_that_reached_the_new_state);
            // _debug_mati_simple() << std::endl;

            //            boost::set_difference(bfses_to_be_visited_by_current_state,
            //            bfses_that_reached_the_new_state, std::inserter(difference,
            //            difference.end()));

            bfs_id_bit_set difference = bfses_to_be_visited_by_current_state
                                      & ~bfses_that_reached_the_new_state;
            difference &= bit_mask_for_current_chunk;

            // _debug_mati() << "difference: ";
            // debug_print_bfs_id_bit_set(difference);
            // _debug_mati_simple() << std::endl;
            // _debug_mati() << "difference size: " << difference.size() << std::endl;
            if (!difference.none()) {
                bfses_to_be_visited_next[new_node_id] |= difference;
                bfss_that_reached_given_node[new_node_id] |= difference;

                bool is_final = automaton.is_final_state[new_node_id.first];
                if (is_final) {
                    search_states_for_current_iteration = {};
                    // _debug_mati() << "new state is final state " << std::endl;
                }
                // #pragma GCC unroll 64
                // for (int bfs_index = difference._Find_first(); bfs_index < NUM_CONCURRENT_BFS;
                //      bfs_index = difference._Find_next(bfs_index))
                // {
                for (int bfs_index = 0; bfs_index < NUM_CONCURRENT_BFS; bfs_index++) {
                    if (!difference[bfs_index]) {
                        continue;
                    }
                    // _debug_mati() << "bfs_index: " << bfs_index << std::endl;
                    auto it = search_states[bfs_index].find(current_state_id);
                    assert(it != search_states[bfs_index].end());
                    auto previous = &(it->second);
                    SearchState s(
                        new_node_id.first,
                        new_node_id.second,
                        previous,
                        transition.inverse,
                        transition.type_id
                    );
                    auto insertion_result = search_states[bfs_index].insert({ new_node_id, s });

                    if (is_final) {
                        if (MULTIPLE_FINAL) {
                            if (reached_final[bfs_index].find(new_node_id.second.id)
                                == reached_final[bfs_index].end())
                            {
                                reached_final[bfs_index].insert(new_node_id.second.id);
                                search_states_for_current_iteration.push(&insertion_result.first->second);
                            } else {
                                // _debug_mati()
                                // << "For bfs_index " << bfs_index << "the final state "
                                // << new_node_id.second.id << " was already reached!" << std::endl;
                            }
                        } else {
                            search_states_for_current_iteration.push(&insertion_result.first->second);
                        }
                    }
                }

                if (is_final) {
                    if (!search_states_for_current_iteration.empty()) {
                        // _debug_mati() << "search states for current iteration not empty! "
                        //                  "search states_for_current_iteration.size() = "
                        //               << search_states_for_current_iteration.size() << std::endl;
                        auto ret = search_states_for_current_iteration.front();
                        search_states_for_current_iteration.pop();
                        return ret;
                    }
                }
            }
        }

        // Construct new iter with the next transition (if there exists one)
        current_transition++;
        if (current_transition < automaton.from_to_connections[current_state_id.first].size()) {
            set_iter(current_state_id);
        }
    }
    return nullptr;
}

template<bool MULTIPLE_FINAL>
void BFSMultipleStartsOptimizedBitset<MULTIPLE_FINAL>::print(std::ostream& os, int indent, bool stats) const
{
    if (stats) {
        os << std::string(indent, ' ') << "[begin: " << stat_begin << " next: " << stat_next
           << " reset: " << stat_reset << " results: " << results << " idx_searches: " << idx_searches
           << "]\n";
    }
    os << std::string(indent, ' ') << "Paths::Any::BFSMultipleStartsOptimizedBitset(path_var: " << path_var
       << ", start: " << start << ", end: " << end << ")\n";
    lhs->print(os, indent + 2, stats);
}

template class Paths::Any::BFSMultipleStartsOptimizedBitset<true>;
template class Paths::Any::BFSMultipleStartsOptimizedBitset<false>;
