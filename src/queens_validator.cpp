#include "solvedin/queens_validator.hpp"

#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace solvedin {

ValidationResult validate_queens_solution(const Board& board, const std::vector<Position>& queens) {
    const int n = board.size();

    // 1. Queen count check
    if (static_cast<int>(queens.size()) != n) {
        return {false, "Invalid queen count: expected " + std::to_string(n) + " queens, but got " +
                           std::to_string(queens.size())};
    }

    // 2. Bounds check
    for (size_t i = 0; i < queens.size(); ++i) {
        const auto& q = queens[i];
        if (q.row < 0 || q.row >= n || q.col < 0 || q.col >= n) {
            return {false, "Queen #" + std::to_string(i + 1) + " at (" + std::to_string(q.row) + ", " +
                               std::to_string(q.col) + ") is outside board boundaries [0, " +
                               std::to_string(n - 1) + "]"};
        }
    }

    // 3. Row uniqueness check
    std::unordered_map<int, int> row_to_col;
    for (const auto& q : queens) {
        auto [it, inserted] = row_to_col.emplace(q.row, q.col);
        if (!inserted) {
            return {false, "Row conflict: multiple queens in row " + std::to_string(q.row) + " at columns " +
                               std::to_string(it->second) + " and " + std::to_string(q.col)};
        }
    }

    // 4. Column uniqueness check
    std::unordered_map<int, int> col_to_row;
    for (const auto& q : queens) {
        auto [it, inserted] = col_to_row.emplace(q.col, q.row);
        if (!inserted) {
            return {false, "Column conflict: multiple queens in column " + std::to_string(q.col) + " at rows " +
                               std::to_string(it->second) + " and " + std::to_string(q.row)};
        }
    }

    // 5. Region uniqueness check
    std::unordered_map<int, Position> region_to_pos;
    for (const auto& q : queens) {
        int reg = board.get_region(q.row, q.col);
        auto [it, inserted] = region_to_pos.emplace(reg, q);
        if (!inserted) {
            return {false, "Region conflict: region " + std::to_string(reg) + " contains multiple queens at (" +
                               std::to_string(it->second.row) + ", " + std::to_string(it->second.col) +
                               ") and (" + std::to_string(q.row) + ", " + std::to_string(q.col) + ")"};
        }
    }

    // 6. Adjacency (touching) check: kings move distance <= 1
    for (size_t i = 0; i < queens.size(); ++i) {
        for (size_t j = i + 1; j < queens.size(); ++j) {
            const auto& q1 = queens[i];
            const auto& q2 = queens[j];
            int dr = std::abs(q1.row - q2.row);
            int dc = std::abs(q1.col - q2.col);

            if (dr <= 1 && dc <= 1) {
                std::string direction;
                if (dr == 0 && dc == 1) {
                    direction = "horizontally";
                } else if (dr == 1 && dc == 0) {
                    direction = "vertically";
                } else {
                    direction = "diagonally";
                }
                return {false, "Adjacency conflict: queens at (" + std::to_string(q1.row) + ", " +
                                   std::to_string(q1.col) + ") and (" + std::to_string(q2.row) + ", " +
                                   std::to_string(q2.col) + ") touch " + direction};
            }
        }
    }

    return {true, "Valid"};
}

ValidationResult validate_queens_solution(const Board& board, const std::vector<int>& queen_cols) {
    std::vector<Position> queens;
    queens.reserve(queen_cols.size());
    for (size_t r = 0; r < queen_cols.size(); ++r) {
        queens.push_back(Position{static_cast<int>(r), queen_cols[r]});
    }
    return validate_queens_solution(board, queens);
}

} // namespace solvedin
