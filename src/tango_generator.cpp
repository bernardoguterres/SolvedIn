#include "solvedin/tango_generator.hpp"
#include "solvedin/tango_solver.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace solvedin {

namespace {

constexpr int SIZE = TangoBoard::DEFAULT_SIZE; // 6

std::vector<std::string> get_valid_row_patterns() {
    std::vector<std::string> patterns;
    for (int mask = 0; mask < (1 << SIZE); ++mask) {
        if (std::popcount(static_cast<unsigned>(mask)) != SIZE / 2) {
            continue;
        }
        std::string s(SIZE, 'S');
        for (int i = 0; i < SIZE; ++i) {
            if (mask & (1 << i)) {
                s[i] = 'M';
            }
        }
        bool valid = true;
        for (int i = 0; i + 2 < SIZE; ++i) {
            if (s[i] == s[i + 1] && s[i + 1] == s[i + 2]) {
                valid = false;
                break;
            }
        }
        if (valid) {
            patterns.push_back(s);
        }
    }
    return patterns;
}

bool fill_random_tango_grid(
    int r,
    std::vector<std::string>& grid,
    const std::vector<std::string>& valid_rows,
    std::mt19937_64& rng) {

    if (r == SIZE) {
        return true;
    }

    auto candidate_rows = valid_rows;
    std::shuffle(candidate_rows.begin(), candidate_rows.end(), rng);

    for (const auto& row : candidate_rows) {
        bool ok = true;
        for (int c = 0; c < SIZE; ++c) {
            int count_s = (row[c] == 'S');
            int count_m = (row[c] == 'M');
            for (int pr = 0; pr < r; ++pr) {
                if (grid[pr][c] == 'S') {
                    count_s++;
                } else {
                    count_m++;
                }
            }
            if (count_s > SIZE / 2 || count_m > SIZE / 2) {
                ok = false;
                break;
            }
            if (r >= 2 && grid[r - 1][c] == row[c] && grid[r - 2][c] == row[c]) {
                ok = false;
                break;
            }
        }

        if (ok) {
            grid[r] = row;
            if (fill_random_tango_grid(r + 1, grid, valid_rows, rng)) {
                return true;
            }
        }
    }

    return false;
}

struct EdgeInfo {
    int r{0};
    int c{0};
    bool is_vertical{false};
    EdgeClue clue{EdgeClue::NONE};
};

} // namespace

TangoBoard TangoGenerator::generate(uint64_t seed, int given_count, int edge_clue_count) {
    std::mt19937_64 rng(seed);

    const auto valid_rows = get_valid_row_patterns();
    std::vector<std::string> grid(SIZE);
    fill_random_tango_grid(0, grid, valid_rows, rng);

    // Compute ground-truth edge clues for the solved grid
    std::vector<EdgeInfo> all_edges;
    all_edges.reserve(SIZE * (SIZE - 1) * 2);

    // Horizontal edges
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE - 1; ++c) {
            EdgeClue cl = (grid[r][c] == grid[r][c + 1]) ? EdgeClue::SAME : EdgeClue::OPPOSITE;
            all_edges.push_back({r, c, false, cl});
        }
    }

    // Vertical edges
    for (int r = 0; r < SIZE - 1; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            EdgeClue cl = (grid[r][c] == grid[r + 1][c]) ? EdgeClue::SAME : EdgeClue::OPPOSITE;
            all_edges.push_back({r, c, true, cl});
        }
    }

    int target_givens = std::clamp(given_count, 0, SIZE * SIZE);
    int target_edges = std::clamp(edge_clue_count, 0, static_cast<int>(all_edges.size()));

    std::vector<std::pair<int, int>> all_cells;
    all_cells.reserve(SIZE * SIZE);
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            all_cells.push_back({r, c});
        }
    }

    std::optional<TangoBoard> best_board;
    int best_givens = SIZE * SIZE + 1;

    constexpr int MAX_ATTEMPTS = 50;
    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        std::shuffle(all_edges.begin(), all_edges.end(), rng);

        std::vector<std::vector<EdgeClue>> h_clues(SIZE, std::vector<EdgeClue>(SIZE - 1, EdgeClue::NONE));
        std::vector<std::vector<EdgeClue>> v_clues(SIZE - 1, std::vector<EdgeClue>(SIZE, EdgeClue::NONE));

        for (int i = 0; i < target_edges; ++i) {
            if (all_edges[i].is_vertical) {
                v_clues[all_edges[i].r][all_edges[i].c] = all_edges[i].clue;
            } else {
                h_clues[all_edges[i].r][all_edges[i].c] = all_edges[i].clue;
            }
        }

        std::vector<std::vector<TangoSymbol>> sym_grid(SIZE, std::vector<TangoSymbol>(SIZE));
        for (int r = 0; r < SIZE; ++r) {
            for (int c = 0; c < SIZE; ++c) {
                sym_grid[r][c] = (grid[r][c] == 'S') ? TangoSymbol::SUN : TangoSymbol::MOON;
            }
        }

        auto cell_order = all_cells;
        std::shuffle(cell_order.begin(), cell_order.end(), rng);

        int current_givens = SIZE * SIZE;
        for (const auto& [r, c] : cell_order) {
            if (current_givens <= target_givens) {
                break;
            }

            auto original_sym = sym_grid[r][c];
            sym_grid[r][c] = TangoSymbol::EMPTY;

            TangoBoard candidate_board(SIZE, sym_grid, h_clues, v_clues);
            auto solve_res = solve_tango(candidate_board, true); // check_unique = true
            if (solve_res.status == SolveStatus::UNIQUE_SOLUTION) {
                current_givens--;
            } else {
                sym_grid[r][c] = original_sym;
            }
        }

        TangoBoard candidate(SIZE, sym_grid, h_clues, v_clues);
        if (current_givens <= target_givens) {
            return candidate;
        }

        if (current_givens < best_givens) {
            best_givens = current_givens;
            best_board = candidate;
        }
    }

    if (best_board.has_value()) {
        return *best_board;
    }

    // Fallback: full grid with requested edge clues (always unique)
    std::vector<std::vector<EdgeClue>> fallback_h(SIZE, std::vector<EdgeClue>(SIZE - 1, EdgeClue::NONE));
    std::vector<std::vector<EdgeClue>> fallback_v(SIZE - 1, std::vector<EdgeClue>(SIZE, EdgeClue::NONE));
    for (int i = 0; i < target_edges; ++i) {
        if (all_edges[i].is_vertical) {
            fallback_v[all_edges[i].r][all_edges[i].c] = all_edges[i].clue;
        } else {
            fallback_h[all_edges[i].r][all_edges[i].c] = all_edges[i].clue;
        }
    }
    std::vector<std::vector<TangoSymbol>> full_sym(SIZE, std::vector<TangoSymbol>(SIZE));
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            full_sym[r][c] = (grid[r][c] == 'S') ? TangoSymbol::SUN : TangoSymbol::MOON;
        }
    }
    return TangoBoard(SIZE, std::move(full_sym), std::move(fallback_h), std::move(fallback_v));
}

TangoBoard generate_tango(uint64_t seed, int given_count, int edge_clue_count) {
    return TangoGenerator::generate(seed, given_count, edge_clue_count);
}

} // namespace solvedin
