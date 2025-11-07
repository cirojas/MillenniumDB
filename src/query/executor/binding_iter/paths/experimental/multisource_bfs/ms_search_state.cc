#include "ms_search_state.h"

using namespace Paths::Any;

void MSSearchStateSolution::for_each(
    std::function<void(ObjectId)> node_func,
    std::function<void(ObjectId, bool)> edge_func,
    bool begin_at_left
) const
{
    uint32_t start_idx = this->start_index;
    auto start = this->start_node;

    if (begin_at_left) {
        auto cur_state = this->state;

        std::vector<ObjectId> nodes;
        std::vector<ObjectId> edges;
        std::vector<bool> inverse_directions;

        auto it = cur_state->previous.find(start_idx);
        while (it != cur_state->previous.end()) {
            Transition& transition = it->second;
            if (transition.state == nullptr) {
                break;
            }
            nodes.push_back(cur_state->node_id);
            edges.push_back(transition.type_id);
            inverse_directions.push_back(transition.inverse_direction);

            cur_state = transition.state;
            it = cur_state->previous.find(start_idx);
        }

        node_func(start);
        for (int_fast32_t i = nodes.size() - 1; i >= 0; --i) {
            edge_func(edges[i], inverse_directions[i]);
            node_func(nodes[i]);
        }
    } else {
        auto cur_state = this->state;

        auto it = cur_state->previous.find(start_idx);
        while (it != cur_state->previous.end()) {
            Transition transition = cur_state->previous[start_idx];
            if (transition.state == nullptr) {
                break;
            }
            node_func(cur_state->node_id);
            edge_func(transition.type_id, !transition.inverse_direction);

            cur_state = transition.state;
            it = cur_state->previous.find(start_idx);
        }

        node_func(start);
    }
}
