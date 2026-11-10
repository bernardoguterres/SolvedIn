#include "solvedin/sudoku_solver.hpp"
#include "solvedin/sudoku_validator.hpp"

#include <bit>
#include <chrono>
#include <stdexcept>
#include <vector>

namespace solvedin {

namespace {

constexpr int N = SudokuBoard::SIZE;
constexpr uint8_t ALL_DIGITS_MASK = 0b01111110; // Bits 1 through 6

struct PeerList {
    int count{0};
    std::array<std::pair<int, int>, 14> peers{};
};

std::array<std::array<PeerList, N>, N> build_peers() {
    std::array<std::array<PeerList, N>, N> peer_table{};
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            auto& pl = peer_table[r][c];
            int b = SudokuBoard::box_index(r, c);
            int br = (b / 2) * SudokuBoard::BOX_ROWS;
            int bc = (b % 2) * SudokuBoard::BOX_COLS;

            std::array<std::array<bool, N>, N> is_peer{};

            for (int oc = 0; oc < N; ++oc) {
                if (oc != c) is_peer[r][oc] = true;
            }
            for (int orow = 0; orow < N; ++orow) {
                if (orow != r) is_peer[orow][c] = true;
            }
            for (int dr = 0; dr < SudokuBoard::BOX_ROWS; ++dr) {
                for (int dc = 0; dc < SudokuBoard::BOX_COLS; ++dc) {
                    int pr = br + dr;
                    int pc = bc + dc;
                    if (pr != r || pc != c) {
                        is_peer[pr][pc] = true;
                    }
                }
            }

            for (int pr = 0; pr < N; ++pr) {
                for (int pc = 0; pc < N; ++pc) {
                    if (is_peer[pr][pc]) {
                        pl.peers[pl.count++] = {pr, pc};
                    }
                }
            }
        }
    }
    return peer_table;
}

const auto PEERS = build_peers();

struct SudokuContext {
    size_t max_nodes{0};
    double max_time_ms{0.0};
    int max_solutions{1};
    std::chrono::steady_clock::time_point start_time;
    size_t nodes_visited{0};
    bool budget_exhausted{false};
    std::vector<SudokuSolution> found_solutions;
};

bool propagate(std::array<std::array<int, N>, N>& grid,
               std::array<std::array<uint8_t, N>, N>& candidates) {
    bool changed = true;
    while (changed) {
        changed = false;

        // 1. Naked Singles: cells with only 1 possible candidate
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < N; ++c) {
                if (grid[r][c] == 0) {
                    int count = std::popcount(candidates[r][c]);
                    if (count == 0) return false;
                    if (count == 1) {
                        int d = std::countr_zero(candidates[r][c]);
                        grid[r][c] = d;
                        const auto& pl = PEERS[r][c];
                        for (int i = 0; i < pl.count; ++i) {
                            auto [pr, pc] = pl.peers[i];
                            candidates[pr][pc] &= static_cast<uint8_t>(~(1 << d));
                            if (candidates[pr][pc] == 0) return false;
                        }
                        changed = true;
                    }
                }
            }
        }

        // 2. Hidden Singles: digits with only 1 possible location in a unit
        // Check rows
        for (int r = 0; r < N; ++r) {
            for (int d = 1; d <= N; ++d) {
                uint8_t mask = static_cast<uint8_t>(1 << d);
                int count = 0;
                int last_c = -1;
                for (int c = 0; c < N; ++c) {
                    if (candidates[r][c] & mask) {
                        count++;
                        last_c = c;
                    }
                }
                if (count == 0) return false;
                if (count == 1 && grid[r][last_c] == 0) {
                    grid[r][last_c] = d;
                    candidates[r][last_c] = mask;
                    const auto& pl = PEERS[r][last_c];
                    for (int i = 0; i < pl.count; ++i) {
                        auto [pr, pc] = pl.peers[i];
                        candidates[pr][pc] &= static_cast<uint8_t>(~mask);
                        if (candidates[pr][pc] == 0) return false;
                    }
                    changed = true;
                }
            }
        }

        // Check columns
        for (int c = 0; c < N; ++c) {
            for (int d = 1; d <= N; ++d) {
                uint8_t mask = static_cast<uint8_t>(1 << d);
                int count = 0;
                int last_r = -1;
                for (int r = 0; r < N; ++r) {
                    if (candidates[r][c] & mask) {
                        count++;
                        last_r = r;
                    }
                }
                if (count == 0) return false;
                if (count == 1 && grid[last_r][c] == 0) {
                    grid[last_r][c] = d;
                    candidates[last_r][c] = mask;
                    const auto& pl = PEERS[last_r][c];
                    for (int i = 0; i < pl.count; ++i) {
                        auto [pr, pc] = pl.peers[i];
                        candidates[pr][pc] &= static_cast<uint8_t>(~mask);
                        if (candidates[pr][pc] == 0) return false;
                    }
                    changed = true;
                }
            }
        }

        // Check boxes
        for (int b = 0; b < SudokuBoard::NUM_BOXES; ++b) {
            int br = (b / 2) * SudokuBoard::BOX_ROWS;
            int bc = (b % 2) * SudokuBoard::BOX_COLS;
            for (int d = 1; d <= N; ++d) {
                uint8_t mask = static_cast<uint8_t>(1 << d);
                int count = 0;
                int last_r = -1;
                int last_c = -1;
                for (int dr = 0; dr < SudokuBoard::BOX_ROWS; ++dr) {
                    for (int dc = 0; dc < SudokuBoard::BOX_COLS; ++dc) {
                        int r = br + dr;
                        int c = bc + dc;
                        if (candidates[r][c] & mask) {
                            count++;
                            last_r = r;
                            last_c = c;
                        }
                    }
                }
                if (count == 0) return false;
                if (count == 1 && grid[last_r][last_c] == 0) {
                    grid[last_r][last_c] = d;
                    candidates[last_r][last_c] = mask;
                    const auto& pl = PEERS[last_r][last_c];
                    for (int i = 0; i < pl.count; ++i) {
                        auto [pr, pc] = pl.peers[i];
                        candidates[pr][pc] &= static_cast<uint8_t>(~mask);
                        if (candidates[pr][pc] == 0) return false;
                    }
                    changed = true;
                }
            }
        }
    }

    return true;
}

