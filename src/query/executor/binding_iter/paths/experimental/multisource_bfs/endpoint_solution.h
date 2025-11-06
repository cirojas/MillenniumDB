#pragma once

#include "graph_models/object_id.h"

namespace Paths {
struct EndpointSolution {
    int start_index;
    ObjectId end;

    EndpointSolution(int start_index, ObjectId end) :
        start_index(start_index),
        end(end)
    { }

    bool operator==(const EndpointSolution& other) const
    {
        return int(this->start_index == other.start_index) & int(this->end == other.end);
    }

    bool operator<(const EndpointSolution& other) const
    {
        if (this->start_index != other.start_index)
            return this->start_index < other.start_index;
        else
            return this->end < other.end;
    }

    struct Hasher {
        std::size_t operator()(const EndpointSolution& s) const
        {
            return s.end.id;
        }
    };
};
} // namespace Paths
