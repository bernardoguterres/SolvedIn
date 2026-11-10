// Explainable Crossclimb reconstruction (supplied words) vs the original solver.
#include "solvedin/crossclimb_board.hpp"
#include "solvedin/crossclimb_logic.hpp"
#include "solvedin/crossclimb_solver.hpp"
#include "solvedin/trie.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

using namespace solvedin;

int main() {
    auto lex = Trie::get_default_lexicon();
    int n = 0;
    for (const auto& dir : {"fixtures", "fixtures/corpus/crossclimb"}) {
        for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
            const std::string p = e.path().string();
            if (!e.is_regular_file() || p.find("crossclimb") == std::string::npos || p.find("malformed") != std::string::npos) continue;
            auto parsed = CrossclimbBoard::load_from_file(p);
            if (!parsed.success) continue;
            CrossclimbSolver solver(*parsed.board, lex);
            auto base = solver.solve(true);
            auto rep = reason_crossclimb(*parsed.board, lex);
            if (base.status == SolveStatus::UNIQUE_SOLUTION) {
                CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || rep.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
                for (const auto& w : base.solutions[0].ladder) CHECK_ASSERT(rep.final_board.find(w) != std::string::npos);
                CHECK_ASSERT(rep.final_board.find("letter ") != std::string::npos);  // the changing letter is shown
            } else if (base.status == SolveStatus::MULTIPLE_SOLUTIONS) {
                CHECK_ASSERT(rep.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS);
            } else if (base.status == SolveStatus::NO_SOLUTION) {
                CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
            }
            ++n;
        }
    }
    CHECK_ASSERT(n > 0);
    std::cout << "[PASS] explainable Crossclimb reconstruction agrees with the original solver on " << n << " boards\n";
    return 0;
}
