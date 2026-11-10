#include "solvedin/wend_solver.hpp"

#include <chrono>
#include <map>
#include <unordered_map>

namespace solvedin {

namespace {

struct InternalCandidate {
    WendWordPath path_info;
    std::vector<int> cell_indices;
    int length{0};
};

} // namespace

WendSolver::WendSolver(const WendBoard& board, const Trie& lexicon, SolveOptions options)
    : board_(board), lexicon_(lexicon), options_(options) {}

std::vector<WendWordPath> WendSolver::enumerate_candidate_paths() {
    int rows = board_.rows();
    int cols = board_.cols();

    // Collect requested lengths
    std::unordered_map<int, int> req_lengths;
    int max_len = 0;
    for (int len : board_.target_lengths()) {
        req_lengths[len]++;
        if (len > max_len) {
            max_len = len;
        }
    }

    std::vector<WendWordPath> candidates;
    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    std::vector<Position> current_path;
    std::string current_word;

    const int dr[4] = {-1, 1, 0, 0};
    const int dc[4] = {0, 0, -1, 1};

    auto dfs = [&](auto& self, Position pos, const TrieNode* node) -> void {
        int cur_len = static_cast<int>(current_path.size());
        if (req_lengths.contains(cur_len) && node->is_word) {
            candidates.push_back(WendWordPath{current_word, current_path});
        }

        if (cur_len >= max_len) {
            return;
        }

        for (int i = 0; i < 4; ++i) {
            Position next_pos{pos.row + dr[i], pos.col + dc[i]};
            if (board_.is_in_bounds(next_pos) && !board_.is_void(next_pos) && !visited[next_pos.row][next_pos.col]) {
                char next_char = board_.letter_at(next_pos);
                const TrieNode* next_node = Trie::get_child(node, next_char);
                if (next_node != nullptr) {
                    visited[next_pos.row][next_pos.col] = true;
                    current_path.push_back(next_pos);
                    current_word.push_back(next_char);

                    self(self, next_pos, next_node);

                    current_word.pop_back();
                    current_path.pop_back();
                    visited[next_pos.row][next_pos.col] = false;
                }
            }
        }
    };

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board_.is_void(r, c)) continue;
            Position start{r, c};
            char ch = board_.letter_at(start);
            const TrieNode* node = Trie::get_child(lexicon_.root(), ch);
            if (node != nullptr) {
                visited[r][c] = true;
                current_path.push_back(start);
                current_word.push_back(ch);

                dfs(dfs, start, node);

                current_word.pop_back();
                current_path.pop_back();
                visited[r][c] = false;
            }
        }
    }

    return candidates;
}

WendSolveResult WendSolver::solve() {
    return solve(false);
}

