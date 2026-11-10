#include "solvedin/queens_solver.hpp"
#include "solvedin/bitset.hpp"
#include "solvedin/queens_validator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace solvedin {

std::optional<SolverVariant> parse_solver_variant(std::string_view str) noexcept {
    if (str == "baseline") return SolverVariant::BASELINE;
    if (str == "mrv")      return SolverVariant::MRV;
    if (str == "bitset")   return SolverVariant::BITSET;
    return std::nullopt;
}

QueensSolver::QueensSolver(const Board& board, SolverVariant variant)
    : board_(board), variant_(variant) {
    if (board_.size() <= 0) {
        throw std::invalid_argument("Cannot solve board with non-positive size");
    }
}

SolveResult QueensSolver::solve(bool check_unique) {
    switch (variant_) {
    case SolverVariant::BASELINE:
        return solve_baseline(check_unique);
    case SolverVariant::MRV:
        return solve_mrv(check_unique);
    case SolverVariant::BITSET:
        return solve_bitset(check_unique);
    }
    return solve_baseline(check_unique);
}

// ============================================================================
// 1. BASELINE SOLVER: Sequential row-by-row backtracking with flat vector
// ============================================================================

namespace {

struct BaselineContext {
    int n;
    int max_solutions;
    size_t max_nodes{0};
    double max_time_ms{0.0};
    std::chrono::steady_clock::time_point start_time;
    bool budget_exhausted{false};
    size_t nodes_visited{0};
    std::vector<int> current_queens;
    std::vector<bool> col_used;
    std::vector<bool> region_used;
    std::vector<QueensSolution> found_solutions;
};

void backtrack_baseline(const Board& board, int row, BaselineContext& ctx) {
    ctx.nodes_visited++;

    if (ctx.max_nodes > 0 && ctx.nodes_visited >= ctx.max_nodes) {
        ctx.budget_exhausted = true;
        return;
    }
    if (ctx.max_time_ms > 0.0 && (ctx.nodes_visited & 255) == 0) {
        auto now = std::chrono::steady_clock::now();
        double cur_ms = std::chrono::duration<double, std::milli>(now - ctx.start_time).count();
        if (cur_ms >= ctx.max_time_ms) {
            ctx.budget_exhausted = true;
            return;
        }
    }

    if (static_cast<int>(ctx.found_solutions.size()) >= ctx.max_solutions) {
        return;
    }

    if (row == ctx.n) {
        QueensSolution solution;
        solution.queens.reserve(ctx.n);
        for (int r = 0; r < ctx.n; ++r) {
            solution.queens.push_back(Position{r, ctx.current_queens[r]});
        }
        ctx.found_solutions.push_back(std::move(solution));
        return;
    }

    for (int c = 0; c < ctx.n; ++c) {
        if (ctx.col_used[c]) {
            continue;
        }

        if (row > 0 && std::abs(ctx.current_queens[row - 1] - c) <= 1) {
            continue;
        }

        int reg_idx = board.get_region_index(row, c);
        if (ctx.region_used[reg_idx]) {
            continue;
        }

        ctx.col_used[c] = true;
        ctx.region_used[reg_idx] = true;
        ctx.current_queens[row] = c;

        backtrack_baseline(board, row + 1, ctx);

        ctx.col_used[c] = false;
        ctx.region_used[reg_idx] = false;
        ctx.current_queens[row] = -1;

        if (ctx.budget_exhausted || static_cast<int>(ctx.found_solutions.size()) >= ctx.max_solutions) {
            return;
        }
    }
}

} // namespace

