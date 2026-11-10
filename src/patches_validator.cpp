#include "solvedin/patches_validator.hpp"

#include <algorithm>
#include <queue>
#include <sstream>
#include <vector>

namespace solvedin {

std::vector<std::string> PatchesSolution::to_char_grid(int rows, int cols) const {
    std::vector<std::string> grid(rows, std::string(cols, '.'));
    static const std::string symbols =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@#$%&*";

    for (size_t i = 0; i < patches.size(); ++i) {
        char ch = symbols[i % symbols.size()];
        for (const auto& cell : patches[i].cells) {
            if (cell.row >= 0 && cell.row < rows && cell.col >= 0 && cell.col < cols) {
                grid[cell.row][cell.col] = ch;
            }
        }
    }
    return grid;
}

std::string PatchesSolution::to_string(int rows, int cols) const {
    auto grid = to_char_grid(rows, cols);
    std::ostringstream oss;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            oss << grid[r][c] << (c + 1 < cols ? " " : "");
        }
        oss << "\n";
    }
    return oss.str();
}

ValidationResult validate_patches_solution(
    const PatchesBoard& board,
    const PatchesSolution& solution) {

    const int rows = board.rows();
    const int cols = board.cols();
    const int total_cells = board.num_cells();

    // 1. Check patch count matches clue count
    if (solution.patches.size() != board.num_clues()) {
        return {false, "Solution patch count (" + std::to_string(solution.patches.size()) +
                       ") does not match board clue count (" + std::to_string(board.num_clues()) + ")"};
    }

    // 2. Check complete coverage (every cell visited exactly once)
    std::vector<std::vector<int>> cell_coverage(rows, std::vector<int>(cols, -1));
    int total_cells_covered = 0;

    for (size_t p_idx = 0; p_idx < solution.patches.size(); ++p_idx) {
        const auto& patch = solution.patches[p_idx];
        if (patch.cells.empty()) {
            return {false, "Patch " + std::to_string(p_idx) + " contains 0 cells"};
        }

        for (const auto& cell : patch.cells) {
            if (!board.is_in_bounds(cell)) {
                return {false, "Cell (" + std::to_string(cell.row) + ", " +
                               std::to_string(cell.col) + ") in patch " + std::to_string(p_idx) +
                               " is out of bounds"};
            }
            if (cell_coverage[cell.row][cell.col] != -1) {
                return {false, "Cell (" + std::to_string(cell.row) + ", " +
                               std::to_string(cell.col) + ") is covered by multiple patches (patch " +
                               std::to_string(cell_coverage[cell.row][cell.col]) + " and patch " +
                               std::to_string(p_idx) + ")"};
            }
            cell_coverage[cell.row][cell.col] = static_cast<int>(p_idx);
            total_cells_covered++;
        }
    }

    if (total_cells_covered != total_cells) {
        return {false, "Total cells covered (" + std::to_string(total_cells_covered) +
                       ") does not equal board cells (" + std::to_string(total_cells) + ")"};
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (cell_coverage[r][c] == -1) {
                return {false, "Cell (" + std::to_string(r) + ", " + std::to_string(c) +
                               ") is not covered by any patch"};
            }
        }
    }

    // 3. For each patch, validate clue, connectivity, and rectangular shape
    std::vector<bool> clue_visited(board.num_clues(), false);

    for (size_t p_idx = 0; p_idx < solution.patches.size(); ++p_idx) {
        const auto& patch = solution.patches[p_idx];

        // Clue check: exactly one clue inside this patch
        int clues_in_patch = 0;
        std::optional<PatchClue> matched_clue;

        for (const auto& cell : patch.cells) {
            auto cl = board.clue_at(cell.row, cell.col);
            if (cl.has_value()) {
                clues_in_patch++;
                matched_clue = cl;
            }
        }

        if (clues_in_patch == 0) {
            return {false, "Patch " + std::to_string(p_idx) + " contains no clues"};
        }
        if (clues_in_patch > 1) {
            return {false, "Patch " + std::to_string(p_idx) + " contains " +
                           std::to_string(clues_in_patch) + " clues (must contain exactly 1)"};
        }

        if (!(patch.clue_pos == matched_clue->pos)) {
            return {false, "Patch " + std::to_string(p_idx) + " clue position mismatch"};
        }

        // Mark clue as used
        for (size_t c_idx = 0; c_idx < board.clues().size(); ++c_idx) {
            if (board.clues()[c_idx].pos == matched_clue->pos) {
                if (clue_visited[c_idx]) {
                    return {false, "Clue at (" + std::to_string(matched_clue->pos.row) + ", " +
                                   std::to_string(matched_clue->pos.col) + ") claimed by multiple patches"};
                }
                clue_visited[c_idx] = true;
                break;
            }
        }

        // 4. Solid rectangle check:
        // In LinkedIn Patches, all patches must be solid rectangles.
        int r_min = patch.cells[0].row;
        int r_max = patch.cells[0].row;
        int c_min = patch.cells[0].col;
        int c_max = patch.cells[0].col;

        for (const auto& cell : patch.cells) {
            r_min = std::min(r_min, cell.row);
            r_max = std::max(r_max, cell.row);
            c_min = std::min(c_min, cell.col);
            c_max = std::max(c_max, cell.col);
        }

        int height = r_max - r_min + 1;
        int width = c_max - c_min + 1;

        if (static_cast<int>(patch.cells.size()) != height * width) {
            return {false, "Patch " + std::to_string(p_idx) + " is not a solid rectangle"};
        }

        // 5. Size check
        if (matched_clue->has_size()) {
            if (static_cast<int>(patch.cells.size()) != *matched_clue->size) {
                return {false, "Patch " + std::to_string(p_idx) + " size is " +
                               std::to_string(patch.cells.size()) + ", but clue requires size " +
                               std::to_string(*matched_clue->size)};
            }
        }

        // 6. Shape check:
        // SQUARE: height == width
        // TALL: height > width
        // WIDE: width > height
        // FREEFORM: any rectangular aspect ratio (height and width can be anything)
        // NONE: any rectangular aspect ratio
        if (matched_clue->has_shape()) {
            switch (matched_clue->shape) {
            case ShapeConstraint::SQUARE:
                if (width != height) {
                    return {false, "Patch " + std::to_string(p_idx) +
                                   " shape constraint violated: expected SQUARE, got height " +
                                   std::to_string(height) + " != width " + std::to_string(width)};
                }
                break;
            case ShapeConstraint::TALL:
                if (height <= width) {
                    return {false, "Patch " + std::to_string(p_idx) +
                                   " shape constraint violated: expected TALL, got height " +
                                   std::to_string(height) + " <= width " + std::to_string(width)};
                }
                break;
            case ShapeConstraint::WIDE:
                if (width <= height) {
                    return {false, "Patch " + std::to_string(p_idx) +
                                   " shape constraint violated: expected WIDE, got width " +
                                   std::to_string(width) + " <= height " + std::to_string(height)};
                }
                break;
            case ShapeConstraint::FREEFORM:
            case ShapeConstraint::NONE:
                // Any rectangular aspect ratio is permitted!
                break;
            }
        }
    }

    return {true, ""};
}

} // namespace solvedin
