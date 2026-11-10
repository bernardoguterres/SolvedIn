#pragma once

#include "solvedin/common.hpp"
#include "solvedin/crossclimb_board.hpp"
#include "solvedin/trie.hpp"

namespace solvedin {

class CrossclimbValidator {
public:
    [[nodiscard]] static ValidationResult validate(
        const CrossclimbBoard& board,
        const CrossclimbSolution& solution,
        const Trie& lexicon);

    [[nodiscard]] static int hamming_distance(std::string_view a, std::string_view b) noexcept;
};

} // namespace solvedin