SolveResult QueensSolver::solve_baseline(bool check_unique) {
    const int n = board_.size();
    const int max_solutions = check_unique ? 2 : 1;

    BaselineContext ctx;
    ctx.n = n;
    ctx.max_solutions = max_solutions;
    ctx.max_nodes = options_.max_nodes;
    ctx.max_time_ms = options_.max_time_ms;
    ctx.current_queens.assign(n, -1);
    ctx.col_used.assign(n, false);
    ctx.region_used.assign(n, false);
    ctx.start_time = std::chrono::steady_clock::now();

    backtrack_baseline(board_, 0, ctx);
    auto end_time = std::chrono::steady_clock::now();

    SolveResult result;
    result.variant = SolverVariant::BASELINE;
    result.solutions = std::move(ctx.found_solutions);
    result.stats.nodes_visited = ctx.nodes_visited;
    result.stats.budget_exhausted = ctx.budget_exhausted;
    result.stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - ctx.start_time).count();

    if (ctx.budget_exhausted) {
        if (result.solutions.empty()) {
            result.status = SolveStatus::INCONCLUSIVE;
        } else if (check_unique && result.solutions.size() == 1) {
            result.status = SolveStatus::INCONCLUSIVE;
        } else {
            result.status = SolveStatus::MULTIPLE_SOLUTIONS;
        }
    } else if (result.solutions.empty()) {
        result.status = SolveStatus::NO_SOLUTION;
    } else if (!check_unique) {
        result.status = SolveStatus::UNIQUE_SOLUTION;
    } else {
        result.status = (result.solutions.size() == 1)
                            ? SolveStatus::UNIQUE_SOLUTION
                            : SolveStatus::MULTIPLE_SOLUTIONS;
    }

    return result;
}

// ============================================================================
// 2. MRV SOLVER: Minimum Remaining Values with Forward Checking and LCV
// ============================================================================

namespace {

struct MRVCand {
    int cell;
    double score;
};

struct MRVContext {
    int n;
    int max_solutions;
    size_t max_nodes{0};
    double max_time_ms{0.0};
    std::chrono::steady_clock::time_point start_time;
    bool budget_exhausted{false};
    size_t nodes_visited{0};
    bool use_ratio{false};
    double w_other{0.0};

    std::vector<std::vector<int>> cells_in_row;
    std::vector<std::vector<int>> cells_in_col;
    std::vector<std::vector<int>> cells_in_reg;
    std::vector<std::vector<int>> blocked_by_cell;
    std::vector<int> reg_of_cell;
    std::vector<size_t> init_reg_size;

