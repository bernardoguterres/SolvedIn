#include "solvedin/crossclimb_solver.hpp"
#include "solvedin/crossclimb_validator.hpp"

#include <chrono>
#include <set>
#include <unordered_set>

namespace solvedin {

CrossclimbSolver::CrossclimbSolver(const CrossclimbBoard& board, const Trie& lexicon, SolveOptions options)
    : board_(board), lexicon_(lexicon), options_(options) {}

CrossclimbSolveResult CrossclimbSolver::solve() {
    return solve(true); // Default to check_unique so ambiguity is detected as required
}

CrossclimbSolveResult CrossclimbSolver::solve(bool check_unique) {
    auto start_time = std::chrono::steady_clock::now();
    CrossclimbSolveResult result;

    int W = board_.word_length();
    const auto& rungs = board_.rung_words();
    int K = static_cast<int>(rungs.size());

    std::unordered_set<std::string> rung_set(rungs.begin(), rungs.end());

    // 1. Build adjacency list among rungs
    std::vector<std::vector<int>> rung_adj(K);
    for (int i = 0; i < K; ++i) {
        for (int j = i + 1; j < K; ++j) {
            if (CrossclimbValidator::hamming_distance(rungs[i], rungs[j]) == 1) {
                rung_adj[i].push_back(j);
                rung_adj[j].push_back(i);
            }
        }
    }

    // Helper to find 1-letter neighbors in dictionary
    auto find_dict_neighbors = [&](const std::string& word) -> std::vector<std::string> {
        std::vector<std::string> nbrs;
        std::string cand = word;
        for (int pos = 0; pos < W; ++pos) {
            char orig = cand[pos];
            for (char c = 'A'; c <= 'Z'; ++c) {
                if (c == orig) continue;
                cand[pos] = c;
                if (lexicon_.contains(cand) && !rung_set.contains(cand)) {
                    nbrs.push_back(cand);
                }
            }
            cand[pos] = orig;
        }
        return nbrs;
    };

    // 2. Determine start candidates pool
    std::vector<std::string> start_pool;
    if (board_.has_start_word()) {
        const auto& sw = *board_.start_word();
        if (lexicon_.contains(sw) && !rung_set.contains(sw)) {
            start_pool.push_back(sw);
        }
    } else if (board_.has_start_candidates()) {
        for (const auto& sc : board_.start_candidates()) {
            if (lexicon_.contains(sc) && !rung_set.contains(sc)) {
                start_pool.push_back(sc);
            }
        }
    } else {
        std::set<std::string> unique_starts;
        for (const auto& rw : rungs) {
            auto nbrs = find_dict_neighbors(rw);
            for (auto& nb : nbrs) {
                unique_starts.insert(std::move(nb));
            }
        }
        start_pool.assign(unique_starts.begin(), unique_starts.end());
    }

    // 3. Determine end candidates pool
    std::vector<std::string> end_pool;
    if (board_.has_end_word()) {
        const auto& ew = *board_.end_word();
        if (lexicon_.contains(ew) && !rung_set.contains(ew)) {
            end_pool.push_back(ew);
        }
    } else if (board_.has_end_candidates()) {
        for (const auto& ec : board_.end_candidates()) {
            if (lexicon_.contains(ec) && !rung_set.contains(ec)) {
                end_pool.push_back(ec);
            }
        }
    } else {
        std::set<std::string> unique_ends;
        for (const auto& rw : rungs) {
            auto nbrs = find_dict_neighbors(rw);
            for (auto& nb : nbrs) {
                unique_ends.insert(std::move(nb));
            }
        }
        end_pool.assign(unique_ends.begin(), unique_ends.end());
    }

    if (start_pool.empty() || end_pool.empty()) {
        auto end_time = std::chrono::steady_clock::now();
        result.stats.elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        result.status = SolveStatus::NO_SOLUTION;
        return result;
    }

    // Precompute start-to-rung connections
    std::vector<std::vector<int>> start_to_rungs(start_pool.size());
    for (size_t s = 0; s < start_pool.size(); ++s) {
        for (int r = 0; r < K; ++r) {
            if (CrossclimbValidator::hamming_distance(start_pool[s], rungs[r]) == 1) {
                start_to_rungs[s].push_back(r);
            }
        }
    }

    // Precompute end-to-rung connections (by rung index)
    std::vector<std::vector<size_t>> rung_to_ends(K);
    for (size_t e = 0; e < end_pool.size(); ++e) {
        for (int r = 0; r < K; ++r) {
            if (CrossclimbValidator::hamming_distance(end_pool[e], rungs[r]) == 1) {
                rung_to_ends[r].push_back(e);
            }
        }
    }

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

    std::vector<int> path_rungs;
    path_rungs.reserve(K);

    auto dfs = [&](auto& self, size_t s_idx, int curr_rung, uint32_t visited_mask) -> void {
        result.stats.nodes_visited++;

        if (options_.max_nodes > 0 && result.stats.nodes_visited >= options_.max_nodes) {
            result.stats.budget_exhausted = true;
            return;
        }

        if ((result.stats.nodes_visited & 0x3FF) == 0 && check_timeout()) {
            return;
        }

        if (static_cast<int>(path_rungs.size()) == K) {
            // Path covers all rungs! Try matching with ends
            for (size_t e_idx : rung_to_ends[curr_rung]) {
                const auto& end_w = end_pool[e_idx];
                const auto& start_w = start_pool[s_idx];
                if (end_w != start_w) {
                    CrossclimbSolution sol;
                    sol.ladder.reserve(K + 2);
                    sol.ladder.push_back(start_w);
                    for (int r_id : path_rungs) {
                        sol.ladder.push_back(rungs[r_id]);
                    }
                    sol.ladder.push_back(end_w);
                    result.solutions.push_back(std::move(sol));

                    if (!check_unique && result.solutions.size() >= 1) {
                        return;
                    }
                    if (check_unique && result.solutions.size() >= 2) {
                        return;
                    }
                }
            }
            return;
        }

        for (int next_rung : rung_adj[curr_rung]) {
            if (!(visited_mask & (1U << next_rung))) {
                path_rungs.push_back(next_rung);
                self(self, s_idx, next_rung, visited_mask | (1U << next_rung));
                path_rungs.pop_back();

                if (result.stats.budget_exhausted) return;
                if (!check_unique && !result.solutions.empty()) return;
                if (check_unique && result.solutions.size() >= 2) return;
            }
        }
    };

    for (size_t s_idx = 0; s_idx < start_pool.size(); ++s_idx) {
        for (int r0 : start_to_rungs[s_idx]) {
            path_rungs.push_back(r0);
            dfs(dfs, s_idx, r0, (1U << r0));
            path_rungs.pop_back();

            if (result.stats.budget_exhausted) break;
            if (!check_unique && !result.solutions.empty()) break;
            if (check_unique && result.solutions.size() >= 2) break;
        }
        if (result.stats.budget_exhausted) break;
        if (!check_unique && !result.solutions.empty()) break;
        if (check_unique && result.solutions.size() >= 2) break;
    }

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
