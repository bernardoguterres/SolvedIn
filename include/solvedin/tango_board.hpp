#pragma once

#include "solvedin/common.hpp"

#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

enum class TangoSymbol : char {
    EMPTY = '.',
    SUN = 'S',
    MOON = 'M'
};

enum class EdgeClue : char {
    NONE = '.',
    SAME = '=',
    OPPOSITE = 'x'
};

struct TangoParseResult;

class TangoBoard {
public:
    static constexpr int DEFAULT_SIZE = 6;

    TangoBoard();
    explicit TangoBoard(int size,
                        std::vector<std::vector<TangoSymbol>> grid,
                        std::vector<std::vector<EdgeClue>> horizontal_clues,
                        std::vector<std::vector<EdgeClue>> vertical_clues);

    [[nodiscard]] int size() const noexcept { return size_; }
    [[nodiscard]] TangoSymbol get_symbol(int r, int c) const;
    [[nodiscard]] bool is_given(int r, int c) const;
    [[nodiscard]] EdgeClue get_horizontal_clue(int r, int c) const; // between (r, c) and (r, c+1)
    [[nodiscard]] EdgeClue get_vertical_clue(int r, int c) const;   // between (r, c) and (r+1, c)

    [[nodiscard]] const std::vector<std::vector<TangoSymbol>>& grid() const noexcept { return grid_; }
    [[nodiscard]] const std::vector<std::vector<EdgeClue>>& horizontal_clues() const noexcept { return horizontal_clues_; }
    [[nodiscard]] const std::vector<std::vector<EdgeClue>>& vertical_clues() const noexcept { return vertical_clues_; }

    static TangoParseResult load_from_file(const std::string& filepath);
    static TangoParseResult parse(std::istream& in);
    static TangoParseResult parse_from_string(const std::string& content);

    std::string to_string() const;

private:
    int size_{DEFAULT_SIZE};
    std::vector<std::vector<TangoSymbol>> grid_;
    std::vector<std::vector<EdgeClue>> horizontal_clues_; // size x (size - 1)
    std::vector<std::vector<EdgeClue>> vertical_clues_;   // (size - 1) x size
};

struct TangoParseResult {
    bool success{false};
    std::optional<TangoBoard> board;
    std::string error_message;
};

} // namespace solvedin