    std::vector<uint8_t> blocked_count;
    std::vector<bool> row_assigned;
    std::vector<bool> col_assigned;
    std::vector<bool> reg_assigned;
    std::vector<Position> current_queens;
    std::vector<QueensSolution> found_solutions;
};

inline bool mrv_has_wipeout(const MRVContext& ctx) noexcept {
    for (int r = 0; r < ctx.n; ++r) {
        if (!ctx.row_assigned[r]) {
            bool ok = false;
            for (int cell : ctx.cells_in_row[r]) {
                if (ctx.blocked_count[cell] == 0) {
                    ok = true;
                    break;
                }
            }
            if (!ok) return true;
        }
    }
    for (int c = 0; c < ctx.n; ++c) {
        if (!ctx.col_assigned[c]) {
            bool ok = false;
            for (int cell : ctx.cells_in_col[c]) {
                if (ctx.blocked_count[cell] == 0) {
                    ok = true;
                    break;
                }
            }
            if (!ok) return true;
        }
    }
    for (int reg = 0; reg < ctx.n; ++reg) {
        if (!ctx.reg_assigned[reg]) {
            bool ok = false;
            for (int cell : ctx.cells_in_reg[reg]) {
                if (ctx.blocked_count[cell] == 0) {
                    ok = true;
                    break;
                }
            }
            if (!ok) return true;
        }
    }
    return false;
}

void backtrack_mrv_impl(MRVContext& ctx) {
    ctx.nodes_visited++;

    if (ctx.max_nodes > 0 && ctx.nodes_visited >= ctx.max_nodes) {
        ctx.budget_exhausted = true;
        return;
    }
    if (ctx.max_time_ms > 0.0 && (ctx.nodes_visited & 255) == 0) {
        auto now = std::chrono::steady_clock::now();
        double cur_ms = std::chrono::duration<double, std::milli>(now - ctx.start_time).count();
        if (cur_ms >= ctx.max_time_ms) {
            ctx.budget_exhausted = true;
            return;
        }
    }

    if (static_cast<int>(ctx.found_solutions.size()) >= ctx.max_solutions) {
        return;
    }

    if (static_cast<int>(ctx.current_queens.size()) == ctx.n) {
        QueensSolution sol;
        sol.queens = ctx.current_queens;
        std::sort(sol.queens.begin(), sol.queens.end(), [](const Position& a, const Position& b) {
            return (a.row != b.row) ? (a.row < b.row) : (a.col < b.col);
        });
        ctx.found_solutions.push_back(std::move(sol));
        return;
    }

    int best_unit_type = -1; // 0 = row, 1 = col, 2 = reg
    int best_unit_idx = -1;
    size_t min_legal_cells = SIZE_MAX;
    double min_ratio = 1e9;

    // 1. Check rows
    for (int r = 0; r < ctx.n; ++r) {
        if (!ctx.row_assigned[r]) {
            size_t count = 0;
            for (int cell : ctx.cells_in_row[r]) {
                if (ctx.blocked_count[cell] == 0) {
                    count++;
                }
            }
            if (count == 0) {
                return; // Domain wipeout: dead end!
            }
            if (ctx.use_ratio) {
                double rat = static_cast<double>(count) / static_cast<double>(ctx.n);
                if (rat < min_ratio) {
                    min_ratio = rat;
                    best_unit_type = 0;
                    best_unit_idx = r;
                }
            } else {
                if (count < min_legal_cells) {
                    min_legal_cells = count;
                    best_unit_type = 0;
                    best_unit_idx = r;
                }
            }
        }
    }

    // 2. Check columns
    for (int c = 0; c < ctx.n; ++c) {
        if (!ctx.col_assigned[c]) {
            size_t count = 0;
            for (int cell : ctx.cells_in_col[c]) {
                if (ctx.blocked_count[cell] == 0) {
                    count++;
                }
            }
            if (count == 0) {
                return; // Domain wipeout: dead end!
            }
            if (ctx.use_ratio) {
                double rat = static_cast<double>(count) / static_cast<double>(ctx.n);
                if (rat < min_ratio) {
                    min_ratio = rat;
                    best_unit_type = 1;
                    best_unit_idx = c;
                }
            } else {
                if (count < min_legal_cells) {
                    min_legal_cells = count;
                    best_unit_type = 1;
                    best_unit_idx = c;
                }
            }
        }
    }

    // 3. Check regions
    for (int reg = 0; reg < ctx.n; ++reg) {
        if (!ctx.reg_assigned[reg]) {
            size_t count = 0;
            for (int cell : ctx.cells_in_reg[reg]) {
                if (ctx.blocked_count[cell] == 0) {
                    count++;
                }
            }
            if (count == 0) {
                return; // Domain wipeout: dead end!
            }
            if (ctx.use_ratio) {
                double rat = static_cast<double>(count) / static_cast<double>(ctx.init_reg_size[reg]);
                if (rat < min_ratio) {
                    min_ratio = rat;
                    best_unit_type = 2;
                    best_unit_idx = reg;
                }
            } else {
                if (count < min_legal_cells) {
                    min_legal_cells = count;
                    best_unit_type = 2;
                    best_unit_idx = reg;
                }
            }
        }
    }

    if (best_unit_type == -1) {
        return;
    }

    const std::vector<int>& candidate_pool =
        (best_unit_type == 0) ? ctx.cells_in_row[best_unit_idx]
        : (best_unit_type == 1) ? ctx.cells_in_col[best_unit_idx]
                                : ctx.cells_in_reg[best_unit_idx];

    std::vector<MRVCand> cands;
    for (int cell : candidate_pool) {
        if (ctx.blocked_count[cell] == 0) {
            int r = cell / ctx.n;
            int c = cell % ctx.n;
            int reg = ctx.reg_of_cell[cell];

            ctx.row_assigned[r] = true;
            ctx.col_assigned[c] = true;
            ctx.reg_assigned[reg] = true;
            for (int blocked_cell : ctx.blocked_by_cell[cell]) {
                ctx.blocked_count[blocked_cell]++;
            }

            bool wo = mrv_has_wipeout(ctx);

            for (int blocked_cell : ctx.blocked_by_cell[cell]) {
                ctx.blocked_count[blocked_cell]--;
            }
            ctx.row_assigned[r] = false;
            ctx.col_assigned[c] = false;
            ctx.reg_assigned[reg] = false;

            if (!wo) {
                double raw_imp = 0.0;
                for (int blocked_cell : ctx.blocked_by_cell[cell]) {
                    if (ctx.blocked_count[blocked_cell] == 0) raw_imp += 1.0;
                }
                double other_conf = 0.0;
                if (best_unit_type == 2 && ctx.w_other != 0.0) {
                    for (int dr = -1; dr <= 1; ++dr) {
                        for (int dc = -1; dc <= 1; ++dc) {
                            int nr = r + dr;
                            int nc = c + dc;
                            if (nr >= 0 && nr < ctx.n && nc >= 0 && nc < ctx.n) {
                                int idx = nr * ctx.n + nc;
                                if (ctx.blocked_count[idx] == 0) {
                                    int o_reg = ctx.reg_of_cell[idx];
                                    if (o_reg != reg && !ctx.reg_assigned[o_reg]) {
                                        other_conf += 1.0;
                                    }
                                }
                            }
                        }
                    }
                }
                double score = raw_imp + ctx.w_other * other_conf;
                cands.push_back({cell, score});
            }
        }
    }

    if (cands.empty()) {
        return;
    }

    std::sort(cands.begin(), cands.end(), [](const MRVCand& a, const MRVCand& b) {
        return a.score < b.score;
    });

    for (const auto& cand : cands) {
        if (ctx.budget_exhausted || static_cast<int>(ctx.found_solutions.size()) >= ctx.max_solutions) {
            return;
        }

        int cell = cand.cell;
        int r = cell / ctx.n;
        int c = cell % ctx.n;
        int reg = ctx.reg_of_cell[cell];

        ctx.current_queens.push_back(Position{r, c});
        ctx.row_assigned[r] = true;
        ctx.col_assigned[c] = true;
        ctx.reg_assigned[reg] = true;
        for (int blocked_cell : ctx.blocked_by_cell[cell]) {
            ctx.blocked_count[blocked_cell]++;
        }

        backtrack_mrv_impl(ctx);

        for (int blocked_cell : ctx.blocked_by_cell[cell]) {
            ctx.blocked_count[blocked_cell]--;
        }
        ctx.row_assigned[r] = false;
        ctx.col_assigned[c] = false;
        ctx.reg_assigned[reg] = false;
        ctx.current_queens.pop_back();
    }
}

} // namespace

