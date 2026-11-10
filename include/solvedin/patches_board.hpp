#pragma once

#include "solvedin/common.hpp"

#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

enum class ShapeConstraint : char {
    NONE = '?',
    SQUARE = 'S',
    TALL = 'T',
    WIDE = 'W',
    FREEFORM = 'F'
};

[[nodiscard]] constexpr std::string_view to_string(ShapeConstraint s) noexcept {
    switch (s) {
    case ShapeConstraint::NONE:     return "NONE";
    case ShapeConstraint::SQUARE:   return "SQUARE";
    case ShapeConstraint::TALL:     return "TALL";
    case ShapeConstraint::WIDE:     return "WIDE";
    case ShapeConstraint::FREEFORM: return "FREEFORM";
    }
    return "UNKNOWN";
}

struct PatchClue {
    Position pos{0, 0};
    std::optional<int> size{std::nullopt};
    ShapeConstraint shape{ShapeConstraint::NONE};

    [[nodiscard]] bool has_size() const noexcept { return size.has_value(); }
    [[nodiscard]] bool has_shape() const noexcept { return shape != ShapeConstraint::NONE; }
    [[nodiscard]] std::string label() const;

    bool operator==(const PatchClue& other) const = default;
};

struct PatchesParseResult;

class PatchesBoard {
public:
    PatchesBoard();
    PatchesBoard(int rows, int cols, std::vector<PatchClue> clues);

    [[nodiscard]] int rows() const noexcept { return rows_; }
    [[nodiscard]] int cols() const noexcept { return cols_; }
    [[nodiscard]] int num_cells() const noexcept { return rows_ * cols_; }

    [[nodiscard]] const std::vector<PatchClue>& clues() const noexcept { return clues_; }
    [[nodiscard]] size_t num_clues() const noexcept { return clues_.size(); }
    [[nodiscard]] std::optional<PatchClue> clue_at(int r, int c) const;
    [[nodiscard]] bool has_clue(int r, int c) const { return clue_at(r, c).has_value(); }

    [[nodiscard]] bool is_in_bounds(Position p) const noexcept {
        return p.row >= 0 && p.row < rows_ && p.col >= 0 && p.col < cols_;
    }

    static PatchesParseResult load_from_file(const std::string& filepath);
    static PatchesParseResult parse(std::istream& in);
    static PatchesParseResult parse_from_string(const std::string& content);

    [[nodiscard]] std::string to_string() const;

private:
    int rows_{0};
    int cols_{0};
    std::vector<PatchClue> clues_;
    std::vector<std::vector<int>> clue_index_; // rows x cols, -1 if no clue, index in clues_ if clue
};

struct PatchesParseResult {
    bool success{false};
    std::optional<PatchesBoard> board;
    std::string error_message;
};

} // namespace solvedin
