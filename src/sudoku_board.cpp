#include "solvedin/sudoku_board.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace solvedin {

SudokuBoard::SudokuBoard() {
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            grid_[r][c] = 0;
            is_given_[r][c] = false;
        }
    }
}

SudokuBoard::SudokuBoard(const std::array<std::array<int, SIZE>, SIZE>& grid)
    : grid_(grid) {
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            is_given_[r][c] = (grid_[r][c] != 0);
        }
    }
}

int SudokuBoard::get(int r, int c) const {
    if (r < 0 || r >= SIZE || c < 0 || c >= SIZE) {
        throw std::out_of_range("Sudoku cell coordinates out of range");
    }
    return grid_[r][c];
}

bool SudokuBoard::is_given(int r, int c) const {
    if (r < 0 || r >= SIZE || c < 0 || c >= SIZE) {
        throw std::out_of_range("Sudoku cell coordinates out of range");
    }
    return is_given_[r][c];
}

int SudokuBoard::givens_count() const noexcept {
    int count = 0;
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if (is_given_[r][c]) ++count;
        }
    }
    return count;
}

SudokuParseResult SudokuBoard::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {false, std::nullopt, "Cannot open sudoku file: " + filepath};
    }
    return parse(file);
}

SudokuParseResult SudokuBoard::parse_from_string(const std::string& content) {
    std::istringstream stream(content);
    return parse(stream);
}

SudokuParseResult SudokuBoard::parse(std::istream& in) {
    std::vector<std::vector<std::string>> lines_of_tokens;
    std::string line;

    while (std::getline(in, line)) {
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        std::istringstream line_stream(line);
        std::vector<std::string> row_tokens;
        std::string token;
        while (line_stream >> token) {
            row_tokens.push_back(token);
        }
        if (!row_tokens.empty()) {
            lines_of_tokens.push_back(std::move(row_tokens));
        }
    }

    if (lines_of_tokens.empty()) {
        return {false, std::nullopt, "Empty board file"};
    }

    size_t start_line = 0;
    if (lines_of_tokens[0].size() == 1 && lines_of_tokens[0][0] == "6") {
        start_line = 1;
    }

    std::vector<int> cells;
    cells.reserve(36);

    for (size_t l = start_line; l < lines_of_tokens.size(); ++l) {
        for (const auto& tok : lines_of_tokens[l]) {
            if (tok.size() > 1 && tok.find_first_of("123456.0") != std::string::npos &&
                tok.find_first_not_of("123456.0") == std::string::npos) {
                for (char ch : tok) {
                    if (ch == '.' || ch == '0') {
                        cells.push_back(0);
                    } else if (ch >= '1' && ch <= '6') {
                        cells.push_back(ch - '0');
                    } else {
                        return {false, std::nullopt, "Invalid character '" + std::string(1, ch) + "' in sudoku grid"};
                    }
                }
            } else if (tok == "." || tok == "0") {
                cells.push_back(0);
            } else {
                try {
                    size_t pos = 0;
                    int val = std::stoi(tok, &pos);
                    if (pos != tok.size() || val < 1 || val > 6) {
                        return {false, std::nullopt, "Invalid sudoku cell token '" + tok + "' (expected 1-6 or .)"};
                    }
                    cells.push_back(val);
                } catch (...) {
                    return {false, std::nullopt, "Invalid non-integer sudoku token '" + tok + "'"};
                }
            }
        }
    }

    if (cells.size() != 36) {
        return {false, std::nullopt, "Malformed sudoku dimensions: expected 36 cells, got " + std::to_string(cells.size())};
    }

    std::array<std::array<int, SIZE>, SIZE> grid{};
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            grid[r][c] = cells[r * SIZE + c];
        }
    }

    // Check for contradictory givens
    for (int r = 0; r < SIZE; ++r) {
        std::unordered_set<int> row_digits;
        for (int c = 0; c < SIZE; ++c) {
            int d = grid[r][c];
            if (d != 0) {
                if (row_digits.count(d)) {
                    return {false, std::nullopt, "Contradictory givens: duplicate digit " + std::to_string(d) + " in row " + std::to_string(r + 1)};
                }
                row_digits.insert(d);
            }
        }
    }

    for (int c = 0; c < SIZE; ++c) {
        std::unordered_set<int> col_digits;
        for (int r = 0; r < SIZE; ++r) {
            int d = grid[r][c];
            if (d != 0) {
                if (col_digits.count(d)) {
                    return {false, std::nullopt, "Contradictory givens: duplicate digit " + std::to_string(d) + " in column " + std::to_string(c + 1)};
                }
                col_digits.insert(d);
            }
        }
    }

    for (int b = 0; b < NUM_BOXES; ++b) {
        std::unordered_set<int> box_digits;
        int br = (b / 2) * BOX_ROWS;
        int bc = (b % 2) * BOX_COLS;
        for (int dr = 0; dr < BOX_ROWS; ++dr) {
            for (int dc = 0; dc < BOX_COLS; ++dc) {
                int d = grid[br + dr][bc + dc];
                if (d != 0) {
                    if (box_digits.count(d)) {
                        return {false, std::nullopt, "Contradictory givens: duplicate digit " + std::to_string(d) + " in 2x3 box " + std::to_string(b + 1)};
                    }
                    box_digits.insert(d);
                }
            }
        }
    }

    return {true, SudokuBoard(grid), ""};
}

std::string SudokuBoard::to_string() const {
    std::ostringstream ss;
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            int val = grid_[r][c];
            if (val == 0) {
                ss << ".";
            } else {
                ss << val;
            }
            if (c + 1 == BOX_COLS) {
                ss << " | ";
            } else if (c + 1 < SIZE) {
                ss << " ";
            }
        }
        ss << "\n";
        if (r + 1 < SIZE && (r + 1) % BOX_ROWS == 0) {
            ss << "------+-------\n";
        }
    }
    return ss.str();
}

} // namespace solvedin
