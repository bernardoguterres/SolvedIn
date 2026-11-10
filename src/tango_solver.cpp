#include "solvedin/tango_solver.hpp"
#include "solvedin/tango_validator.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace solvedin {

namespace {

constexpr uint8_t MASK_S = 1; // 0b01
constexpr uint8_t MASK_M = 2; // 0b10
constexpr uint8_t MASK_BOTH = 3; // 0b11

struct TangoContext {
    size_t max_nodes{0};
    double max_time_ms{0.0};
    int max_solutions{1};
    std::chrono::steady_clock::time_point start_time;
    size_t nodes_visited{0};
    bool budget_exhausted{false};
    std::vector<TangoSolution> found_solutions;
};

bool propagate_tango(const TangoBoard& board, std::vector<std::vector<uint8_t>>& domains) {
    const int n = board.size();
    const int target_count = n / 2;

    bool changed = true;
    while (changed) {
        changed = false;

        // 1. Edge clues
        // Horizontal clues
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n - 1; ++c) {
                EdgeClue clue = board.get_horizontal_clue(r, c);
                if (clue == EdgeClue::SAME) {
                    uint8_t common = domains[r][c] & domains[r][c + 1];
                    if (common == 0) return false;
                    if (domains[r][c] != common) { domains[r][c] = common; changed = true; }
                    if (domains[r][c + 1] != common) { domains[r][c + 1] = common; changed = true; }
                } else if (clue == EdgeClue::OPPOSITE) {
                    if (domains[r][c] == MASK_S) {
                        if ((domains[r][c + 1] & MASK_M) == 0) return false;
                        if (domains[r][c + 1] != MASK_M) { domains[r][c + 1] = MASK_M; changed = true; }
                    } else if (domains[r][c] == MASK_M) {
                        if ((domains[r][c + 1] & MASK_S) == 0) return false;
                        if (domains[r][c + 1] != MASK_S) { domains[r][c + 1] = MASK_S; changed = true; }
                    }
                    if (domains[r][c + 1] == MASK_S) {
                        if ((domains[r][c] & MASK_M) == 0) return false;
                        if (domains[r][c] != MASK_M) { domains[r][c] = MASK_M; changed = true; }
                    } else if (domains[r][c + 1] == MASK_M) {
                        if ((domains[r][c] & MASK_S) == 0) return false;
                        if (domains[r][c] != MASK_S) { domains[r][c] = MASK_S; changed = true; }
                    }
                }
            }
        }

        // Vertical clues
        for (int r = 0; r < n - 1; ++r) {
            for (int c = 0; c < n; ++c) {
                EdgeClue clue = board.get_vertical_clue(r, c);
                if (clue == EdgeClue::SAME) {
                    uint8_t common = domains[r][c] & domains[r + 1][c];
                    if (common == 0) return false;
                    if (domains[r][c] != common) { domains[r][c] = common; changed = true; }
                    if (domains[r + 1][c] != common) { domains[r + 1][c] = common; changed = true; }
                } else if (clue == EdgeClue::OPPOSITE) {
                    if (domains[r][c] == MASK_S) {
                        if ((domains[r + 1][c] & MASK_M) == 0) return false;
                        if (domains[r + 1][c] != MASK_M) { domains[r + 1][c] = MASK_M; changed = true; }
                    } else if (domains[r][c] == MASK_M) {
                        if ((domains[r + 1][c] & MASK_S) == 0) return false;
                        if (domains[r + 1][c] != MASK_S) { domains[r + 1][c] = MASK_S; changed = true; }
                    }
                    if (domains[r + 1][c] == MASK_S) {
                        if ((domains[r][c] & MASK_M) == 0) return false;
                        if (domains[r][c] != MASK_M) { domains[r][c] = MASK_M; changed = true; }
                    } else if (domains[r + 1][c] == MASK_M) {
                        if ((domains[r][c] & MASK_S) == 0) return false;
                        if (domains[r][c] != MASK_S) { domains[r][c] = MASK_S; changed = true; }
                    }
                }
            }
        }

