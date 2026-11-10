#include "solvedin/crossclimb_validator.hpp"

#include <algorithm>
#include <unordered_set>

namespace solvedin {

int CrossclimbValidator::hamming_distance(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return -1;
    int dist = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            dist++;
        }
    }
    return dist;
}

ValidationResult CrossclimbValidator::validate(
    const CrossclimbBoard& board,
    const CrossclimbSolution& solution,
    const Trie& lexicon) {

    int K = board.num_rungs();
    int expected_total = K + 2;

    if (static_cast<int>(solution.ladder.size()) != expected_total) {
        return ValidationResult{
            false,
            "Ladder has " + std::to_string(solution.ladder.size()) +
            " words, expected " + std::to_string(expected_total) +
            " (START + " + std::to_string(K) + " rungs + END)"
        };
    }

    int W = board.word_length();
    std::unordered_set<std::string> seen_words;

    // Check individual words, length, duplicates, and lexicon
    for (size_t i = 0; i < solution.ladder.size(); ++i) {
        const auto& word = solution.ladder[i];
        if (static_cast<int>(word.size()) != W) {
            return ValidationResult{
                false,
                "Word '" + word + "' at position " + std::to_string(i) +
                " has length " + std::to_string(word.size()) + ", expected " + std::to_string(W)
            };
        }
        if (seen_words.contains(word)) {
            return ValidationResult{
                false,
                "Duplicate word in ladder: '" + word + "'"
            };
        }
        seen_words.insert(word);

        if (!lexicon.contains(word)) {
            return ValidationResult{
                false,
                "Word '" + word + "' is not present in dictionary"
            };
        }
    }

    // Check Hamming distance 1 between adjacent words
    for (size_t i = 0; i + 1 < solution.ladder.size(); ++i) {
        int dist = hamming_distance(solution.ladder[i], solution.ladder[i + 1]);
        if (dist != 1) {
            return ValidationResult{
                false,
                "Invalid ladder step between '" + solution.ladder[i] + "' and '" +
                solution.ladder[i + 1] + "': Hamming distance is " + std::to_string(dist) +
                ", expected 1"
            };
        }
    }

    // Check middle rungs are exact permutation of board rung words
    std::vector<std::string> actual_rungs(solution.ladder.begin() + 1, solution.ladder.begin() + 1 + K);
    std::vector<std::string> expected_rungs = board.rung_words();
    std::sort(actual_rungs.begin(), actual_rungs.end());
    std::sort(expected_rungs.begin(), expected_rungs.end());
    if (actual_rungs != expected_rungs) {
        return ValidationResult{
            false,
            "Middle rungs in ladder do not match the required rung words set"
        };
    }

    // Check start word constraints
    const std::string& start_word = solution.ladder.front();
    if (board.has_start_word()) {
        if (start_word != *board.start_word()) {
            return ValidationResult{
                false,
                "Start word '" + start_word + "' does not match required start '" + *board.start_word() + "'"
            };
        }
    }
    if (board.has_start_candidates()) {
        const auto& cands = board.start_candidates();
        if (std::find(cands.begin(), cands.end(), start_word) == cands.end()) {
            return ValidationResult{
                false,
                "Start word '" + start_word + "' is not among the allowed start clue candidates"
            };
        }
    }

    // Check end word constraints
    const std::string& end_word = solution.ladder.back();
    if (board.has_end_word()) {
        if (end_word != *board.end_word()) {
            return ValidationResult{
                false,
                "End word '" + end_word + "' does not match required end '" + *board.end_word() + "'"
            };
        }
    }
    if (board.has_end_candidates()) {
        const auto& cands = board.end_candidates();
        if (std::find(cands.begin(), cands.end(), end_word) == cands.end()) {
            return ValidationResult{
                false,
                "End word '" + end_word + "' is not among the allowed end clue candidates"
            };
        }
    }

    return ValidationResult{true, ""};
}

} // namespace solvedin
