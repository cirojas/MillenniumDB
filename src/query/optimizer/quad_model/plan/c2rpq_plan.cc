#include "c2rpq_plan.h"

// #include "graph_models/quad_model/quad_model.h"
#include "graph_models/quad_model/quad_object_id.h"
#include "query/exceptions.h"
// #include "query/executor/binding_iter/paths/experimental/multisource_bfs/polaco/multiple.h"
// #include "query/executor/binding_iter/paths/experimental/multisource_bfs/polaco/naive.h"
// #include "query/executor/binding_iter/paths/experimental/multisource_bfs/polaco/optimized_bitset.h"
#include "query/executor/binding_iter/paths/experimental/multisource_bfs/all_ms_bfs_enum.h"
#include "query/executor/binding_iter/paths/experimental/multisource_bfs/ms_bfs_enum.h"
#include "query/executor/binding_iter/paths/experimental/multisource_bfs/only_endpoint.h"
#include "query/executor/binding_iter/paths/index_provider/quad_model_index_provider.h"
#include "query/query_context.h"

using namespace MQL;
using namespace Paths;

C2RPQ_Plan::C2RPQ_Plan(
    const std::set<VarId>& join_vars,
    std::unique_ptr<BindingIter> lhs,
    std::vector<bool>& begin_at_left,
    Path::Direction direction,
    VarId path_var,
    Id from,
    Id to,
    RegularPathExpr& path,
    PathSemantic path_semantic,
    uint64_t /*K*/
) :
    lhs(std::move(lhs)),
    begin_at_left(begin_at_left),
    direction(direction),
    path_var(path_var),
    from(from),
    to(to),
    // path(path),
    // K(K),
    path_semantic(path_semantic)
{
    automaton = path.get_rpq_automaton(&QuadObjectId::get_named_node);
    auto inverted_path = path.clone()->invert();
    automaton_inverted = inverted_path->get_rpq_automaton(&QuadObjectId::get_named_node);

    assert(from.is_var() && to.is_var());

    start_at_from = join_vars.find(from.get_var()) != join_vars.end();
    if (!start_at_from) {
        assert(join_vars.find(to.get_var()) != join_vars.end());
    } else {
        assert(join_vars.find(to.get_var()) == join_vars.end());
    }
}

std::unique_ptr<IndexProvider> C2RPQ_Plan::get_provider(const RPQ_DFA& automaton) const
{
    auto t_info = std::unordered_map<uint64_t, IndexType>();
    auto t_inv_info = std::unordered_map<uint64_t, IndexType>();
    for (size_t state = 0; state < automaton.from_to_connections.size(); state++) {
        for (auto& transition : automaton.from_to_connections[state]) {
            if (transition.inverse) {
                // Avoid transitions that are already stored
                if (t_inv_info.find(transition.type_id.id) != t_inv_info.end()) {
                    continue;
                }
                t_inv_info.insert({ transition.type_id.id, IndexType::BTREE });

            } else {
                // Avoid transitions that are already stored
                if (t_info.find(transition.type_id.id) != t_info.end()) {
                    continue;
                }
                t_info.insert({ transition.type_id.id, IndexType::BTREE });
            }
        }
    }
    return std::make_unique<QuadModelIndexProvider>(
        std::move(t_info),
        std::move(t_inv_info),
        &get_query_ctx().thread_info.interruption_requested
    );
}

std::unique_ptr<BindingIter> C2RPQ_Plan::get_enum(const RPQ_DFA& automaton, VarId start, VarId end)
{
    auto provider = get_provider(automaton);
    switch (path_semantic) {
    case PathSemantic::DEFAULT:
    case PathSemantic::ANY_WALKS: {
        if (automaton.total_final_states > 1) {
            return std::make_unique<Any::BFSMultiSource<true>>(
                std::move(lhs),
                path_var,
                start,
                end,
                automaton,
                std::move(provider)
            );
        } else {
            return std::make_unique<Any::BFSMultiSource<false>>(
                std::move(lhs),
                path_var,
                start,
                end,
                automaton,
                std::move(provider)
            );
        }
    }
    case PathSemantic::ALL_SHORTEST_WALKS: {
        if (automaton.total_final_states > 1) {
            return std::make_unique<AllShortest::BFSMultiSource<true>>(
                std::move(lhs),
                path_var,
                start,
                end,
                automaton,
                std::move(provider)
            );
        } else {
            return std::make_unique<AllShortest::BFSMultiSource<false>>(
                std::move(lhs),
                path_var,
                start,
                end,
                automaton,
                std::move(provider)
            );
        }
    }
    default:
        throw QuerySemanticException("PathSemantic not supported yet");
    }
}

std::unique_ptr<BindingIter> C2RPQ_Plan::get_binding_iter()
{
    bool right_to_left = direction == Path::Direction::RIGHT_TO_LEFT;

    begin_at_left[path_var.id] = start_at_from != right_to_left;
    const RPQ_DFA& used_automaton = start_at_from ? automaton : automaton_inverted;
    Id start = start_at_from ? from : to;
    Id end   = start_at_from ? to   : from;
    return get_enum(used_automaton, start.get_var(), end.get_var());
}
