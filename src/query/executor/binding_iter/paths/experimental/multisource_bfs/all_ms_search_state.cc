#include "all_ms_search_state.h"

using namespace Paths::AllShortest;

bool MultiSourceSearchStateSolution::has_next()
{
    for (int i = 0; i < static_cast<int>(iter_state_cur.size()); i++) {
        iter_state_cur[i]++;
        if (iter_state_cur[i] != iter_state_end[i]) {
            Transition* current_transition = iter_state_cur[i].base();
            for (int j = i - 1; j >= 0; j--) {
                current_path_edges[j] = current_transition->type_id;
                inverse_directions[j] = current_transition->inverse_direction;
                current_path_nodes[j] = current_transition->state->node_id;
                auto it = current_transition->state->start2previous.find(start_idx);
                assert(it != current_transition->state->start2previous.end());
                iter_state_cur[j] = it->second.previous.begin();
                iter_state_end[j] = it->second.previous.end();

                current_transition = iter_state_cur[j].base();

                if (j == 0) {
                    assert(current_transition->state == nullptr);
                }
            }
            return true;
        }
    }

    at_end = true;
    return false;
}

void MultiSourceSearchStateSolution::start_enumeration()
{
    at_end = false;

    auto previous_info_it = state->start2previous.find(start_idx);
    assert(previous_info_it != state->start2previous.end());

    // TODO: maybe save additional info for the first transition?

    // TODO: previous_info.distance is the correct size?
    auto path_distance = previous_info_it->second.distance;
    iter_state_cur.resize(path_distance);
    iter_state_end.resize(path_distance);

    current_path_nodes.resize(path_distance + 1);
    current_path_edges.resize(path_distance);
    inverse_directions.resize(path_distance);

    current_path_nodes[path_distance] = state->node_id;

    Transition* current_transition = &previous_info_it->second.previous.back();
    for (int j = iter_state_cur.size() - 1; j >= 0; j--) {
        assert(current_transition->state != nullptr);
        current_path_edges[j] = current_transition->type_id;
        inverse_directions[j] = current_transition->inverse_direction;
        current_path_nodes[j] = current_transition->state->node_id;

        auto it = current_transition->state->start2previous.find(start_idx);
        assert(it != current_transition->state->start2previous.end());
        iter_state_cur[j] = it->second.previous.begin();
        iter_state_end[j] = it->second.previous.end();

        current_transition = iter_state_cur[j].base();
        if (j == 0) {
            assert(current_transition->state == nullptr);
        }
    }
}

void MultiSourceSearchStateSolution::print(
    std::ostream& os,
    std::function<void(std::ostream& os, ObjectId)> print_node,
    std::function<void(std::ostream& os, ObjectId, bool)> print_edge,
    bool begin_at_left
) const
{
    if (!begin_at_left) {
        print_node(os, current_path_nodes.back());
        for (int i = current_path_edges.size() - 1; i >= 0; --i) {
            print_edge(os, current_path_edges[i], inverse_directions[i]);
            print_node(os, current_path_nodes[i]);
        }
    } else {
        for (int i = 0; i < (int) current_path_edges.size(); ++i) {
            print_node(os, current_path_nodes[i]);
            print_edge(os, current_path_edges[i], inverse_directions[i]);
        }

        print_node(os, current_path_nodes.back());
    }
    os.flush();
}
