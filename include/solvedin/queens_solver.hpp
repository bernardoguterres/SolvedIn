#pragma once

#include "solvedin/board.hpp"
#include "solvedin/common.hpp"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace solvedin {

enum class SolverVariant {
    BASELINE, // Row-by-row backtracking with flat vector representation
    MRV,      // Minimum Remaining Values heuristic with standard vector tracking
    BITSET    // Minimum Remaining Values heuristic with multi-word bitsets
};

[[nodiscard]] constexpr std::string_view to_string(SolverVariant v) noexcept {
    switch (v) {
    case SolverVariant::BASELINE: return "baseline";
    case SolverVariant::MRV:      return "mrv";
    case SolverVariant::BITSET:   return "bitset";
    }
    return "unknown";
}

[[nodiscard]] std::optional<SolverVariant> parse_solver_variant(std::string_view str) noexcept;

struct QueensSolution {
    std::vector<Position> queens;

    bool operator==(const QueensSolution& other) const = default;
};

struct SolveResult {
    SolveStatus status{SolveStatus::NO_SOLUTION};
    std::vector<QueensSolution> solutions;
    SolveStats stats;
    SolverVariant variant{SolverVariant::BASELINE};
};

/// Unified solver interface supporting Baseline, MRV, and BitSet variants.
class QueensSolver {
public:
    explicit QueensSolver(const Board& board, SolverVariant variant = SolverVariant::BASELINE);

    void set_options(SolveOptions options) noexcept { options_ = options; }
    void set_max_nodes(size_t max_nodes) noexcept { options_.max_nodes = max_nodes; }
    void set_max_time_ms(double max_time_ms) noexcept { options_.max_time_ms = max_time_ms; }

    SolveResult solve(bool check_unique = false);

private:
    const Board& board_;
    SolverVariant variant_;
    SolveOptions options_;

    SolveResult solve_baseline(bool check_unique);
    SolveResult solve_mrv(bool check_unique);
    SolveResult solve_bitset(bool check_unique);
};

/// Convenience function to solve the board with the chosen variant and options.
SolveResult solve_queens(const Board& board, bool check_unique = false,
                         SolverVariant variant = SolverVariant::BASELINE,
                         SolveOptions options = {});

} // namespace solvedin
