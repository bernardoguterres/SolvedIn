#pragma once

// Explainable Zip: the path is a set of links between orthogonal neighbours (no wall between them).
// Each link is ON the path, OFF it, or undecided. The start (checkpoint 1) and the end (last checkpoint)
// need one link, every other cell needs two. A partial path is a set of fragments; each fragment has two
// free ends, so "where the path is" never depends on a single moving head.
//
// Rules: corridor (a cell with exactly as many usable links as it needs), full cell, premature loop,
// checkpoint order inside a fragment, start-to-end shortcut, and the only link between two areas (bridge).
// Contradictions: dead end (too few usable links) and stranded area (the usable links split the board).

#include "solvedin/reasoning.hpp"
#include "solvedin/zip_board.hpp"

#include <optional>
#include <string>
#include <vector>

namespace solvedin {

namespace zip_logic {

enum class Link : char { UNKNOWN = '?', ON = '+', OFF = '-' };

struct Edge {
    int a, b;  // cell indices (row-major)
};

struct State {
    std::vector<Edge> edges;   // fixed: neighbour pairs without a wall
    std::vector<Link> links;   // per edge
};

struct Deduction {
    std::string rule;
    std::string explanation;
    std::vector<Position> involved;
    std::vector<int> on, off;  // edge indices
    bool contradiction{false};
};

[[nodiscard]] State initial_state(const ZipBoard& board);
[[nodiscard]] std::optional<Deduction> next_deduction(const ZipBoard& board, const State& s);
void apply(State& s, const Deduction& d);
[[nodiscard]] std::string render(const ZipBoard& board, const State& s);
// The ordered path if the ON links form a complete solution candidate, else empty.
[[nodiscard]] std::vector<Position> extract_path(const ZipBoard& board, const State& s);

} // namespace zip_logic

[[nodiscard]] ReasoningReport reason_zip(const ZipBoard& board, const ReasoningOptions& options = {});

} // namespace solvedin
