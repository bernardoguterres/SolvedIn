#pragma once

// Explainable Wend. Candidates are the dictionary word paths the solver already enumerates with trie prefix
// pruning (orthogonal steps, no cell reused, lengths shown on the board; the same word along different cells
// is a different candidate). Everything below ASSUMES the supplied dictionary: a word missing from it is
// treated as impossible, so a miss shows up as "no solution", never as a wrong deduction about the board.
//
// Rules: a cell only one word path covers (scarce cell), a length with exactly as many disjoint paths left
// as the board still needs (forced length), and one-step look-ahead that removes a path if placing it would
// strand a cell, wall off an area no combination of the remaining lengths can fill, or use up a length.

#include "solvedin/reasoning.hpp"
#include "solvedin/trie.hpp"
#include "solvedin/wend_board.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

namespace wend_logic {

struct State {
    std::vector<WendWordPath> paths;          // fixed
    std::vector<std::vector<int>> path_cells;  // fixed: cell indices per path
    std::vector<char> alive;
    std::vector<int> placed;                   // path indices in placement order
    std::vector<int> owner;                    // per cell: path index or -1 (void cells: -2)
    std::map<int, int> remaining;              // length -> how many more words of that length
};

struct Deduction {
    std::string rule;
    std::string explanation;
    std::vector<Position> involved;
    std::vector<int> place, eliminate;
    bool contradiction{false};
};

// Look-ahead elimination is skipped when more than this many paths are live (bounded work).
constexpr size_t kMaxLookaheadPaths = 4000;

[[nodiscard]] State initial_state(const WendBoard& board, const Trie& lexicon);
[[nodiscard]] std::optional<Deduction> next_deduction(const WendBoard& board, const State& s);
void apply(const WendBoard& board, State& s, const Deduction& d);
[[nodiscard]] std::string render(const WendBoard& board, const State& s);

} // namespace wend_logic

[[nodiscard]] ReasoningReport reason_wend(const WendBoard& board, const Trie& lexicon, const std::string& lexicon_name,
                                          const ReasoningOptions& options = {});

} // namespace solvedin
