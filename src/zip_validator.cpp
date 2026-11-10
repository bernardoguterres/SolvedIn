#include "solvedin/zip_validator.hpp"

#include <string>
#include <vector>

namespace solvedin {

ValidationResult validate_zip_solution(
    const ZipBoard& board,
    const std::vector<Position>& path) {

    const int rows = board.rows();
    const int cols = board.cols();
    const int total_cells = board.num_cells();

    // 1. Path length check
    if (static_cast<int>(path.size()) != total_cells) {
        return {false, "Path length (" + std::to_string(path.size()) +
                       ") does not match board cell count (" + std::to_string(total_cells) + ")"};
    }

    if (total_cells == 0) {
        return {false, "Board has 0 cells"};
    }

    // 2. Start endpoint check: must be checkpoint 1
    const Position start_cp = board.checkpoint_pos(1);
    if (!(path[0] == start_cp)) {
        return {false, "Path must start at checkpoint 1 at (" +
                       std::to_string(start_cp.row) + ", " + std::to_string(start_cp.col) +
                       "), but started at (" + std::to_string(path[0].row) + ", " +
                       std::to_string(path[0].col) + ")"};
    }

    // 3. End endpoint check: must be checkpoint K
    const int max_cp = board.max_checkpoint();
    const Position end_cp = board.checkpoint_pos(max_cp);
    if (!(path.back() == end_cp)) {
        return {false, "Path must end at checkpoint " + std::to_string(max_cp) + " at (" +
                       std::to_string(end_cp.row) + ", " + std::to_string(end_cp.col) +
                       "), but ended at (" + std::to_string(path.back().row) + ", " +
                       std::to_string(path.back().col) + ")"};
    }

    // 4. Exact coverage check: each cell visited exactly once
    std::vector<std::vector<bool>> visited(rows, std::vector<bool>(cols, false));
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& p = path[i];
        if (!board.is_in_bounds(p)) {
            return {false, "Step " + std::to_string(i) + " at (" +
                           std::to_string(p.row) + ", " + std::to_string(p.col) +
                           ") is out of board bounds"};
        }
        if (visited[p.row][p.col]) {
            return {false, "Cell (" + std::to_string(p.row) + ", " + std::to_string(p.col) +
                           ") is visited more than once (duplicate at step " + std::to_string(i) + ")"};
        }
        visited[p.row][p.col] = true;
    }

    // Check all cells were visited
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (!visited[r][c]) {
                return {false, "Cell (" + std::to_string(r) + ", " + std::to_string(c) +
                               ") was not visited by the path"};
            }
        }
    }

    // 5. Orthogonal adjacency and wall checks for each consecutive step
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const auto& from = path[i];
        const auto& to = path[i + 1];

        int dr = std::abs(to.row - from.row);
        int dc = std::abs(to.col - from.col);
        if (dr + dc != 1) {
            return {false, "Invalid move from step " + std::to_string(i) + " (" +
                           std::to_string(from.row) + ", " + std::to_string(from.col) + ") to step " +
                           std::to_string(i + 1) + " (" + std::to_string(to.row) + ", " +
                           std::to_string(to.col) + "): not orthogonally adjacent"};
        }

        if (!board.can_move(from, to)) {
            return {false, "Invalid move from step " + std::to_string(i) + " (" +
                           std::to_string(from.row) + ", " + std::to_string(from.col) + ") to step " +
                           std::to_string(i + 1) + " (" + std::to_string(to.row) + ", " +
                           std::to_string(to.col) + "): blocked by a wall"};
        }
    }

    // 6. Checkpoint order check: 1..K visited in strictly increasing sequential order
    int expected_next_cp = 1;
    for (size_t i = 0; i < path.size(); ++i) {
        const auto& p = path[i];
        int cp = board.checkpoint_at(p.row, p.col);
        if (cp > 0) {
            if (cp != expected_next_cp) {
                return {false, "Checkpoint ordering violation: encountered checkpoint " +
                               std::to_string(cp) + " at step " + std::to_string(i) +
                               ", expected checkpoint " + std::to_string(expected_next_cp)};
            }
            expected_next_cp++;
        }
    }

    if (expected_next_cp != max_cp + 1) {
        return {false, "Not all checkpoints were visited: reached checkpoint " +
                       std::to_string(expected_next_cp - 1) + " of " + std::to_string(max_cp)};
    }

    return {true, ""};
}

} // namespace solvedin
