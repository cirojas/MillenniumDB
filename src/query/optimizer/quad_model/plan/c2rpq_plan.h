#pragma once

#include "query/executor/binding_iter.h"
#include "query/executor/binding_iter/paths/index_provider/path_index.h"
#include "query/parser/op/mql/graph_pattern/path.h"
#include "query/parser/paths/regular_path_expr.h"

namespace MQL {
class C2RPQ_Plan {
public:
    C2RPQ_Plan(
        const std::set<VarId>& join_vars,
        std::unique_ptr<BindingIter> lhs,
        std::vector<bool>& begin_at_left,
        Path::Direction direction,
        VarId path_var,
        Id from,
        Id to,
        RegularPathExpr& path,
        PathSemantic semantic,
        uint64_t K
    );

    std::unique_ptr<BindingIter> get_binding_iter();

private:
    std::unique_ptr<BindingIter> lhs;

    std::vector<bool>& begin_at_left;
    Path::Direction direction;
    const VarId path_var;
    const Id from;
    const Id to;
    // RegularPathExpr& path;
    // uint64_t K;

    PathSemantic path_semantic;

    RPQ_DFA automaton;
    RPQ_DFA automaton_inverted;

    // if false the path will start at to
    bool start_at_from;

    // Construct index provider for this automaton
    std::unique_ptr<Paths::IndexProvider> get_provider(const RPQ_DFA& automaton) const;

    // std::unique_ptr<BindingIter> get_check(const RPQ_DFA& automaton, Id start, Id end) const;
    std::unique_ptr<BindingIter> get_enum(const RPQ_DFA& automaton, VarId start, VarId end);
    // std::unique_ptr<BindingIter> get_unfixed(const RPQ_DFA& automaton, VarId start, VarId end) const;
};
} // namespace MQL
