#pragma once

#include "solvedin/common.hpp"

#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

struct ZipParseResult;

class ZipBoard {
public:
    ZipBoard();
    ZipBoard(int rows, int cols,
             std::vector<std::vector<int>> grid,
             std::vector<std::vector<bool>> h_walls,
             std::vector<std::vector<bool>> v_walls);

    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int cols() const noexcept { return cols_; }
    [[nodiscard]] int num_cells() const noexcept { return rows_ * cols_; }
    [[nodiscard]] int max_checkpoint() const noexcept { return max_checkpoint_; }

    [[nodiscard]] int checkpoint_at(int r, int c) const;
    [[nodiscard]] bool has_checkpoint(int r, int c) const { return checkpoint_at(r, c) > 0; }
    [[nodiscard]] Position checkpoint_pos(int num) const;

    [[nodiscard]] bool has_h_wall(int r, int c) const; // wall between (r, c) and (r+1, c)
    [[nodiscard]] bool has_v_wall(int r, int c) const; // wall between (r, c) and (r, c+1)

    [[nodiscard]] bool is_in_bounds(Position p) const noexcept {
        return p.row >= 0 && p.row < rows_ && p.col >= 0 && p.col < cols_;
    }

    [[nodiscard]] bool can_move(Position from, Position to) const;

    [[nodiscard]] const std::vector<std::vector<int>>& grid() const noexcept { return grid_; }
    [[nodiscard]] const std::vector<std::vector<bool>>& h_walls() const noexcept { return h_walls_; }
    [[nodiscard]] const std::vector<std::vector<bool>>& v_walls() const noexcept { return v_walls_; }

    static ZipParseResult load_from_file(const std::string& filepath);
    static ZipParseResult parse(std::istream& in);
    static ZipParseResult parse_from_string(const std::string& content);

    [[nodiscard]] std::string to_string() const;

private:
    int rows_{0};
    int cols_{0};
    int max_checkpoint_{0};
    std::vector<std::vector<int>> grid_;        // rows x cols, 0 for empty, k for checkpoint k
    std::vector<std::vector<bool>> h_walls_;   // (rows - 1) x cols
    std::vector<std::vector<bool>> v_walls_;   // rows x (cols - 1)
    std::vector<Position> checkpoint_positions_; // 1-indexed, size max_checkpoint_ + 1
};

struct ZipParseResult {
    bool success{false};
    std::optional<ZipBoard> board;
    std::string error_message;
};

} // namespace solvedin