        // 2. Row counts
        for (int r = 0; r < n; ++r) {
            int s_count = 0, m_count = 0;
            for (int c = 0; c < n; ++c) {
                if (domains[r][c] == MASK_S) ++s_count;
                if (domains[r][c] == MASK_M) ++m_count;
            }
            if (s_count > target_count || m_count > target_count) return false;
            if (s_count == target_count) {
                for (int c = 0; c < n; ++c) {
                    if (domains[r][c] == MASK_BOTH) {
                        domains[r][c] = MASK_M;
                        changed = true;
                    }
                }
            }
            if (m_count == target_count) {
                for (int c = 0; c < n; ++c) {
                    if (domains[r][c] == MASK_BOTH) {
                        domains[r][c] = MASK_S;
                        changed = true;
                    }
                }
            }
        }

        // Column counts
        for (int c = 0; c < n; ++c) {
            int s_count = 0, m_count = 0;
            for (int r = 0; r < n; ++r) {
                if (domains[r][c] == MASK_S) ++s_count;
                if (domains[r][c] == MASK_M) ++m_count;
            }
            if (s_count > target_count || m_count > target_count) return false;
            if (s_count == target_count) {
                for (int r = 0; r < n; ++r) {
                    if (domains[r][c] == MASK_BOTH) {
                        domains[r][c] = MASK_M;
                        changed = true;
                    }
                }
            }
            if (m_count == target_count) {
                for (int r = 0; r < n; ++r) {
                    if (domains[r][c] == MASK_BOTH) {
                        domains[r][c] = MASK_S;
                        changed = true;
                    }
                }
            }
        }

        // 3. No 3 consecutive identical in rows
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n - 2; ++c) {
                // Check for S
                if (domains[r][c] == MASK_S && domains[r][c + 1] == MASK_S) {
                    if ((domains[r][c + 2] & MASK_M) == 0) return false;
                    if (domains[r][c + 2] != MASK_M) { domains[r][c + 2] = MASK_M; changed = true; }
                }
                if (domains[r][c + 1] == MASK_S && domains[r][c + 2] == MASK_S) {
                    if ((domains[r][c] & MASK_M) == 0) return false;
                    if (domains[r][c] != MASK_M) { domains[r][c] = MASK_M; changed = true; }
                }
                if (domains[r][c] == MASK_S && domains[r][c + 2] == MASK_S) {
                    if ((domains[r][c + 1] & MASK_M) == 0) return false;
                    if (domains[r][c + 1] != MASK_M) { domains[r][c + 1] = MASK_M; changed = true; }
                }

                // Check for M
                if (domains[r][c] == MASK_M && domains[r][c + 1] == MASK_M) {
                    if ((domains[r][c + 2] & MASK_S) == 0) return false;
                    if (domains[r][c + 2] != MASK_S) { domains[r][c + 2] = MASK_S; changed = true; }
                }
                if (domains[r][c + 1] == MASK_M && domains[r][c + 2] == MASK_M) {
                    if ((domains[r][c] & MASK_S) == 0) return false;
                    if (domains[r][c] != MASK_S) { domains[r][c] = MASK_S; changed = true; }
                }
                if (domains[r][c] == MASK_M && domains[r][c + 2] == MASK_M) {
                    if ((domains[r][c + 1] & MASK_S) == 0) return false;
                    if (domains[r][c + 1] != MASK_S) { domains[r][c + 1] = MASK_S; changed = true; }
                }
            }
        }

        // No 3 consecutive identical in columns
        for (int c = 0; c < n; ++c) {
            for (int r = 0; r < n - 2; ++r) {
                // Check for S
                if (domains[r][c] == MASK_S && domains[r + 1][c] == MASK_S) {
                    if ((domains[r + 2][c] & MASK_M) == 0) return false;
                    if (domains[r + 2][c] != MASK_M) { domains[r + 2][c] = MASK_M; changed = true; }
                }
                if (domains[r + 1][c] == MASK_S && domains[r + 2][c] == MASK_S) {
                    if ((domains[r][c] & MASK_M) == 0) return false;
                    if (domains[r][c] != MASK_M) { domains[r][c] = MASK_M; changed = true; }
                }
                if (domains[r][c] == MASK_S && domains[r + 2][c] == MASK_S) {
                    if ((domains[r + 1][c] & MASK_M) == 0) return false;
                    if (domains[r + 1][c] != MASK_M) { domains[r + 1][c] = MASK_M; changed = true; }
                }

                // Check for M
                if (domains[r][c] == MASK_M && domains[r + 1][c] == MASK_M) {
                    if ((domains[r + 2][c] & MASK_S) == 0) return false;
                    if (domains[r + 2][c] != MASK_S) { domains[r + 2][c] = MASK_S; changed = true; }
                }
                if (domains[r + 1][c] == MASK_M && domains[r + 2][c] == MASK_M) {
                    if ((domains[r][c] & MASK_S) == 0) return false;
                    if (domains[r][c] != MASK_S) { domains[r][c] = MASK_S; changed = true; }
                }
                if (domains[r][c] == MASK_M && domains[r + 2][c] == MASK_M) {
                    if ((domains[r + 1][c] & MASK_S) == 0) return false;
                    if (domains[r + 1][c] != MASK_S) { domains[r + 1][c] = MASK_S; changed = true; }
                }
            }
        }
    }

    return true;
}

