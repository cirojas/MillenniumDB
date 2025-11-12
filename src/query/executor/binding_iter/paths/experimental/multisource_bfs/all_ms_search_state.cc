#include "all_ms_search_state.h"

using namespace Paths::AllShortest;

bool MSSearchStateSolution::has_next()
{
    for (int i = 0; i < static_cast<int>(iter_state_cur.size()); i++) {
        iter_state_cur[i]++;
        if (iter_state_cur[i] != iter_state_end[i]) {
            auto current_transition = iter_state_cur[i];
            for (int j = i - 1; j >= 0; j--) {
                current_path_edges[j] = current_transition->type_id;
                inverse_directions[j] = current_transition->inverse_direction;
                current_path_nodes[j] = current_transition->state->node_id;
                auto it = current_transition->state->start2previous.find(start_idx);
                assert(it != current_transition->state->start2previous.end());
                iter_state_cur[j] = it->second.previous.begin();
                iter_state_end[j] = it->second.previous.end();

                current_transition = iter_state_cur[j];

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

void MSSearchStateSolution::start_enumeration()
{
    at_end = false;

    auto previous_info_it = state->start2previous.find(start_idx);
    assert(previous_info_it != state->start2previous.end());

    auto path_distance = previous_info_it->second.distance;
    iter_state_cur.resize(path_distance);
    iter_state_end.resize(path_distance);

    current_path_nodes.resize(path_distance + 1);
    current_path_edges.resize(path_distance);
    inverse_directions.resize(path_distance);

    current_path_nodes[path_distance] = state->node_id;

    auto current_transition = std::prev(previous_info_it->second.previous.end());
    for (int j = iter_state_cur.size() - 1; j >= 0; j--) {
        assert(current_transition->state != nullptr);
        current_path_edges[j] = current_transition->type_id;
        inverse_directions[j] = current_transition->inverse_direction;
        current_path_nodes[j] = current_transition->state->node_id;

        auto it = current_transition->state->start2previous.find(start_idx);
        assert(it != current_transition->state->start2previous.end());
        iter_state_cur[j] = it->second.previous.begin();
        iter_state_end[j] = it->second.previous.end();

        current_transition = iter_state_cur[j];
        if (j == 0) {
            assert(current_transition->state == nullptr);
        }
    }
}

void MSSearchStateSolution::for_each(
    std::function<void(ObjectId)> node_func,
    std::function<void(ObjectId, bool)> edge_func,
    bool begin_at_left
) const
{
    if (!begin_at_left) {
        node_func(current_path_nodes.back());
        for (int i = current_path_edges.size() - 1; i >= 0; --i) {
            edge_func(current_path_edges[i], inverse_directions[i]);
            node_func(current_path_nodes[i]);
        }
    } else {
        for (int i = 0; i < (int) current_path_edges.size(); ++i) {
            node_func(current_path_nodes[i]);
            edge_func(current_path_edges[i], inverse_directions[i]);
        }

        node_func(current_path_nodes.back());
    }
}
