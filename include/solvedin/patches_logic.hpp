#pragma once

// Explainable Patches: every clue becomes one solid rectangle that contains no other clue; the rectangles tile
// the board. Candidates are the same rectangles the exact-cover solver enumerates (area and shape filters).
// Rules work from clues and from cells: a clue with one rectangle left; a cell every rectangle of a clue covers
// (reserved for that clue); a cell only one rectangle can cover; a cell only one clue can reach.

#include "solvedin/patches_board.hpp"
#include "solvedin/reasoning.hpp"

#include <optional>
#include <string>
#include <vector>

namespace solvedin {

namespace patches_logic {

struct Rect {
    int clue;
    int r0, c0, h, w;
    [[nodiscard]] bool covers(int r, int c) const { return r >= r0 && r < r0 + h && c >= c0 && c < c0 + w; }
};

struct State {
    std::vector<Rect> rects;      // all enumerated candidates (fixed)
    std::vector<char> alive;      // per rect: not eliminated
    std::vector<int> placed;      // per clue: rect index or -1
    std::vector<int> owner;       // per cell: clue index or -1
    bool enumeration_failed{false};
    std::string failure;
};

struct Deduction {
    std::string rule;
    std::string explanation;
    std::vector<Position> involved;
    std::vector<int> place;      // rect indices
    std::vector<int> eliminate;  // rect indices
    bool contradiction{false};
};

[[nodiscard]] State initial_state(const PatchesBoard& board);
[[nodiscard]] std::optional<Deduction> next_deduction(const PatchesBoard& board, const State& s);
void apply(const PatchesBoard& board, State& s, const Deduction& d);
[[nodiscard]] std::string render(const PatchesBoard& board, const State& s);

} // namespace patches_logic

[[nodiscard]] ReasoningReport reason_patches(const PatchesBoard& board, const ReasoningOptions& options = {});

} // namespace solvedin
