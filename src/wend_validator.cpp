#include "solvedin/wend_validator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>

namespace solvedin {

ValidationResult WendValidator::validate(
    const WendBoard& board,
    const WendSolution& solution,
    const Trie& lexicon) {

    int rows = board.rows();
    int cols = board.cols();

    // 1. Check count of words
    if (solution.words.size() != board.target_lengths().size()) {
        return ValidationResult{
            false,
            "Solution has " + std::to_string(solution.words.size()) +
            " words, expected " + std::to_string(board.target_lengths().size())
        };
    }

    // 2. Check multiset of word lengths
    std::vector<int> expected_lens = board.target_lengths();
    std::vector<int> actual_lens;
    actual_lens.reserve(solution.words.size());
    for (const auto& w : solution.words) {
        actual_lens.push_back(w.length());
    }
    std::sort(expected_lens.begin(), expected_lens.end());
    std::sort(actual_lens.begin(), actual_lens.end());
    if (expected_lens != actual_lens) {
        return ValidationResult{
            false,
            "Word lengths in solution do not match target word lengths"
        };
    }

    // 3. Check each path and word
    std::vector<std::vector<int>> visit_count(rows, std::vector<int>(cols, 0));

    for (size_t i = 0; i < solution.words.size(); ++i) {
        const auto& w = solution.words[i];
        if (w.word.empty() || w.path.empty()) {
            return ValidationResult{false, "Word " + std::to_string(i + 1) + " is empty"};
        }
        if (static_cast<int>(w.word.size()) != w.length()) {
            return ValidationResult{
                false,
                "Word length mismatch for word '" + w.word + "': string length " +
                std::to_string(w.word.size()) + " vs path length " + std::to_string(w.length())
            };
        }

        // Check dictionary
        if (!lexicon.contains(w.word)) {
            return ValidationResult{
                false,
                "Word '" + w.word + "' is not present in the dictionary"
            };
        }

        std::set<std::pair<int, int>> self_visited;
        for (size_t step = 0; step < w.path.size(); ++step) {
            Position pos = w.path[step];
            if (!board.is_in_bounds(pos) || board.is_void(pos)) {
                return ValidationResult{
                    false,
                    "Cell (" + std::to_string(pos.row) + "," + std::to_string(pos.col) +
                    ") in word '" + w.word + "' is out of board bounds or void"
                };
            }

            char expected_char = board.letter_at(pos);
            if (w.word[step] != expected_char) {
                return ValidationResult{
                    false,
                    "Letter mismatch at cell (" + std::to_string(pos.row) + "," + std::to_string(pos.col) +
                    "): board has '" + std::string(1, expected_char) + "' but word has '" +
                    std::string(1, w.word[step]) + "'"
                };
            }

            // Check self-intersection
            auto coord = std::make_pair(pos.row, pos.col);
            if (self_visited.contains(coord)) {
                return ValidationResult{
                    false,
                    "Word '" + w.word + "' self-intersects at cell (" +
                    std::to_string(pos.row) + "," + std::to_string(pos.col) + ")"
                };
            }
            self_visited.insert(coord);

            // Check orthogonal adjacency with previous step
            if (step > 0) {
                Position prev = w.path[step - 1];
                int dist = std::abs(pos.row - prev.row) + std::abs(pos.col - prev.col);
                if (dist != 1) {
                    return ValidationResult{
                        false,
                        "Non-orthogonal step between (" + std::to_string(prev.row) + "," +
                        std::to_string(prev.col) + ") and (" + std::to_string(pos.row) + "," +
                        std::to_string(pos.col) + ") in word '" + w.word + "'"
                    };
                }
            }

            visit_count[pos.row][pos.col]++;
        }
    }

    // 4. Check exact cover (every letter cell visited exactly once, void cells 0 times)
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board.is_void(r, c)) {
                if (visit_count[r][c] != 0) {
                    return ValidationResult{
                        false,
                        "Void cell (" + std::to_string(r) + "," + std::to_string(c) + ") was visited by a word"
                    };
                }
            } else {
                if (visit_count[r][c] == 0) {
                    return ValidationResult{
                        false,
                        "Cell (" + std::to_string(r) + "," + std::to_string(c) + ") is not covered by any word"
                    };
                }
                if (visit_count[r][c] > 1) {
                    return ValidationResult{
                        false,
                        "Cell (" + std::to_string(r) + "," + std::to_string(c) +
                        ") is covered multiple times (" + std::to_string(visit_count[r][c]) + ")"
                    };
                }
            }
        }
    }

    return ValidationResult{true, ""};
}

} // namespace solvedin
