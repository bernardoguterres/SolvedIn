#pragma once

#include "solvedin/common.hpp"
#include "solvedin/crossclimb_board.hpp"
#include "solvedin/trie.hpp"

#include <vector>

namespace solvedin {

struct CrossclimbSolveResult {
    SolveStatus status{SolveStatus::NO_SOLUTION};
    std::vector<CrossclimbSolution> solutions;
    SolveStats stats;
};

class CrossclimbSolver {
public:
    CrossclimbSolver(const CrossclimbBoard& board, const Trie& lexicon, SolveOptions options = {});

    [[nodiscard]] CrossclimbSolveResult solve();
    [[nodiscard]] CrossclimbSolveResult solve(bool check_unique);

private:
    const CrossclimbBoard& board_;
    const Trie& lexicon_;
    SolveOptions options_;
};

} // namespace solvedin