WendSolveResult WendSolver::solve(bool check_unique) {
    auto start_time = std::chrono::steady_clock::now();
    WendSolveResult result;

    int rows = board_.rows();
    int cols = board_.cols();
    int num_cells = rows * cols;

    auto raw_candidates = enumerate_candidate_paths();

    // Build internal candidate structures
    std::vector<InternalCandidate> candidates;
    candidates.reserve(raw_candidates.size());
    for (auto& rc : raw_candidates) {
        InternalCandidate ic;
        ic.length = rc.length();
        ic.cell_indices.reserve(rc.path.size());
        for (const auto& pos : rc.path) {
            ic.cell_indices.push_back(pos.row * cols + pos.col);
        }
        ic.path_info = std::move(rc);
        candidates.push_back(std::move(ic));
    }

    // Map cell -> candidate indices that cover this cell
    std::vector<std::vector<size_t>> cell_to_candidates(num_cells);
    for (size_t i = 0; i < candidates.size(); ++i) {
        for (int c_idx : candidates[i].cell_indices) {
            cell_to_candidates[c_idx].push_back(i);
        }
    }

    // Target length counts
    std::map<int, int> remaining_lengths;
    for (int len : board_.target_lengths()) {
        remaining_lengths[len]++;
    }

    std::vector<bool> cell_covered(num_cells, false);
    int covered_count = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board_.is_void(r, c)) {
                cell_covered[r * cols + c] = true;
                covered_count++;
            }
        }
    }

    std::vector<size_t> chosen_candidates;

    auto check_timeout = [&]() -> bool {
        if (options_.max_time_ms > 0.0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(now - start_time).count();
            if (elapsed >= options_.max_time_ms) {
                result.stats.budget_exhausted = true;
                return true;
            }
        }
        return false;
    };

    auto search = [&](auto& self) -> void {
        result.stats.nodes_visited++;

        if (options_.max_nodes > 0 && result.stats.nodes_visited >= options_.max_nodes) {
            result.stats.budget_exhausted = true;
            return;
        }

        if ((result.stats.nodes_visited & 0x3FF) == 0 && check_timeout()) {
            return;
        }

        if (covered_count == num_cells) {
            // Validate all requested lengths are used
            bool all_zero = true;
            for (const auto& [len, cnt] : remaining_lengths) {
                if (cnt != 0) {
                    all_zero = false;
                    break;
                }
            }
            if (all_zero) {
                WendSolution sol;
                sol.words.reserve(chosen_candidates.size());
                for (size_t idx : chosen_candidates) {
                    sol.words.push_back(candidates[idx].path_info);
                }
                result.solutions.push_back(std::move(sol));
            }
            return;
        }

        // MRV: select uncovered cell with fewest valid candidate paths
        int best_cell = -1;
        size_t min_options = std::numeric_limits<size_t>::max();
        std::vector<size_t> best_valid_cands;

        for (int c = 0; c < num_cells; ++c) {
            if (cell_covered[c]) continue;

            std::vector<size_t> valid_cands;
            for (size_t cand_idx : cell_to_candidates[c]) {
                const auto& cand = candidates[cand_idx];
                if (remaining_lengths[cand.length] <= 0) continue;

                bool overlap = false;
                for (int cell_id : cand.cell_indices) {
                    if (cell_covered[cell_id]) {
                        overlap = true;
                        break;
                    }
                }
                if (!overlap) {
                    valid_cands.push_back(cand_idx);
                }
            }

            if (valid_cands.empty()) {
                // Dead end
                return;
            }

            if (valid_cands.size() < min_options) {
                min_options = valid_cands.size();
                best_cell = c;
                best_valid_cands = std::move(valid_cands);
                if (min_options == 1) {
                    break; // Cannot get better than 1
                }
            }
        }

        if (best_cell == -1) {
            return;
        }

        // Branch on valid candidates for best_cell
        for (size_t cand_idx : best_valid_cands) {
            const auto& cand = candidates[cand_idx];

            // Re-check overlap (in case loop state modified)
            bool overlap = false;
            for (int cell_id : cand.cell_indices) {
                if (cell_covered[cell_id]) {
                    overlap = true;
                    break;
                }
            }
            if (overlap || remaining_lengths[cand.length] <= 0) {
                continue;
            }

            // Apply
            for (int cell_id : cand.cell_indices) {
                cell_covered[cell_id] = true;
            }
            covered_count += cand.length;
            remaining_lengths[cand.length]--;
            chosen_candidates.push_back(cand_idx);

            self(self);

            // Undo
            chosen_candidates.pop_back();
            remaining_lengths[cand.length]++;
            covered_count -= cand.length;
            for (int cell_id : cand.cell_indices) {
                cell_covered[cell_id] = false;
            }

            if (result.stats.budget_exhausted) {
                return;
            }

            if (!check_unique && !result.solutions.empty()) {
                return;
            }
            if (check_unique && result.solutions.size() >= 2) {
                return;
            }
        }
    };

    search(search);

    auto end_time = std::chrono::steady_clock::now();
    result.stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (result.stats.budget_exhausted) {
        result.status = SolveStatus::INCONCLUSIVE;
    } else if (result.solutions.empty()) {
        result.status = SolveStatus::NO_SOLUTION;
    } else if (result.solutions.size() == 1) {
        result.status = SolveStatus::UNIQUE_SOLUTION;
    } else {
        result.status = SolveStatus::MULTIPLE_SOLUTIONS;
    }

    return result;
}

} // namespace solvedin
