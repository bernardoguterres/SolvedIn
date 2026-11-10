#pragma once

#include "solvedin/common.hpp"
#include "solvedin/trie.hpp"
#include "solvedin/wend_board.hpp"

#include <vector>

namespace solvedin {

struct WendSolveResult {
    SolveStatus status{SolveStatus::NO_SOLUTION};
    std::vector<WendSolution> solutions;
    SolveStats stats;
};

class WendSolver {
public:
    WendSolver(const WendBoard& board, const Trie& lexicon, SolveOptions options = {});

    [[nodiscard]] WendSolveResult solve();
    [[nodiscard]] WendSolveResult solve(bool check_unique);

    [[nodiscard]] std::vector<WendWordPath> enumerate_candidate_paths();

private:
    const WendBoard& board_;
    const Trie& lexicon_;
    SolveOptions options_;
};

} // namespace solvedin
