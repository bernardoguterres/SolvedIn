#pragma once

// Explainable Pinpoint: category inference after each clue reveal. Everything here is a HYPOTHESIS, never a proof.
// Two generators, reported separately:
//   lexicon   the built-in (or supplied) category lexicon: which categories list the revealed clues
//   compound  open words-before/after inference from a plain word list: an affix X such that X+clue or clue+X is
//             a dictionary word for EVERY revealed clue ("FIRE" + LIGHT / HOUSE / FLY). Only closed compounds are
//             detected (FIRELIGHT, not "fire drill").
// After each reveal: the leading hypothesis, its connection to every clue, and a decision: answer, ask for the next
// clue, or (after the last clue) abstain. Archive retrieval is not part of this mode.

#include "solvedin/pinpoint.hpp"
#include "solvedin/reasoning.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace solvedin {

struct PinpointReasoningOptions {
    double lexicon_threshold{0.50};  // existing guesser's confidence threshold
    int min_clues_to_answer{2};      // never commit on a single clue
    // Answer from the compound generator only if at most this many affixes explain all clues. Default 0 = never:
    // on the 100 development puzzles every single-affix answer was wrong (26 of 26) because LinkedIn mostly uses
    // open compounds and phrases ("safety pin") that a word list cannot see, so compounds are shown, not answered.
    size_t max_compound_options{0};
};

[[nodiscard]] ReasoningReport reason_pinpoint(const std::vector<std::string>& clues, const PinpointLexicon& lexicon,
                                              const std::unordered_set<std::string>* words, const PinpointReasoningOptions& options = {});

} // namespace solvedin