SolveResult QueensSolver::solve_mrv(bool check_unique) {
    const int n = board_.size();
    const int total_cells = n * n;
    const int max_solutions = check_unique ? 2 : 1;

    std::vector<size_t> init_reg_size(n, 0);
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            init_reg_size[board_.get_region_index(r, c)]++;
        }
    }
    size_t min_init_reg = SIZE_MAX;
    for (int reg = 0; reg < n; ++reg) {
        if (init_reg_size[reg] < min_init_reg) min_init_reg = init_reg_size[reg];
    }
    bool use_ratio = (n >= 64 && min_init_reg >= static_cast<size_t>(n) / 2);
    double w_other = (n <= 32 || use_ratio) ? 0.0 : 1.0;

    MRVContext ctx;
    ctx.n = n;
    ctx.max_solutions = max_solutions;
    ctx.max_nodes = options_.max_nodes;
    ctx.max_time_ms = options_.max_time_ms;
    ctx.use_ratio = use_ratio;
    ctx.w_other = w_other;
    ctx.cells_in_row.resize(n);
    ctx.cells_in_col.resize(n);
    ctx.cells_in_reg.resize(n);
    ctx.blocked_by_cell.resize(total_cells);
    ctx.reg_of_cell.resize(total_cells);
    ctx.init_reg_size = std::move(init_reg_size);
    ctx.blocked_count.assign(total_cells, 0);
    ctx.row_assigned.assign(n, false);
    ctx.col_assigned.assign(n, false);
    ctx.reg_assigned.assign(n, false);

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            int idx = r * n + c;
            int reg = board_.get_region_index(r, c);
            ctx.reg_of_cell[idx] = reg;
            ctx.cells_in_row[r].push_back(idx);
            ctx.cells_in_col[c].push_back(idx);
            ctx.cells_in_reg[reg].push_back(idx);
        }
    }

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            int idx = r * n + c;
            int reg = ctx.reg_of_cell[idx];
            std::vector<bool> mark(total_cells, false);

            for (int cc = 0; cc < n; ++cc) mark[r * n + cc] = true;
            for (int rr = 0; rr < n; ++rr) mark[rr * n + c] = true;
            for (int c_idx : ctx.cells_in_reg[reg]) mark[c_idx] = true;
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int nr = r + dr;
                    int nc = c + dc;
                    if (nr >= 0 && nr < n && nc >= 0 && nc < n) {
                        mark[nr * n + nc] = true;
                    }
                }
            }

            for (int i = 0; i < total_cells; ++i) {
                if (mark[i]) {
                    ctx.blocked_by_cell[idx].push_back(i);
                }
            }
        }
    }

    ctx.start_time = std::chrono::steady_clock::now();
    backtrack_mrv_impl(ctx);
    auto end_time = std::chrono::steady_clock::now();

    SolveResult result;
    result.variant = SolverVariant::MRV;
    result.solutions = std::move(ctx.found_solutions);
    result.stats.nodes_visited = ctx.nodes_visited;
    result.stats.budget_exhausted = ctx.budget_exhausted;
    result.stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - ctx.start_time).count();

    if (ctx.budget_exhausted) {
        if (result.solutions.empty()) {
            result.status = SolveStatus::INCONCLUSIVE;
        } else if (check_unique && result.solutions.size() == 1) {
            result.status = SolveStatus::INCONCLUSIVE;
        } else {
            result.status = SolveStatus::MULTIPLE_SOLUTIONS;
        }
    } else if (result.solutions.empty()) {
        result.status = SolveStatus::NO_SOLUTION;
    } else if (!check_unique) {
        result.status = SolveStatus::UNIQUE_SOLUTION;
    } else {
        result.status = (result.solutions.size() == 1)
                            ? SolveStatus::UNIQUE_SOLUTION
                            : SolveStatus::MULTIPLE_SOLUTIONS;
    }

    return result;
}

