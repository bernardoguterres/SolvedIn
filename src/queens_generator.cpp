#include "solvedin/queens_generator.hpp"
#include "solvedin/queens_validator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace solvedin {

GeneratedBoard QueensGenerator::generate(int size, uint64_t seed) {
    if (size <= 0) {
        throw std::invalid_argument("Board size must be positive");
    }
    if (size > 1 && size < 4) {
        throw std::invalid_argument("No valid non-touching placement exists for N=2 or N=3");
    }

    if (size == 1) {
        Board b(1, {0});
        return {b, {{0, 0}}};
    }

    std::mt19937_64 rng(seed);

    // 1. Generate valid non-touching queen placement
    std::vector<int> cols(size);
    std::iota(cols.begin(), cols.end(), 0);

    bool placement_found = false;
    for (int retry = 0; retry < 1000 && !placement_found; ++retry) {
        std::shuffle(cols.begin(), cols.end(), rng);

        // Min-conflicts local search with random perturbation
        for (int step = 0; step < 50000; ++step) {
            std::vector<int> conflicts;
            for (int r = 0; r < size - 1; ++r) {
                if (std::abs(cols[r] - cols[r + 1]) <= 1) {
                    conflicts.push_back(r);
                }
            }

            if (conflicts.empty()) {
                placement_found = true;
                break;
            }

            std::uniform_int_distribution<size_t> conf_dist(0, conflicts.size() - 1);
            int conf_r = conflicts[conf_dist(rng)];
            std::uniform_int_distribution<int> all_dist(0, size - 1);
            int swap_r = all_dist(rng);
            if (swap_r != conf_r) {
                std::swap(cols[conf_r], cols[swap_r]);
            }
        }
    }

    if (!placement_found) {
        throw std::runtime_error("Failed to generate non-touching placement for size " + std::to_string(size));
    }

    std::vector<Position> planted_queens;
    planted_queens.reserve(size);
    for (int r = 0; r < size; ++r) {
        planted_queens.push_back(Position{r, cols[r]});
    }

    // 2. Grow N connected regions from the queen cells
    const int total_cells = size * size;
    std::vector<int> grid(total_cells, -1);
    std::vector<std::vector<int>> frontiers(size);
    std::vector<int> sizes(size, 1);

    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};

    for (int r = 0; r < size; ++r) {
        int c = cols[r];
        grid[r * size + c] = r;
        for (int d = 0; d < 4; ++d) {
            int nr = r + dr[d];
            int nc = c + dc[d];
            if (nr >= 0 && nr < size && nc >= 0 && nc < size) {
                frontiers[r].push_back(nr * size + nc);
            }
        }
    }

    int unassigned_count = total_cells - size;
    while (unassigned_count > 0) {
        // Collect regions that have valid unassigned frontier cells
        std::vector<int> active_regions;
        for (int i = 0; i < size; ++i) {
            bool has_unassigned = false;
            for (int cell : frontiers[i]) {
                if (grid[cell] == -1) {
                    has_unassigned = true;
                    break;
                }
            }
            if (has_unassigned) {
                active_regions.push_back(i);
            }
        }

        if (active_regions.empty()) {
            break;
        }

        // Fair growth: prioritize regions with minimum size
        int min_sz = INT32_MAX;
        for (int reg : active_regions) {
            if (sizes[reg] < min_sz) {
                min_sz = sizes[reg];
            }
        }

        std::vector<int> min_size_regions;
        for (int reg : active_regions) {
            if (sizes[reg] == min_sz) {
                min_size_regions.push_back(reg);
            }
        }

        std::uniform_int_distribution<size_t> reg_dist(0, min_size_regions.size() - 1);
        int chosen_reg = min_size_regions[reg_dist(rng)];

        std::vector<int> unassigned_cells;
        for (int cell : frontiers[chosen_reg]) {
            if (grid[cell] == -1) {
                unassigned_cells.push_back(cell);
            }
        }

        if (unassigned_cells.empty()) {
            continue;
        }

        std::uniform_int_distribution<size_t> cell_dist(0, unassigned_cells.size() - 1);
        int chosen_cell = unassigned_cells[cell_dist(rng)];

        grid[chosen_cell] = chosen_reg;
        sizes[chosen_reg]++;
        unassigned_count--;

        int cr = chosen_cell / size;
        int cc = chosen_cell % size;
        for (int d = 0; d < 4; ++d) {
            int nr = cr + dr[d];
            int nc = cc + dc[d];
            if (nr >= 0 && nr < size && nc >= 0 && nc < size && grid[nr * size + nc] == -1) {
                frontiers[chosen_reg].push_back(nr * size + nc);
            }
        }
    }

    // Assign any residual isolated cells to an adjacent region
    while (true) {
        bool any_unassigned = false;
        bool assigned_in_this_pass = false;
        for (int r = 0; r < size; ++r) {
            for (int c = 0; c < size; ++c) {
                if (grid[r * size + c] == -1) {
                    any_unassigned = true;
                    for (int d = 0; d < 4; ++d) {
                        int nr = r + dr[d];
                        int nc = c + dc[d];
                        if (nr >= 0 && nr < size && nc >= 0 && nc < size && grid[nr * size + nc] != -1) {
                            grid[r * size + c] = grid[nr * size + nc];
                            assigned_in_this_pass = true;
                            break;
                        }
                    }
                }
            }
        }
        if (!any_unassigned || !assigned_in_this_pass) {
            break;
        }
    }

    Board board(size, std::move(grid));

    // 3. Independent verification of planted placement and region connectivity
    auto val = validate_queens_solution(board, planted_queens);
    if (!val.is_valid) {
        throw std::runtime_error("Planted placement validation failed: " + val.error_message);
    }

    if (!verify_region_connectivity(board, planted_queens)) {
        throw std::runtime_error("Region connectivity verification failed");
    }

    return {std::move(board), std::move(planted_queens)};
}

