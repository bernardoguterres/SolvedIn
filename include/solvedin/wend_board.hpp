#pragma once

#include "solvedin/common.hpp"

#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

struct WendWordPath {
    std::string word;
    std::vector<Position> path;

    [[nodiscard]] int length() const noexcept {
        return static_cast<int>(path.size());
    }

    bool operator==(const WendWordPath& other) const = default;
};

struct WendSolution {
    std::vector<WendWordPath> words;

    [[nodiscard]] std::string to_string(int rows, int cols) const;
};

struct WendParseResult;

class WendBoard {
public:
    WendBoard();
    WendBoard(int rows, int cols,
              std::vector<std::string> grid,
              std::vector<int> target_lengths);

    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int cols() const noexcept { return cols_; }
    [[nodiscard]] int num_cells() const noexcept { return rows_ * cols_; }

    [[nodiscard]] char letter_at(int r, int c) const;
    [[nodiscard]] char letter_at(Position p) const { return letter_at(p.row, p.col); }

    [[nodiscard]] bool is_in_bounds(Position p) const noexcept {
        return p.row >= 0 && p.row < rows_ && p.col >= 0 && p.col < cols_;
    }

    [[nodiscard]] bool is_void(int r, int c) const noexcept {
        return r < 0 || r >= rows_ || c < 0 || c >= cols_ || grid_[r][c] == '.';
    }

    [[nodiscard]] bool is_void(Position p) const noexcept {
        return is_void(p.row, p.col);
    }

    [[nodiscard]] int letter_count() const noexcept {
        int cnt = 0;
        for (const auto& row : grid_) {
            for (char c : row) {
                if (c != '.') cnt++;
            }
        }
        return cnt;
    }

    [[nodiscard]] const std::vector<std::string>& grid() const noexcept { return grid_; }
    [[nodiscard]] const std::vector<int>& target_lengths() const noexcept { return target_lengths_; }

    static WendParseResult load_from_file(const std::string& filepath);
    static WendParseResult parse(std::istream& in);
    static WendParseResult parse_from_string(const std::string& content);

    [[nodiscard]] std::string to_string() const;

private:
    int rows_{0};
    int cols_{0};
    std::vector<std::string> grid_;
    std::vector<int> target_lengths_;
};

struct WendParseResult {
    bool success{false};
    std::optional<WendBoard> board;
    std::string error_message;
};

} // namespace solvedin