// ============================================================================
// 3. BITSET SOLVER: Same MRV search order using multi-word bitsets
// ============================================================================

namespace {

inline size_t count_intersection(const BitSet& a, const BitSet& b) noexcept {
    size_t total = 0;
    const size_t nw = a.num_words();
    for (size_t i = 0; i < nw; ++i) {
        total += static_cast<size_t>(std::popcount(a.words()[i] & b.words()[i]));
    }
    return total;
}

inline bool has_intersection(const BitSet& a, const BitSet& b) noexcept {
    const size_t nw = a.num_words();
    for (size_t i = 0; i < nw; ++i) {
        if ((a.words()[i] & b.words()[i]) != 0) return true;
    }
    return false;
}

struct BitsetCand {
    int cell;
    double score;
};

struct BitsetContext {
    int n;
    int max_solutions;
    size_t max_nodes{0};
    double max_time_ms{0.0};
    std::chrono::steady_clock::time_point start_time;
    bool budget_exhausted{false};
    size_t nodes_visited{0};
    bool use_ratio{false};
    double w_other{0.0};

    std::vector<BitSet> row_mask;
    std::vector<BitSet> col_mask;
    std::vector<BitSet> reg_mask;
    std::vector<BitSet> blocked_mask;
    std::vector<int> reg_of_cell;
    std::vector<size_t> init_reg_size;

