#include "solvedin/patches_solver.hpp"
#include "solvedin/bitset.hpp"

#include <algorithm>
#include <chrono>
#include <vector>

namespace solvedin {

namespace {

struct CandidateShape {
    int clue_index{0};
    Position clue_pos;
    std::vector<Position> cells;
    uint64_t mask_lo{0}; // for bits 0..63
    uint64_t mask_hi{0}; // for bits 64..127
};

bool overlaps_mask(const CandidateShape& a, uint64_t m_lo, uint64_t m_hi) {
    if ((a.mask_lo & m_lo) != 0) return true;
    if ((a.mask_hi & m_hi) != 0) return true;
    return false;
}

void make_mask(const std::vector<Position>& cells, int cols, uint64_t& lo, uint64_t& hi) {
    lo = 0;
    hi = 0;
    for (const auto& c : cells) {
        int bit = c.row * cols + c.col;
        if (bit < 64) {
            lo |= (1ULL << bit);
        } else if (bit < 128) {
            hi |= (1ULL << (bit - 64));
        }
    }
}

} // namespace

PatchesSolver::PatchesSolver(const PatchesBoard& board, SolveOptions options)
    : board_(board), options_(options) {}

PatchesSolveResult PatchesSolver::solve(bool check_unique) {
    const auto start_time = std::chrono::steady_clock::now();
    PatchesSolveResult result;

    const int rows = board_.rows();
    const int cols = board_.cols();
    const int total_cells = board_.num_cells();
    const size_t num_clues = board_.num_clues();

    if (total_cells <= 0 || num_clues == 0) {
        result.status = SolveStatus::NO_SOLUTION;
        return result;
    }

    // Quick size sum check if all clues have specified sizes
    bool all_sizes_specified = true;
    int specified_sum = 0;
    for (const auto& cl : board_.clues()) {
        if (cl.has_size()) {
            specified_sum += *cl.size;
        } else {
            all_sizes_specified = false;
        }
    }
    if (all_sizes_specified && specified_sum != total_cells) {
        result.status = SolveStatus::NO_SOLUTION;
        return result;
    }

    // Minimum size calculation per clue
    std::vector<int> min_size(num_clues, 1);
    int total_min_size = 0;
    for (size_t i = 0; i < num_clues; ++i) {
        const auto& cl = board_.clues()[i];
        if (cl.has_size()) {
            min_size[i] = *cl.size;
        } else if (cl.shape == ShapeConstraint::TALL || cl.shape == ShapeConstraint::WIDE) {
            min_size[i] = 2;
        } else {
            min_size[i] = 1;
        }
        total_min_size += min_size[i];
    }

    if (total_min_size > total_cells) {
        result.status = SolveStatus::NO_SOLUTION;
        return result;
    }

    auto check_budget = [&](size_t count) -> bool {
        if (options_.max_nodes > 0 && count >= options_.max_nodes) {
            result.stats.budget_exhausted = true;
            return true;
        }
        if ((count & 1023) == 0 && options_.max_time_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double, std::milli>(now - start_time).count();
            if (elapsed >= options_.max_time_ms) {
                result.stats.budget_exhausted = true;
                return true;
            }
        }
        return false;
    };

    // 1. Enumerate legal rectangular candidate shapes for each clue
    // In LinkedIn Patches, all patches must be solid rectangles.
    // FREEFORM clues allow any rectangular aspect ratio.
    std::vector<std::vector<CandidateShape>> candidates_per_clue(num_clues);
    size_t enum_steps = 0;

    for (size_t k = 0; k < num_clues; ++k) {
        const auto& clue = board_.clues()[k];
        Position cp = clue.pos;

        // Determine size range
        int min_sz = min_size[k];
        int max_sz = total_cells - (total_min_size - min_size[k]);
        if (clue.has_size()) {
            min_sz = *clue.size;
            max_sz = *clue.size;
        }

        if (min_sz > max_sz || min_sz > total_cells) {
            result.status = SolveStatus::NO_SOLUTION;
            return result;
        }

        for (int h = 1; h <= rows; ++h) {
            for (int w = 1; w <= cols; ++w) {
                enum_steps++;
                if (check_budget(enum_steps)) goto enumeration_done;

                int sz = h * w;
                if (sz < min_sz || sz > max_sz) {
                    continue;
                }
                if (clue.has_size() && sz != *clue.size) {
                    continue;
                }

                // Check shape filter
                if (clue.shape == ShapeConstraint::SQUARE && w != h) continue;
                if (clue.shape == ShapeConstraint::TALL && h <= w) continue;
                if (clue.shape == ShapeConstraint::WIDE && w <= h) continue;
                // FREEFORM and NONE allow any rectangular aspect ratio

                // Try all placements of h x w rectangle containing (cp.row, cp.col)
                int r_start_min = std::max(0, cp.row - h + 1);
                int r_start_max = std::min(rows - h, cp.row);
                int c_start_min = std::max(0, cp.col - w + 1);
                int c_start_max = std::min(cols - w, cp.col);

                for (int r0 = r_start_min; r0 <= r_start_max; ++r0) {
                    for (int c0 = c_start_min; c0 <= c_start_max; ++c0) {
                        // Check no other clue cell inside
                        bool has_other_clue = false;
                        std::vector<Position> rect_cells;
                        rect_cells.reserve(sz);

                        for (int r = r0; r < r0 + h && !has_other_clue; ++r) {
                            for (int c = c0; c < c0 + w; ++c) {
                                if ((r != cp.row || c != cp.col) && board_.has_clue(r, c)) {
                                    has_other_clue = true;
                                    break;
                                }
                                rect_cells.push_back(Position{r, c});
                            }
                        }

                        if (!has_other_clue) {
                            CandidateShape cand;
                            cand.clue_index = static_cast<int>(k);
                            cand.clue_pos = cp;
                            cand.cells = std::move(rect_cells);
                            make_mask(cand.cells, cols, cand.mask_lo, cand.mask_hi);
                            candidates_per_clue[k].push_back(std::move(cand));
                        }
                    }
                }
            }
        }

        // If any clue has 0 legal candidate shapes, no solution exists
        if (candidates_per_clue[k].empty() && !result.stats.budget_exhausted) {
            result.status = SolveStatus::NO_SOLUTION;
            return result;
        }
    }

enumeration_done:
    if (result.stats.budget_exhausted) {
        result.status = SolveStatus::INCONCLUSIVE;
        return result;
    }

    // 2. Search for exact cover using MRV backtracking
    uint64_t covered_lo = 0;
    uint64_t covered_hi = 0;
    int covered_count = 0;
    std::vector<bool> clue_used(num_clues, false);
    // Pointers into candidates_per_clue (never resized after enumeration): placing a rectangle copies nothing.
    // Patch objects (which own a cell vector) are built only when a complete cover is found.
    std::vector<const CandidateShape*> current_patches;
    current_patches.reserve(num_clues);

    // Mask of all board cells; the cell-coverage prune is only sound when every cell has a bit (<= 128 cells).
    const bool cell_prune = total_cells <= 128;
    const uint64_t full_lo = total_cells >= 64 ? ~0ULL : ((1ULL << total_cells) - 1);
    const uint64_t full_hi = total_cells <= 64 ? 0ULL : (total_cells >= 128 ? ~0ULL : ((1ULL << (total_cells - 64)) - 1));

    auto search = [&](auto& self) -> void {
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

        if (current_patches.size() == num_clues) {
            if (covered_count == total_cells) {
                PatchesSolution sol;
                sol.patches.reserve(current_patches.size());
                for (const CandidateShape* cs : current_patches) sol.patches.push_back(Patch{cs->clue_pos, cs->cells});
                auto val = validate_patches_solution(board_, sol);
                if (val.is_valid) {
                    result.solutions.push_back(std::move(sol));
                }
            }
            return;
        }

        // Find unused clue with minimum valid candidates (MRV), and prune on cell coverage.
        //
        // Profile history (Patches #151, 8x8, 16 clues):
        //  1. ~63% of samples were malloc/free + memmove because a std::vector of compatible candidates was built for every
        //     unused clue at every node.  That was removed (counting scan, pointer stack): same nodes, ~5.7x throughput.
        //  2. After that, 99.7% of samples are inside this scan and the search still visits 1.39e9 nodes: the bottleneck is
        //     the NUMBER of nodes, not the cost per node.  The search only pruned "an unused clue has no compatible
        //     candidate".  It did not notice an uncovered CELL that no remaining candidate of any unused clue can cover,
        //     which makes the branch dead just as surely.  The scan below ORs together the masks of every compatible
        //     candidate of every unused clue and backtracks if some uncovered cell is outside that union.
        // Safety: a complete cover must put every cell in some chosen rectangle, and each chosen rectangle is a compatible
        // candidate of an unused clue, so a cell outside the union can never be covered by any completion.  The MRV choice
        // rule (first clue with the strictly smallest compatible count) and the candidate order are unchanged, so the
        // first solution found is the same; only dead branches are cut earlier.
        const uint64_t node_lo = covered_lo, node_hi = covered_hi;
        int best_clue = -1;
        size_t min_cand_count = std::numeric_limits<size_t>::max();
        uint64_t union_lo = 0, union_hi = 0;

        for (size_t k = 0; k < num_clues; ++k) {
            if (clue_used[k]) continue;

            size_t count = 0;
            for (const auto& cand : candidates_per_clue[k]) {
                if (!overlaps_mask(cand, node_lo, node_hi)) {
                    ++count;
                    union_lo |= cand.mask_lo;
                    union_hi |= cand.mask_hi;
                }
            }

            // Pruning: if an unused clue has 0 compatible candidate shapes, backtrack immediately
            if (count == 0) {
                return;
            }

            if (count < min_cand_count) {
                min_cand_count = count;
                best_clue = static_cast<int>(k);
            }
        }

        // Pruning: every uncovered cell must be coverable by some compatible candidate (only valid while all cells fit
        // in the 128-bit masks).
        if (cell_prune && (((~node_lo) & full_lo & ~union_lo) | ((~node_hi) & full_hi & ~union_hi)) != 0) {
            return;
        }

        if (best_clue == -1) {
            return;
        }

        clue_used[best_clue] = true;
        for (const auto& cand : candidates_per_clue[best_clue]) {
            // covered_* equals node_* here: every child restores them before the next iteration
            if (overlaps_mask(cand, node_lo, node_hi)) continue;

            covered_lo |= cand.mask_lo;
            covered_hi |= cand.mask_hi;
            covered_count += static_cast<int>(cand.cells.size());
            current_patches.push_back(&cand);

            self(self);

            current_patches.pop_back();
            covered_count -= static_cast<int>(cand.cells.size());
            covered_lo &= ~cand.mask_lo;
            covered_hi &= ~cand.mask_hi;

            if (result.stats.budget_exhausted) return;
            if (!check_unique && !result.solutions.empty()) return;
            if (check_unique && result.solutions.size() >= 2) return;
        }
        clue_used[best_clue] = false;
    };

    search(search);

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

PatchesSolveResult solve_patches(const PatchesBoard& board, bool check_unique, SolveOptions options) {
    PatchesSolver solver(board, options);
    return solver.solve(check_unique);
}

} // namespace solvedin
