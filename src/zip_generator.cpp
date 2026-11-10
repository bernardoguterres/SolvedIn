#include "solvedin/zip_generator.hpp"
#include "solvedin/zip_solver.hpp"

#include <algorithm>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace solvedin {

namespace {

static constexpr int DR[] = {-1, 1, 0, 0};
static constexpr int DC[] = {0, 0, -1, 1};

struct MoveOption {
    Position pos;
    int unvisited_neighbors{0};
    uint32_t random_rank{0};
};

bool search_hamiltonian_path(
    int rows, int cols,
    std::vector<Position>& path,
    std::vector<std::vector<bool>>& visited,
    std::mt19937_64& rng) {

    const int total_cells = rows * cols;
    if (static_cast<int>(path.size()) == total_cells) {
        return true;
    }

    Position cur = path.back();

    auto count_unvisited = [&](int r, int c) {
        int count = 0;
        for (int k = 0; k < 4; ++k) {
            int nr = r + DR[k];
            int nc = c + DC[k];
            if (nr >= 0 && nr < rows && nc >= 0 && nc < cols && !visited[nr][nc]) {
                count++;
            }
        }
        return count;
    };

    std::vector<MoveOption> options;
    for (int k = 0; k < 4; ++k) {
        int nr = cur.row + DR[k];
        int nc = cur.col + DC[k];
        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols && !visited[nr][nc]) {
            options.push_back({{nr, nc}, count_unvisited(nr, nc), static_cast<uint32_t>(rng())});
        }
    }

    // Warnsdorff's heuristic: visit neighbors with fewest onward moves first
    std::sort(options.begin(), options.end(), [](const MoveOption& a, const MoveOption& b) {
        if (a.unvisited_neighbors != b.unvisited_neighbors) {
            return a.unvisited_neighbors < b.unvisited_neighbors;
        }
        return a.random_rank < b.random_rank;
    });

    for (const auto& opt : options) {
        visited[opt.pos.row][opt.pos.col] = true;
        path.push_back(opt.pos);

        if (search_hamiltonian_path(rows, cols, path, visited, rng)) {
            return true;
        }

        path.pop_back();
        visited[opt.pos.row][opt.pos.col] = false;
    }

    return false;
}

std::vector<Position> generate_hamiltonian_walk(int rows, int cols, std::mt19937_64& rng) {
    const int total_cells = rows * cols;

    std::vector<Position> valid_starts;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            // For odd number of cells, bipartite path must start and end on majority color
            if (total_cells % 2 == 1 && (r + c) % 2 != 0) {
                continue;
            }
            valid_starts.push_back({r, c});
        }
    }
    std::shuffle(valid_starts.begin(), valid_starts.end(), rng);

    for (const auto& start : valid_starts) {
        std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
        std::vector<Position> path;
        path.reserve(total_cells);

        visited[start.row][start.col] = true;
        path.push_back(start);

        if (search_hamiltonian_path(rows, cols, path, visited, rng)) {
            return path;
        }
    }

    throw std::runtime_error("Failed to generate Hamiltonian walk on grid");
}

struct WallLocation {
    int r{0};
    int c{0};
    bool is_vertical{false};
};

} // namespace