    BitSet unassigned_rows;
    BitSet unassigned_cols;
    BitSet unassigned_regs;

    std::vector<Position> current_queens;
    std::vector<QueensSolution> found_solutions;
};

inline bool bitset_has_wipeout(const BitsetContext& ctx, const BitSet& test_avail) noexcept {
    for (int r = 0; r < ctx.n; ++r) {
        if (ctx.unassigned_rows.test(r) && !has_intersection(test_avail, ctx.row_mask[r])) return true;
    }
    for (int c = 0; c < ctx.n; ++c) {
        if (ctx.unassigned_cols.test(c) && !has_intersection(test_avail, ctx.col_mask[c])) return true;
    }
    for (int reg = 0; reg < ctx.n; ++reg) {
        if (ctx.unassigned_regs.test(reg) && !has_intersection(test_avail, ctx.reg_mask[reg])) return true;
    }
    return false;
}

void backtrack_bitset_impl(BitsetContext& ctx, const BitSet& available_cells) {
    ctx.nodes_visited++;

    if (ctx.max_nodes > 0 && ctx.nodes_visited >= ctx.max_nodes) {
        ctx.budget_exhausted = true;
        return;
    }
    if (ctx.max_time_ms > 0.0 && (ctx.nodes_visited & 255) == 0) {
        auto now = std::chrono::steady_clock::now();
        double cur_ms = std::chrono::duration<double, std::milli>(now - ctx.start_time).count();
        if (cur_ms >= ctx.max_time_ms) {
            ctx.budget_exhausted = true;
            return;
        }
    }

    if (static_cast<int>(ctx.found_solutions.size()) >= ctx.max_solutions) {
        return;
    }

    if (static_cast<int>(ctx.current_queens.size()) == ctx.n) {
        QueensSolution sol;
        sol.queens = ctx.current_queens;
        std::sort(sol.queens.begin(), sol.queens.end(), [](const Position& a, const Position& b) {
            return (a.row != b.row) ? (a.row < b.row) : (a.col < b.col);
        });
        ctx.found_solutions.push_back(std::move(sol));
        return;
    }

    int best_unit_type = -1; // 0 = row, 1 = col, 2 = reg
    int best_unit_idx = -1;
    size_t min_legal_cells = SIZE_MAX;
    double min_ratio = 1e9;

    // 1. Check rows
    for (int r = 0; r < ctx.n; ++r) {
        if (ctx.unassigned_rows.test(r)) {
            size_t count = count_intersection(available_cells, ctx.row_mask[r]);
            if (count == 0) {
                return; // Domain wipeout: dead end!
            }
            if (ctx.use_ratio) {
                double rat = static_cast<double>(count) / static_cast<double>(ctx.n);
                if (rat < min_ratio) {
                    min_ratio = rat;
                    best_unit_type = 0;
                    best_unit_idx = r;
                }
            } else {
                if (count < min_legal_cells) {
                    min_legal_cells = count;
                    best_unit_type = 0;
                    best_unit_idx = r;
                }
            }
        }
    }

    // 2. Check columns
    for (int c = 0; c < ctx.n; ++c) {
        if (ctx.unassigned_cols.test(c)) {
            size_t count = count_intersection(available_cells, ctx.col_mask[c]);
            if (count == 0) {
                return; // Domain wipeout: dead end!
            }
            if (ctx.use_ratio) {
                double rat = static_cast<double>(count) / static_cast<double>(ctx.n);
                if (rat < min_ratio) {
                    min_ratio = rat;
                    best_unit_type = 1;
                    best_unit_idx = c;
                }
            } else {
                if (count < min_legal_cells) {
                    min_legal_cells = count;
                    best_unit_type = 1;
                    best_unit_idx = c;
                }
            }
        }
    }

    // 3. Check regions
    for (int reg = 0; reg < ctx.n; ++reg) {
        if (ctx.unassigned_regs.test(reg)) {
            size_t count = count_intersection(available_cells, ctx.reg_mask[reg]);
            if (count == 0) {
                return; // Domain wipeout: dead end!
            }
            if (ctx.use_ratio) {
                double rat = static_cast<double>(count) / static_cast<double>(ctx.init_reg_size[reg]);
                if (rat < min_ratio) {
                    min_ratio = rat;
                    best_unit_type = 2;
                    best_unit_idx = reg;
                }
            } else {
                if (count < min_legal_cells) {
                    min_legal_cells = count;
                    best_unit_type = 2;
                    best_unit_idx = reg;
                }
            }
        }
    }

    if (best_unit_type == -1) {
        return;
    }

    const BitSet& unit_mask =
        (best_unit_type == 0) ? ctx.row_mask[best_unit_idx]
        : (best_unit_type == 1) ? ctx.col_mask[best_unit_idx]
                                : ctx.reg_mask[best_unit_idx];

    BitSet legal_candidates = available_cells & unit_mask;

    std::vector<BitsetCand> cands;

    legal_candidates.for_each_set_bit([&](int cell) {
        int r = cell / ctx.n;
        int c = cell % ctx.n;
        int reg = ctx.reg_of_cell[cell];

        ctx.unassigned_rows.reset(r);
        ctx.unassigned_cols.reset(c);
        ctx.unassigned_regs.reset(reg);

        BitSet next_available = available_cells & ~ctx.blocked_mask[cell];
        bool wo = bitset_has_wipeout(ctx, next_available);

        ctx.unassigned_rows.set(r);
        ctx.unassigned_cols.set(c);
        ctx.unassigned_regs.set(reg);

        if (!wo) {
            double raw_imp = static_cast<double>(count_intersection(available_cells, ctx.blocked_mask[cell]));
            double other_conf = 0.0;
            if (best_unit_type == 2 && ctx.w_other != 0.0) {
                for (int dr = -1; dr <= 1; ++dr) {
                    for (int dc = -1; dc <= 1; ++dc) {
                        int nr = r + dr;
                        int nc = c + dc;
                        if (nr >= 0 && nr < ctx.n && nc >= 0 && nc < ctx.n) {
                            int idx = nr * ctx.n + nc;
                            if (available_cells.test(idx)) {
                                int o_reg = ctx.reg_of_cell[idx];
                                if (o_reg != reg && ctx.unassigned_regs.test(o_reg)) {
                                    other_conf += 1.0;
                                }
                            }
                        }
                    }
                }
            }
            double score = raw_imp + ctx.w_other * other_conf;
            cands.push_back({cell, score});
        }
    });

    if (cands.empty()) {
        return;
    }

    std::sort(cands.begin(), cands.end(), [](const BitsetCand& a, const BitsetCand& b) {
        return a.score < b.score;
    });

    for (const auto& cand : cands) {
        if (ctx.budget_exhausted || static_cast<int>(ctx.found_solutions.size()) >= ctx.max_solutions) {
            return;
        }

        int cell = cand.cell;
        int r = cell / ctx.n;
        int c = cell % ctx.n;
        int reg = ctx.reg_of_cell[cell];

        ctx.current_queens.push_back(Position{r, c});
        ctx.unassigned_rows.reset(r);
        ctx.unassigned_cols.reset(c);
        ctx.unassigned_regs.reset(reg);

        BitSet next_available = available_cells & ~ctx.blocked_mask[cell];

        backtrack_bitset_impl(ctx, next_available);

        ctx.unassigned_rows.set(r);
        ctx.unassigned_cols.set(c);
        ctx.unassigned_regs.set(reg);
        ctx.current_queens.pop_back();
    }
}

} // namespace