bool QueensGenerator::verify_region_connectivity(const Board& board, const std::vector<Position>& planted_queens) {
    const int n = board.size();
    if (static_cast<int>(planted_queens.size()) != n) {
        return false;
    }

    // Check each region
    for (int reg = 0; reg < n; ++reg) {
        // Collect all cells in region reg
        std::vector<int> region_cells;
        for (int r = 0; r < n; ++r) {
            for (int c = 0; c < n; ++c) {
                if (board.get_region(r, c) == reg) {
                    region_cells.push_back(r * n + c);
                }
            }
        }

        if (region_cells.empty()) {
            return false;
        }

        // Verify exactly one planted queen belongs to this region
        int queen_count = 0;
        Position queen_pos{-1, -1};
        for (const auto& q : planted_queens) {
            if (board.get_region(q.row, q.col) == reg) {
                queen_count++;
                queen_pos = q;
            }
        }
        if (queen_count != 1) {
            return false;
        }

        // BFS to check 4-connectivity
        std::unordered_set<int> visited;
        std::queue<int> q_cells;
        int seed_cell = queen_pos.row * n + queen_pos.col;
        q_cells.push(seed_cell);
        visited.insert(seed_cell);

        const int dr[4] = {-1, 1, 0, 0};
        const int dc[4] = {0, 0, -1, 1};

        while (!q_cells.empty()) {
            int curr = q_cells.front();
            q_cells.pop();
            int cr = curr / n;
            int cc = curr % n;

            for (int d = 0; d < 4; ++d) {
                int nr = cr + dr[d];
                int nc = cc + dc[d];
                if (nr >= 0 && nr < n && nc >= 0 && nc < n) {
                    int neighbor_idx = nr * n + nc;
                    if (board.get_region(nr, nc) == reg && visited.find(neighbor_idx) == visited.end()) {
                        visited.insert(neighbor_idx);
                        q_cells.push(neighbor_idx);
                    }
                }
            }
        }

        if (visited.size() != region_cells.size()) {
            return false; // Not all cells in the region were reachable from the planted queen
        }
    }

    return true;
}

} // namespace solvedin
