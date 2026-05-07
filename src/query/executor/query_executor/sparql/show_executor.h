#pragma once

#include "graph_models/object_id.h"
#include "network/sparql/response_type.h"
#include "query/executor/query_executor/query_executor.h"
#include "query/parser/op/sparql/op_show.h"

#include <vector>

namespace SPARQL {

template<ResponseType, OpShow::Type>
class ShowExecutor : public QueryExecutor {
public:
    ShowExecutor();

    uint64_t execute(std::ostream&) override;

    void analyze(std::ostream&, bool print_stats = false, int indent = 0) const override;

    static void print(std::ostream& os, ObjectId oid);

private:
    std::vector<VarId> projection_vars;
};
} // namespace SPARQL