SolveResult QueensSolver::solve_bitset(bool check_unique) {
    const int n = board_.size();
    const int total_cells = n * n;
    const int max_solutions = check_unique ? 2 : 1;

    std::vector<size_t> init_reg_size(n, 0);
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            init_reg_size[board_.get_region_index(r, c)]++;
        }
    }
    size_t min_init_reg = SIZE_MAX;
    for (int reg = 0; reg < n; ++reg) {
        if (init_reg_size[reg] < min_init_reg) min_init_reg = init_reg_size[reg];
    }
    bool use_ratio = (n >= 64 && min_init_reg >= static_cast<size_t>(n) / 2);
    double w_other = (n <= 32 || use_ratio) ? 0.0 : 1.0;

    BitsetContext ctx;
    ctx.n = n;
    ctx.max_solutions = max_solutions;
    ctx.max_nodes = options_.max_nodes;
    ctx.max_time_ms = options_.max_time_ms;
    ctx.use_ratio = use_ratio;
    ctx.w_other = w_other;
    ctx.row_mask.assign(n, BitSet(total_cells, false));
    ctx.col_mask.assign(n, BitSet(total_cells, false));
    ctx.reg_mask.assign(n, BitSet(total_cells, false));
    ctx.blocked_mask.assign(total_cells, BitSet(total_cells, false));
    ctx.reg_of_cell.resize(total_cells);
    ctx.init_reg_size = std::move(init_reg_size);

    ctx.unassigned_rows = BitSet(n, true);
    ctx.unassigned_cols = BitSet(n, true);
    ctx.unassigned_regs = BitSet(n, true);

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            int idx = r * n + c;
            int reg = board_.get_region_index(r, c);
            ctx.reg_of_cell[idx] = reg;
            ctx.row_mask[r].set(idx);
            ctx.col_mask[c].set(idx);
            ctx.reg_mask[reg].set(idx);
        }
    }

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            int idx = r * n + c;
            int reg = ctx.reg_of_cell[idx];

            BitSet mask(total_cells, false);
            mask |= ctx.row_mask[r];
            mask |= ctx.col_mask[c];
            mask |= ctx.reg_mask[reg];

            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int nr = r + dr;
                    int nc = c + dc;
                    if (nr >= 0 && nr < n && nc >= 0 && nc < n) {
                        mask.set(nr * n + nc);
                    }
                }
            }
            ctx.blocked_mask[idx] = std::move(mask);
        }
    }

    BitSet available_cells(total_cells, true);

    ctx.start_time = std::chrono::steady_clock::now();
    backtrack_bitset_impl(ctx, available_cells);
    auto end_time = std::chrono::steady_clock::now();

    SolveResult result;
    result.variant = SolverVariant::BITSET;
    result.solutions = std::move(ctx.found_solutions);
    result.stats.nodes_visited = ctx.nodes_visited;
    result.stats.budget_exhausted = ctx.budget_exhausted;
    result.stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - ctx.start_time).count();

    if (ctx.budget_exhausted) {
        if (result.solutions.empty()) {
            result.status = SolveStatus::INCONCLUSIVE;
        } else if (check_unique && result.solutions.size() == 1) {
            result.status = SolveStatus::INCONCLUSIVE;
        } else {
            result.status = SolveStatus::MULTIPLE_SOLUTIONS;
        }
    } else if (result.solutions.empty()) {
        result.status = SolveStatus::NO_SOLUTION;
    } else if (!check_unique) {
        result.status = SolveStatus::UNIQUE_SOLUTION;
    } else {
        result.status = (result.solutions.size() == 1)
                            ? SolveStatus::UNIQUE_SOLUTION
                            : SolveStatus::MULTIPLE_SOLUTIONS;
    }

    return result;
}

SolveResult solve_queens(const Board& board, bool check_unique, SolverVariant variant, SolveOptions options) {
    QueensSolver solver(board, variant);
    solver.set_options(options);
    return solver.solve(check_unique);
}

} // namespace solvedin