void backtrack_tango(const TangoBoard& board,
                     std::vector<std::vector<uint8_t>> domains,
                     TangoContext& ctx) {
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

    const int n = board.size();
    int pick_r = -1, pick_c = -1;

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            if (domains[r][c] == MASK_BOTH) {
                pick_r = r;
                pick_c = c;
                break;
            }
        }
        if (pick_r != -1) break;
    }

    if (pick_r == -1) {
        // All cells are fixed, extract solution
        std::vector<std::string> sol_grid(n, std::string(n, '.'));
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) {
                sol_grid[r][c] = (domains[r][c] == MASK_S) ? 'S' : 'M';
            }
        }
        auto val = validate_tango_solution(board, sol_grid);
        if (val.is_valid) {
            ctx.found_solutions.push_back({std::move(sol_grid)});
        }
        return;
    }

    // Try S then M
    for (uint8_t branch_sym : {MASK_S, MASK_M}) {
        auto next_domains = domains;
        next_domains[pick_r][pick_c] = branch_sym;

        if (propagate_tango(board, next_domains)) {
            backtrack_tango(board, next_domains, ctx);
        }

        if (ctx.budget_exhausted || static_cast<int>(ctx.found_solutions.size()) >= ctx.max_solutions) {
            return;
        }
    }
}

} // namespace

TangoSolver::TangoSolver(const TangoBoard& board, SolveOptions options)
    : board_(board), options_(options) {}

TangoSolveResult TangoSolver::solve(bool check_unique) {
    const int n = board_.size();
    TangoContext ctx;
    ctx.max_nodes = options_.max_nodes;
    ctx.max_time_ms = options_.max_time_ms;
    ctx.max_solutions = check_unique ? 2 : 1;
    ctx.start_time = std::chrono::steady_clock::now();

    std::vector<std::vector<uint8_t>> domains(n, std::vector<uint8_t>(n, MASK_BOTH));

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            TangoSymbol s = board_.get_symbol(r, c);
            if (s == TangoSymbol::SUN) {
                domains[r][c] = MASK_S;
            } else if (s == TangoSymbol::MOON) {
                domains[r][c] = MASK_M;
            } else {
                domains[r][c] = MASK_BOTH;
            }
        }
    }

    if (propagate_tango(board_, domains)) {
        backtrack_tango(board_, domains, ctx);
    }

    auto end_time = std::chrono::steady_clock::now();
    TangoSolveResult result;
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

TangoSolveResult solve_tango(const TangoBoard& board, bool check_unique, SolveOptions options) {
    TangoSolver solver(board, options);
    return solver.solve(check_unique);
}

} // namespace solvedin