ZipBoard ZipGenerator::generate(int rows, int cols, int num_checkpoints, uint64_t seed) {
    if (rows < 2 || cols < 2) {
        throw std::invalid_argument("Zip board must be at least 2x2");
    }

    const int total_cells = rows * cols;
    const int target_cps = std::clamp(num_checkpoints, 2, total_cells);

    std::mt19937_64 rng(seed);

    // 1. Generate a self-avoiding Hamiltonian walk visiting every cell
    const std::vector<Position> path = generate_hamiltonian_walk(rows, cols, rng);

    // 2. Assign checkpoint numbers in order along the walk
    std::vector<int> cp_indices;
    cp_indices.reserve(target_cps);
    cp_indices.push_back(0); // Checkpoint 1 at start of path
    for (int k = 1; k < target_cps - 1; ++k) {
        int idx = static_cast<int>((static_cast<int64_t>(k) * (total_cells - 1)) / (target_cps - 1));
        cp_indices.push_back(idx);
    }
    cp_indices.push_back(total_cells - 1); // Checkpoint target_cps at end of path

    std::vector<std::vector<int>> grid(rows, std::vector<int>(cols, 0));
    for (int k = 0; k < target_cps; ++k) {
        Position p = path[cp_indices[k]];
        grid[p.row][p.col] = k + 1;
    }

    // 3. Mark edges traversed by the planted Hamiltonian path
    std::vector<std::vector<bool>> path_h_edge(rows, std::vector<bool>(cols - 1, false));
    std::vector<std::vector<bool>> path_v_edge(rows - 1, std::vector<bool>(cols, false));

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        Position u = path[i];
        Position v = path[i + 1];
        if (u.row == v.row) {
            int min_c = std::min(u.col, v.col);
            path_h_edge[u.row][min_c] = true;
        } else if (u.col == v.col) {
            int min_r = std::min(u.row, v.row);
            path_v_edge[min_r][u.col] = true;
        }
    }

    // 4. Candidate walls: edges not traversed by the planted path
    std::vector<WallLocation> candidate_walls;

    // Horizontal wall at (r, c) blocks movement between (r, c) and (r+1, c)
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!path_v_edge[r][c]) {
                candidate_walls.push_back({r, c, false});
            }
        }
    }

    // Vertical wall at (r, c) blocks movement between (r, c) and (r, c+1)
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            if (!path_h_edge[r][c]) {
                candidate_walls.push_back({r, c, true});
            }
        }
    }

    std::vector<std::vector<bool>> h_walls(rows - 1, std::vector<bool>(cols, false));
    std::vector<std::vector<bool>> v_walls(rows, std::vector<bool>(cols - 1, false));

    ZipBoard initial_board(rows, cols, grid, h_walls, v_walls);
    auto initial_res = solve_zip(initial_board, true);

    // 5. If not unique, place walls to eliminate alternative paths
    std::vector<WallLocation> added_walls;
    if (initial_res.status != SolveStatus::UNIQUE_SOLUTION) {
        std::shuffle(candidate_walls.begin(), candidate_walls.end(), rng);

        for (const auto& w : candidate_walls) {
            if (w.is_vertical) {
                v_walls[w.r][w.c] = true;
            } else {
                h_walls[w.r][w.c] = true;
            }
            added_walls.push_back(w);

            ZipBoard board_with_wall(rows, cols, grid, h_walls, v_walls);
            auto res = solve_zip(board_with_wall, true);
            if (res.status == SolveStatus::UNIQUE_SOLUTION) {
                break;
            }
        }
    }

    // 6. Prune unnecessary walls to keep puzzle elegant while ensuring uniqueness
    for (auto it = added_walls.rbegin(); it != added_walls.rend(); ++it) {
        const auto& w = *it;
        if (w.is_vertical) {
            v_walls[w.r][w.c] = false;
        } else {
            h_walls[w.r][w.c] = false;
        }

        ZipBoard board_without_wall(rows, cols, grid, h_walls, v_walls);
        auto res = solve_zip(board_without_wall, true);
        if (res.status != SolveStatus::UNIQUE_SOLUTION) {
            // Wall is necessary to prevent ambiguous solutions; restore it
            if (w.is_vertical) {
                v_walls[w.r][w.c] = true;
            } else {
                h_walls[w.r][w.c] = true;
            }
        }
    }

    return ZipBoard(rows, cols, std::move(grid), std::move(h_walls), std::move(v_walls));
}

ZipBoard generate_zip(int rows, int cols, int num_checkpoints, uint64_t seed) {
    return ZipGenerator::generate(rows, cols, num_checkpoints, seed);
}

} // namespace solvedin
