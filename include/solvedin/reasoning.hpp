#pragma once

// Shared vocabulary for the explainable ("logic") solvers.
//
// Every grid game exposes three runs built on the same step log:
//   hint     the first useful deduction from the board as it stands (nothing is guessed)
//   deduce   repeated sound deductions only; stops with a partial board when no move can be proved
//   explain  deductions first, then bounded backtracking with the same deductions inside every branch,
//            then a separate search that proves (or disproves) uniqueness
// Steps keep forced moves, speculative branches, failed branches and the uniqueness check distinct.

#include "solvedin/common.hpp"

#include <cstddef>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace solvedin {

enum class StepKind {
    DEDUCTION,      // forced by a named rule from the current board
    BRANCH,         // speculative choice (fallback search)
    CONTRADICTION,  // the current branch broke a rule; it is abandoned
    SOLUTION,       // a complete board that the independent validator accepted
    STALL,          // deduce-only run: no rule applies, board is partial
    HYPOTHESIS      // semantic games: a ranked suggestion, not a proof
};

enum class SearchPhase {
    SOLVE,          // looking for the first solution
    VERIFY_UNIQUE   // first solution found; searching the rest of the tree for a second one
};

struct CellChange {
    Position cell;
    std::string value;  // e.g. "S", "M", "Q", "x", "not 4", "4"
};

struct ReasoningStep {
    StepKind kind{StepKind::DEDUCTION};
    SearchPhase phase{SearchPhase::SOLVE};
    int depth{0};                    // number of speculative choices currently in force
    std::string rule;                // short rule id, e.g. "line-patterns"
    std::string explanation;         // one readable sentence (or a few lines)
    std::vector<Position> involved;  // premise cells/clues the rule looked at
    std::vector<CellChange> changes; // placements or eliminations it made
    std::string board;               // rendered board, attached at branches, stalls and solutions
};

struct ReasoningStats {
    size_t deductions{0};
    size_t branches{0};           // speculative decisions taken (each value tried counts once)
    size_t failed_branches{0};    // branches that ended in a contradiction
    size_t solution_nodes{0};     // search nodes until the first solution (or until the search ended)
    size_t uniqueness_nodes{0};   // search nodes spent after the first solution, proving uniqueness
    double solution_ms{0.0};
    double uniqueness_ms{0.0};
    bool budget_exhausted{false};
    std::map<std::string, size_t> rule_counts;  // deductions by rule
    std::map<std::string, size_t> work;         // game-specific work counters (e.g. line patterns enumerated)
};

enum class ReasoningOutcome {
    SOLVED_BY_DEDUCTION,   // unique, no speculative branch was needed
    SOLVED_WITH_FALLBACK,  // unique, needed at least one branch
    MULTIPLE_SOLUTIONS,
    NO_SOLUTION,
    STALLED,               // deduce-only: partial board, no provable move
    HINT,                  // hint run: one step returned
    NO_HINT,               // hint run: no rule applies (the next move would be a guess)
    INCONCLUSIVE,          // budget exhausted
    ANSWERED,              // semantic games: evidence was strong enough to commit to a hypothesis
    ABSTAINED              // semantic games: evidence too weak after every revealed clue
};

struct ReasoningReport {
    ReasoningOutcome outcome{ReasoningOutcome::NO_SOLUTION};
    std::vector<std::string> notes;           // assumptions printed before the steps (e.g. which dictionary)
    std::vector<ReasoningStep> steps;
    ReasoningStats stats;
    std::string final_board;                  // solution or partial board
    std::vector<std::string> solutions_text;  // rendered solutions (at most 2)
};

enum class ReasoningMode { HINT, DEDUCE, EXPLAIN };

struct ReasoningOptions {
    ReasoningMode mode{ReasoningMode::EXPLAIN};
    bool check_unique{true};
    size_t max_nodes{0};       // 0 = unlimited
    double max_time_ms{0.0};   // 0 = unlimited
    size_t max_logged_steps{100000};  // keeps memory bounded on large searches; counts are always complete
    bool basic_rules{false};   // only the simplest rule tier (e.g. Sudoku singles), to measure what stronger rules buy
};

[[nodiscard]] const char* to_string(StepKind k) noexcept;
[[nodiscard]] const char* to_string(SearchPhase p) noexcept;
[[nodiscard]] const char* to_string(ReasoningOutcome o) noexcept;

// Human-readable trace. max_print = 0 prints every logged step.
void print_reasoning_report(std::ostream& os, const ReasoningReport& report, ReasoningMode mode, size_t max_print = 0);

// "(r, c)" with 0-based coordinates, matching the rest of the CLI output.
[[nodiscard]] std::string cell_name(Position p);

} // namespace solvedin
