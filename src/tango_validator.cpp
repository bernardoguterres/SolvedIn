#include "solvedin/tango_validator.hpp"

#include <string>

namespace solvedin {

ValidationResult validate_tango_solution(
    const TangoBoard& initial,
    const std::vector<std::string>& solution) {

    const int n = initial.size();
    if (static_cast<int>(solution.size()) != n) {
        return {false, "Invalid solution row count: expected " + std::to_string(n) +
                       ", got " + std::to_string(solution.size())};
    }

    const int target_count = n / 2;

    // 1. Basic format and preservation of givens
    for (int r = 0; r < n; ++r) {
        if (static_cast<int>(solution[r].size()) != n) {
            return {false, "Row " + std::to_string(r + 1) + " length mismatch: expected " +
                           std::to_string(n) + ", got " + std::to_string(solution[r].size())};
        }
        for (int c = 0; c < n; ++c) {
            char sym = solution[r][c];
            if (sym != 'S' && sym != 'M') {
                return {false, "Invalid cell character '" + std::string(1, sym) +
                               "' at (" + std::to_string(r) + ", " + std::to_string(c) + ")"};
            }

            if (initial.is_given(r, c)) {
                char given_char = (initial.get_symbol(r, c) == TangoSymbol::SUN) ? 'S' : 'M';
                if (sym != given_char) {
                    return {false, "Given symbol '" + std::string(1, given_char) +
                                   "' at (" + std::to_string(r) + ", " + std::to_string(c) +
                                   ") was altered to '" + std::string(1, sym) + "'"};
                }
            }
        }
    }

    // 2. Validate horizontal edge clues
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n - 1; ++c) {
            EdgeClue clue = initial.get_horizontal_clue(r, c);
            if (clue == EdgeClue::SAME && solution[r][c] != solution[r][c + 1]) {
                return {false, "Horizontal clue '=' violated between (" + std::to_string(r) + "," +
                               std::to_string(c) + ") and (" + std::to_string(r) + "," +
                               std::to_string(c + 1) + "): '" + solution[r][c] + "' != '" + solution[r][c + 1] + "'"};
            }
            if (clue == EdgeClue::OPPOSITE && solution[r][c] == solution[r][c + 1]) {
                return {false, "Horizontal clue 'x' violated between (" + std::to_string(r) + "," +
                               std::to_string(c) + ") and (" + std::to_string(r) + "," +
                               std::to_string(c + 1) + "): both are '" + solution[r][c] + "'"};
            }
        }
    }

    // 3. Validate vertical edge clues
    for (int r = 0; r < n - 1; ++r) {
        for (int c = 0; c < n; ++c) {
            EdgeClue clue = initial.get_vertical_clue(r, c);
            if (clue == EdgeClue::SAME && solution[r][c] != solution[r + 1][c]) {
                return {false, "Vertical clue '=' violated between (" + std::to_string(r) + "," +
                               std::to_string(c) + ") and (" + std::to_string(r + 1) + "," +
                               std::to_string(c) + "): '" + solution[r][c] + "' != '" + solution[r + 1][c] + "'"};
            }
            if (clue == EdgeClue::OPPOSITE && solution[r][c] == solution[r + 1][c]) {
                return {false, "Vertical clue 'x' violated between (" + std::to_string(r) + "," +
                               std::to_string(c) + ") and (" + std::to_string(r + 1) + "," +
                               std::to_string(c) + "): both are '" + solution[r][c] + "'"};
            }
        }
    }

    // 4. Equal counts and no 3 consecutive identical in each row
    for (int r = 0; r < n; ++r) {
        int sun_cnt = 0, moon_cnt = 0;
        for (int c = 0; c < n; ++c) {
            if (solution[r][c] == 'S') ++sun_cnt;
            if (solution[r][c] == 'M') ++moon_cnt;

            if (c >= 2 && solution[r][c] == solution[r][c - 1] && solution[r][c - 1] == solution[r][c - 2]) {
                return {false, "Row " + std::to_string(r + 1) + " contains 3 consecutive '" +
                               std::string(1, solution[r][c]) + "' at columns " +
                               std::to_string(c - 1) + "-" + std::to_string(c + 1)};
            }
        }
        if (sun_cnt != target_count || moon_cnt != target_count) {
            return {false, "Row " + std::to_string(r + 1) + " does not have equal symbol counts (found " +
                           std::to_string(sun_cnt) + " S, " + std::to_string(moon_cnt) + " M; expected " +
                           std::to_string(target_count) + " each)"};
        }
    }

    // 5. Equal counts and no 3 consecutive identical in each column
    for (int c = 0; c < n; ++c) {
        int sun_cnt = 0, moon_cnt = 0;
        for (int r = 0; r < n; ++r) {
            if (solution[r][c] == 'S') ++sun_cnt;
            if (solution[r][c] == 'M') ++moon_cnt;

            if (r >= 2 && solution[r][c] == solution[r - 1][c] && solution[r - 1][c] == solution[r - 2][c]) {
                return {false, "Column " + std::to_string(c + 1) + " contains 3 consecutive '" +
                               std::string(1, solution[r][c]) + "' at rows " +
                               std::to_string(r - 1) + "-" + std::to_string(r + 1)};
            }
        }
        if (sun_cnt != target_count || moon_cnt != target_count) {
            return {false, "Column " + std::to_string(c + 1) + " does not have equal symbol counts (found " +
                           std::to_string(sun_cnt) + " S, " + std::to_string(moon_cnt) + " M; expected " +
                           std::to_string(target_count) + " each)"};
        }
    }

    return {true, ""};
}

} // namespace solvedin
