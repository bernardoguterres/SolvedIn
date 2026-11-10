#include "solvedin/zip_solver.hpp"
#include "solvedin/zip_validator.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <queue>
#include <sstream>

namespace solvedin {

std::vector<std::vector<int>> ZipSolution::to_grid(int rows, int cols) const {
    std::vector<std::vector<int>> grid(rows, std::vector<int>(cols, 0));
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& p = path[i];
        if (p.row >= 0 && p.row < rows && p.col >= 0 && p.col < cols) {
            grid[p.row][p.col] = static_cast<int>(i + 1);
        }
    }
    return grid;
}

std::string ZipSolution::to_string(int rows, int cols) const {
    auto grid = to_grid(rows, cols);
    int max_val = rows * cols;
    int width = 1;
    while (max_val >= 10) {
        width++;
        max_val /= 10;
    }

    std::ostringstream oss;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            oss << std::setw(width) << grid[r][c] << (c + 1 < cols ? " " : "");
        }
        oss << "\n";
    }
    return oss.str();
}

namespace {

struct MoveCandidate {
    Position pos;
    int degree{0};
    int cp{0};
};

} // namespace

ZipSolver::ZipSolver(const ZipBoard& board, SolveOptions options)
    : board_(board), options_(options) {}

ZipSolveResult ZipSolver::solve(bool check_unique) {
    const auto start_time = std::chrono::steady_clock::now();
    ZipSolveResult result;

    const int rows = board_.rows();
    const int cols = board_.cols();
    const int total_cells = board_.num_cells();
    const int max_cp = board_.max_checkpoint();

    if (total_cells < 2 || max_cp < 2) {
        result.status = SolveStatus::NO_SOLUTION;
        return result;
    }

    Position start_pos = board_.checkpoint_pos(1);
    Position end_pos = board_.checkpoint_pos(max_cp);
    if (!board_.is_in_bounds(start_pos) || !board_.is_in_bounds(end_pos)) {
        result.status = SolveStatus::NO_SOLUTION;
        return result;
    }

    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    std::vector<Position> path;
    path.reserve(total_cells);

    visited[start_pos.row][start_pos.col] = true;
    path.push_back(start_pos);

    // BFS scratch structures to avoid dynamic allocations during pruning
    std::vector<std::vector<int>> bfs_visited(rows, std::vector<int>(cols, 0));
    int bfs_marker = 0;
    std::vector<Position> bfs_queue;
    bfs_queue.reserve(total_cells);

    // Helper: checks if unvisited cells are cut off or next checkpoint is unreachable
    auto cuts_off_unvisited = [&](Position head, int next_cp_target, int remaining_unvisited) -> bool {
        if (remaining_unvisited <= 1) {
            return false;
        }

        bfs_marker++;
        bfs_queue.clear();

        // Start BFS from head's unvisited neighbors
        static constexpr int dr[] = {-1, 1, 0, 0};
        static constexpr int dc[] = {0, 0, -1, 1};

        Position start_bfs{-1, -1};
        for (int d = 0; d < 4; ++d) {
            Position nxt{head.row + dr[d], head.col + dc[d]};
            if (board_.is_in_bounds(nxt) && !visited[nxt.row][nxt.col] && board_.can_move(head, nxt)) {
                start_bfs = nxt;
                break;
            }
        }

        if (start_bfs.row == -1) {
            // Head has no unvisited neighbors but remaining_unvisited > 0 -> stuck!
            return true;
        }

        bfs_visited[start_bfs.row][start_bfs.col] = bfs_marker;
        bfs_queue.push_back(start_bfs);
        int head_idx = 0;
        int reachable_count = 0;

        bool target_cp_reached = false;
        Position target_cp_pos = (next_cp_target <= max_cp) ? board_.checkpoint_pos(next_cp_target) : Position{-1, -1};

        while (head_idx < static_cast<int>(bfs_queue.size())) {
            Position curr = bfs_queue[head_idx++];
            reachable_count++;

            if (curr == target_cp_pos) {
                target_cp_reached = true;
            }

            for (int d = 0; d < 4; ++d) {
                Position nxt{curr.row + dr[d], curr.col + dc[d]};
                if (board_.is_in_bounds(nxt) && !visited[nxt.row][nxt.col] &&
                    board_.can_move(curr, nxt) && bfs_visited[nxt.row][nxt.col] != bfs_marker) {
                    bfs_visited[nxt.row][nxt.col] = bfs_marker;
                    bfs_queue.push_back(nxt);
                }
            }
        }

        // If not all remaining unvisited cells were reached from start_bfs -> CUT OFF!
        if (reachable_count < remaining_unvisited) {
            return true;
        }

        // If next checkpoint is not reached -> unreachable checkpoint!
        if (next_cp_target <= max_cp && !target_cp_reached) {
            return true;
        }

        // Degree / dead-end check on unvisited cells
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (!visited[r][c]) {
                    Position u{r, c};
                    int unvisited_neighbors = 0;
                    for (int d = 0; d < 4; ++d) {
                        Position nxt{r + dr[d], c + dc[d]};
                        if (board_.is_in_bounds(nxt) && !visited[nxt.row][nxt.col] && board_.can_move(u, nxt)) {
                            unvisited_neighbors++;
                        }
                    }
                    if (board_.can_move(u, head)) {
                        unvisited_neighbors++;
                    }

                    if (u == end_pos) {
                        if (unvisited_neighbors < 1) {
                            return true; // Cannot enter destination
                        }
                    } else {
                        if (unvisited_neighbors < 2) {
                            return true; // Cannot enter and leave intermediate cell!
                        }
                    }
                }
            }
        }

        return false;
    };

    // Recursive search
    auto dfs = [&](auto& self, int next_cp) -> void {
        result.stats.nodes_visited++;

        if (options_.max_nodes > 0 && result.stats.nodes_visited >= options_.max_nodes) {
            result.stats.budget_exhausted = true;
            return;
        }

        if ((result.stats.nodes_visited & 1023) == 0 && options_.max_time_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(now - start_time).count();
            if (elapsed >= options_.max_time_ms) {
                result.stats.budget_exhausted = true;
                return;
            }
        }

        if (static_cast<int>(path.size()) == total_cells) {
            if (path.back() == end_pos && next_cp == max_cp + 1) {
                // Independently validate the solution
                auto val = validate_zip_solution(board_, path);
                if (val.is_valid) {
                    result.solutions.push_back(ZipSolution{path});
                }
            }
            return;
        }

        Position curr = path.back();
        int remaining = total_cells - static_cast<int>(path.size());

        // Pruning: check if unvisited cells are cut off
        if (cuts_off_unvisited(curr, next_cp, remaining)) {
            return;
        }

        // Generate candidate next moves
        static constexpr int dr[] = {-1, 1, 0, 0};
        static constexpr int dc[] = {0, 0, -1, 1};

        std::vector<MoveCandidate> candidates;
        candidates.reserve(4);

        for (int d = 0; d < 4; ++d) {
            Position nxt{curr.row + dr[d], curr.col + dc[d]};
            if (!board_.is_in_bounds(nxt) || visited[nxt.row][nxt.col]) {
                continue;
            }
            if (!board_.can_move(curr, nxt)) {
                continue;
            }

            int cp = board_.checkpoint_at(nxt.row, nxt.col);
            // Checkpoint ordering rule:
            // If cell has a checkpoint, it MUST be next_cp!
            if (cp > 0 && cp != next_cp) {
                continue;
            }
            // Also, cannot visit checkpoint K before all cells are filled
            if (cp == max_cp && remaining > 1) {
                continue;
            }

            // Count unvisited neighbors for Warnsdorff heuristic
            int unvisited_deg = 0;
            for (int nd = 0; nd < 4; ++nd) {
                Position n2{nxt.row + dr[nd], nxt.col + dc[nd]};
                if (board_.is_in_bounds(n2) && !visited[n2.row][n2.col] && board_.can_move(nxt, n2)) {
                    unvisited_deg++;
                }
            }

            candidates.push_back(MoveCandidate{nxt, unvisited_deg, cp});
        }

        // Sort candidates:
        // Prioritize checkpoint matches and lower degree (Warnsdorff's rule)
        std::sort(candidates.begin(), candidates.end(), [](const MoveCandidate& a, const MoveCandidate& b) {
            if (a.cp > 0 && b.cp == 0) return true;
            if (b.cp > 0 && a.cp == 0) return false;
            return a.degree < b.degree;
        });

        for (const auto& cand : candidates) {
            visited[cand.pos.row][cand.pos.col] = true;
            path.push_back(cand.pos);

            int updated_cp = (cand.cp == next_cp) ? next_cp + 1 : next_cp;
            self(self, updated_cp);

            path.pop_back();
            visited[cand.pos.row][cand.pos.col] = false;

            if (result.stats.budget_exhausted) return;
            if (!check_unique && !result.solutions.empty()) return;
            if (check_unique && result.solutions.size() >= 2) return;
        }
    };

    dfs(dfs, 2);

    auto end_time = std::chrono::steady_clock::now();
    result.stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (result.stats.budget_exhausted) {
        result.status = SolveStatus::INCONCLUSIVE;
    } else if (result.solutions.empty()) {
        result.status = SolveStatus::NO_SOLUTION;
    } else if (check_unique) {
        result.status = (result.solutions.size() == 1) ? SolveStatus::UNIQUE_SOLUTION : SolveStatus::MULTIPLE_SOLUTIONS;
    } else {
        result.status = SolveStatus::UNIQUE_SOLUTION;
    }

    return result;
}

ZipSolveResult solve_zip(const ZipBoard& board, bool check_unique, SolveOptions options) {
    ZipSolver solver(board, options);
    return solver.solve(check_unique);
}

} // namespace solvedin
