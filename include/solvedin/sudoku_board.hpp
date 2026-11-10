#pragma once

#include "solvedin/common.hpp"

#include <array>
#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

struct SudokuParseResult;

class SudokuBoard {
public:
    static constexpr int SIZE = 6;
    static constexpr int BOX_ROWS = 2;
    static constexpr int BOX_COLS = 3;
    static constexpr int NUM_BOXES = 6;

    SudokuBoard();
    explicit SudokuBoard(const std::array<std::array<int, SIZE>, SIZE>& grid);

    [[nodiscard]] int get(int r, int c) const;
    [[nodiscard]] bool is_given(int r, int c) const;
    [[nodiscard]] static constexpr int box_index(int r, int c) noexcept {
        return (r / BOX_ROWS) * (SIZE / BOX_COLS) + (c / BOX_COLS);
    }
    [[nodiscard]] const std::array<std::array<int, SIZE>, SIZE>& grid() const noexcept { return grid_; }
    [[nodiscard]] int givens_count() const noexcept;

    static SudokuParseResult load_from_file(const std::string& filepath);
    static SudokuParseResult parse(std::istream& in);
    static SudokuParseResult parse_from_string(const std::string& content);

    std::string to_string() const;

private:
    std::array<std::array<int, SIZE>, SIZE> grid_{};
    std::array<std::array<bool, SIZE>, SIZE> is_given_{};
};

struct SudokuParseResult {
    bool success{false};
    std::optional<SudokuBoard> board;
    std::string error_message;
};

} // namespace solvedin