void backtrack_sudoku(std::array<std::array<int, N>, N> grid,
                      std::array<std::array<uint8_t, N>, N> candidates,
                      SudokuContext& ctx) {
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

    // Check if fully solved
    bool all_filled = true;
    int best_r = -1;
    int best_c = -1;
    int min_cands = N + 1;

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            if (grid[r][c] == 0) {
                all_filled = false;
                int cnt = std::popcount(candidates[r][c]);
                if (cnt < min_cands) {
                    min_cands = cnt;
                    best_r = r;
                    best_c = c;
                }
            }
        }
    }

    if (all_filled) {
        ctx.found_solutions.push_back({grid});
        return;
    }

    if (best_r == -1 || min_cands == 0) {
        return;
    }

    uint8_t cell_cands = candidates[best_r][best_c];
    for (int d = 1; d <= N; ++d) {
        if (!(cell_cands & (1 << d))) continue;

        auto next_grid = grid;
        auto next_candidates = candidates;

        next_grid[best_r][best_c] = d;
        next_candidates[best_r][best_c] = static_cast<uint8_t>(1 << d);

        const auto& pl = PEERS[best_r][best_c];
        bool valid = true;
        for (int i = 0; i < pl.count; ++i) {
            auto [pr, pc] = pl.peers[i];
            next_candidates[pr][pc] &= static_cast<uint8_t>(~(1 << d));
            if (next_candidates[pr][pc] == 0) {
                valid = false;
                break;
            }
        }

        if (valid && propagate(next_grid, next_candidates)) {
            backtrack_sudoku(next_grid, next_candidates, ctx);
        }

        if (ctx.budget_exhausted || static_cast<int>(ctx.found_solutions.size()) >= ctx.max_solutions) {
            return;
        }
    }
}

} // namespace

SudokuSolver::SudokuSolver(const SudokuBoard& board, SolveOptions options)
    : board_(board), options_(options) {}

SudokuSolveResult SudokuSolver::solve(bool check_unique) {
    SudokuContext ctx;
    ctx.max_nodes = options_.max_nodes;
    ctx.max_time_ms = options_.max_time_ms;
    ctx.max_solutions = check_unique ? 2 : 1;
    ctx.start_time = std::chrono::steady_clock::now();

    std::array<std::array<int, N>, N> grid{};
    std::array<std::array<uint8_t, N>, N> candidates{};

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            grid[r][c] = board_.get(r, c);
            if (grid[r][c] != 0) {
                candidates[r][c] = static_cast<uint8_t>(1 << grid[r][c]);
            } else {
                candidates[r][c] = ALL_DIGITS_MASK;
            }
        }
    }

    // Initial peer constraint elimination for given cells
    bool valid = true;
    for (int r = 0; r < N && valid; ++r) {
        for (int c = 0; c < N && valid; ++c) {
            int d = grid[r][c];
            if (d != 0) {
                const auto& pl = PEERS[r][c];
                for (int i = 0; i < pl.count; ++i) {
                    auto [pr, pc] = pl.peers[i];
                    candidates[pr][pc] &= static_cast<uint8_t>(~(1 << d));
                    if (candidates[pr][pc] == 0) {
                        valid = false;
                        break;
                    }
                }
            }
        }
    }

    if (valid && propagate(grid, candidates)) {
        backtrack_sudoku(grid, candidates, ctx);
    }

    auto end_time = std::chrono::steady_clock::now();
    SudokuSolveResult result;
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

SudokuSolveResult solve_sudoku(const SudokuBoard& board, bool check_unique, SolveOptions options) {
    SudokuSolver solver(board, options);
    return solver.solve(check_unique);
}

} // namespace solvedin
