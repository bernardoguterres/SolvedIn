#pragma once

// Explainable Crossclimb reconstruction (supplied-word mode): the rung words are given, unordered; the ladder is
// a path START -> every rung once -> END in which neighbours differ in exactly one letter. A hidden endpoint is a
// node linked to every rung that has a candidate (from START_CANDIDATES / END_CANDIDATES, or else the dictionary)
// one letter away. This is NOT clue solving: the player-visible clues are not used here.
// Rules on the one-letter links: corridor (a word with exactly as many possible neighbours as it needs), full word,
// no loop, no early start-to-end link; then a guess on a link. The final ladder shows the letter that changes.

#include "solvedin/crossclimb_board.hpp"
#include "solvedin/reasoning.hpp"
#include "solvedin/trie.hpp"

namespace solvedin {

[[nodiscard]] ReasoningReport reason_crossclimb(const CrossclimbBoard& board, const Trie& lexicon, const ReasoningOptions& options = {});

} // namespace solvedin
