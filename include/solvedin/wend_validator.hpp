#pragma once

#include "solvedin/common.hpp"
#include "solvedin/trie.hpp"
#include "solvedin/wend_board.hpp"

namespace solvedin {

class WendValidator {
public:
    [[nodiscard]] static ValidationResult validate(
        const WendBoard& board,
        const WendSolution& solution,
        const Trie& lexicon);
};

} // namespace solvedin
