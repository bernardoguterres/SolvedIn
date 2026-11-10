#include "solvedin/tango_board.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace solvedin {

namespace {

TangoSymbol parse_symbol(char ch) {
    char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    if (upper == 'S' || upper == 'O') return TangoSymbol::SUN;
    if (upper == 'M' || upper == 'X') return TangoSymbol::MOON;
    if (upper == '.') return TangoSymbol::EMPTY;
    throw std::invalid_argument(std::string("Invalid tango symbol '") + ch + "' (expected S, M, or .)");
}

EdgeClue parse_edge_clue(char ch) {
    if (ch == '=') return EdgeClue::SAME;
    if (ch == 'x' || ch == 'X') return EdgeClue::OPPOSITE;
    if (ch == '.') return EdgeClue::NONE;
    throw std::invalid_argument(std::string("Invalid tango edge clue '") + ch + "' (expected =, x, or .)");
}

char symbol_to_char(TangoSymbol s) {
    switch (s) {
    case TangoSymbol::SUN: return 'S';
    case TangoSymbol::MOON: return 'M';
    case TangoSymbol::EMPTY: return '.';
    }
    return '.';
}

char clue_to_char(EdgeClue c) {
    switch (c) {
    case EdgeClue::SAME: return '=';
    case EdgeClue::OPPOSITE: return 'x';
    case EdgeClue::NONE: return '.';
    }
    return '.';
}

} // namespace

TangoBoard::TangoBoard()
    : size_(DEFAULT_SIZE),
      grid_(DEFAULT_SIZE, std::vector<TangoSymbol>(DEFAULT_SIZE, TangoSymbol::EMPTY)),
      horizontal_clues_(DEFAULT_SIZE, std::vector<EdgeClue>(DEFAULT_SIZE - 1, EdgeClue::NONE)),
      vertical_clues_(DEFAULT_SIZE - 1, std::vector<EdgeClue>(DEFAULT_SIZE, EdgeClue::NONE)) {}

TangoBoard::TangoBoard(int size,
                       std::vector<std::vector<TangoSymbol>> grid,
                       std::vector<std::vector<EdgeClue>> horizontal_clues,
                       std::vector<std::vector<EdgeClue>> vertical_clues)
    : size_(size),
      grid_(std::move(grid)),
      horizontal_clues_(std::move(horizontal_clues)),
      vertical_clues_(std::move(vertical_clues)) {}

TangoSymbol TangoBoard::get_symbol(int r, int c) const {
    if (r < 0 || r >= size_ || c < 0 || c >= size_) {
        throw std::out_of_range("Tango cell coordinates out of range");
    }
    return grid_[r][c];
}

bool TangoBoard::is_given(int r, int c) const {
    return get_symbol(r, c) != TangoSymbol::EMPTY;
}

EdgeClue TangoBoard::get_horizontal_clue(int r, int c) const {
    if (r < 0 || r >= size_ || c < 0 || c >= size_ - 1) {
        throw std::out_of_range("Tango horizontal clue coordinates out of range");
    }
    return horizontal_clues_[r][c];
}

EdgeClue TangoBoard::get_vertical_clue(int r, int c) const {
    if (r < 0 || r >= size_ - 1 || c < 0 || c >= size_) {
        throw std::out_of_range("Tango vertical clue coordinates out of range");
    }
    return vertical_clues_[r][c];
}

TangoParseResult TangoBoard::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {false, std::nullopt, "Cannot open tango file: " + filepath};
    }
    return parse(file);
}

TangoParseResult TangoBoard::parse_from_string(const std::string& content) {
    std::istringstream stream(content);
    return parse(stream);
}

TangoParseResult TangoBoard::parse(std::istream& in) {
    std::vector<std::string> raw_tokens;
    std::string line;

    while (std::getline(in, line)) {
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        std::istringstream line_stream(line);
        std::string token;
        while (line_stream >> token) {
            raw_tokens.push_back(token);
        }
    }

    if (raw_tokens.empty()) {
        return {false, std::nullopt, "Empty tango board file"};
    }

    int size = DEFAULT_SIZE;
    size_t idx = 0;

    // Check if first token is an integer dimension N
    try {
        size_t p = 0;
        int parsed_n = std::stoi(raw_tokens[0], &p);
        if (p == raw_tokens[0].size()) {
            if (parsed_n <= 0 || (parsed_n % 2 != 0)) {
                return {false, std::nullopt, "Invalid tango size " + std::to_string(parsed_n) + " (must be positive even integer)"};
            }
            size = parsed_n;
            idx = 1;
        }
    } catch (...) {}

    std::vector<std::vector<TangoSymbol>> grid(size, std::vector<TangoSymbol>(size, TangoSymbol::EMPTY));
    std::vector<std::vector<EdgeClue>> h_clues(size, std::vector<EdgeClue>(size - 1, EdgeClue::NONE));
    std::vector<std::vector<EdgeClue>> v_clues(size - 1, std::vector<EdgeClue>(size, EdgeClue::NONE));

    enum class Section { NONE, GRID, HORIZONTAL, VERTICAL, CLUES };
    Section current_section = Section::NONE;

    std::vector<std::string> grid_tokens;
    std::vector<std::string> h_tokens;
    std::vector<std::string> v_tokens;
    std::vector<std::string> coord_clue_tokens;

    for (; idx < raw_tokens.size(); ++idx) {
        std::string upper_tok = raw_tokens[idx];
        for (char& ch : upper_tok) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

        if (upper_tok == "GRID") {
            current_section = Section::GRID;
        } else if (upper_tok == "HORIZONTAL" || upper_tok == "H_CLUES" || upper_tok == "H_EDGES") {
            current_section = Section::HORIZONTAL;
        } else if (upper_tok == "VERTICAL" || upper_tok == "V_CLUES" || upper_tok == "V_EDGES") {
            current_section = Section::VERTICAL;
        } else if (upper_tok == "CLUES") {
            current_section = Section::CLUES;
        } else {
            if (current_section == Section::GRID) {
                grid_tokens.push_back(raw_tokens[idx]);
            } else if (current_section == Section::HORIZONTAL) {
                h_tokens.push_back(raw_tokens[idx]);
            } else if (current_section == Section::VERTICAL) {
                v_tokens.push_back(raw_tokens[idx]);
            } else if (current_section == Section::CLUES) {
                coord_clue_tokens.push_back(raw_tokens[idx]);
            } else {
                // Without explicit section tags: first size*size tokens are grid, next size*(size-1) are horizontal, next (size-1)*size are vertical
                if (grid_tokens.size() < static_cast<size_t>(size * size)) {
                    grid_tokens.push_back(raw_tokens[idx]);
                } else if (h_tokens.size() < static_cast<size_t>(size * (size - 1))) {
                    h_tokens.push_back(raw_tokens[idx]);
                } else if (v_tokens.size() < static_cast<size_t>((size - 1) * size)) {
                    v_tokens.push_back(raw_tokens[idx]);
                } else {
                    return {false, std::nullopt, "Extra trailing token '" + raw_tokens[idx] + "' in tango file"};
                }
            }
        }
    }

    // Flatten multi-character strings in grid_tokens if needed
    std::vector<char> grid_chars;
    for (const auto& tok : grid_tokens) {
        for (char ch : tok) grid_chars.push_back(ch);
    }

    if (grid_chars.size() != static_cast<size_t>(size * size)) {
        return {false, std::nullopt, "Malformed tango grid: expected " + std::to_string(size * size) +
                                     " symbols, found " + std::to_string(grid_chars.size())};
    }

    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            try {
                grid[r][c] = parse_symbol(grid_chars[r * size + c]);
            } catch (const std::exception& ex) {
                return {false, std::nullopt, ex.what()};
            }
        }
    }

    // Parse horizontal clues
    if (!h_tokens.empty()) {
        std::vector<char> h_chars;
        for (const auto& tok : h_tokens) {
            for (char ch : tok) h_chars.push_back(ch);
        }
        if (h_chars.size() != static_cast<size_t>(size * (size - 1))) {
            return {false, std::nullopt, "Malformed horizontal clues: expected " +
                                         std::to_string(size * (size - 1)) + " clues, found " +
                                         std::to_string(h_chars.size())};
        }
        for (int r = 0; r < size; ++r) {
            for (int c = 0; c < size - 1; ++c) {
                try {
                    h_clues[r][c] = parse_edge_clue(h_chars[r * (size - 1) + c]);
                } catch (const std::exception& ex) {
                    return {false, std::nullopt, ex.what()};
                }
            }
        }
    }

    // Parse vertical clues
    if (!v_tokens.empty()) {
        std::vector<char> v_chars;
        for (const auto& tok : v_tokens) {
            for (char ch : tok) v_chars.push_back(ch);
        }
        if (v_chars.size() != static_cast<size_t>((size - 1) * size)) {
            return {false, std::nullopt, "Malformed vertical clues: expected " +
                                         std::to_string((size - 1) * size) + " clues, found " +
                                         std::to_string(v_chars.size())};
        }
        for (int r = 0; r < size - 1; ++r) {
            for (int c = 0; c < size; ++c) {
                try {
                    v_clues[r][c] = parse_edge_clue(v_chars[r * size + c]);
                } catch (const std::exception& ex) {
                    return {false, std::nullopt, ex.what()};
                }
            }
        }
    }

    // Parse coordinate-based clues (r1 c1 r2 c2 clue)
    if (!coord_clue_tokens.empty()) {
        if (coord_clue_tokens.size() % 5 != 0) {
            return {false, std::nullopt, "Malformed CLUES section: expected 5 tokens per clue (r1 c1 r2 c2 clue)"};
        }
        for (size_t i = 0; i < coord_clue_tokens.size(); i += 5) {
            int r1 = std::stoi(coord_clue_tokens[i]);
            int c1 = std::stoi(coord_clue_tokens[i + 1]);
            int r2 = std::stoi(coord_clue_tokens[i + 2]);
            int c2 = std::stoi(coord_clue_tokens[i + 3]);
            EdgeClue clue = parse_edge_clue(coord_clue_tokens[i + 4][0]);

            if (r1 == r2 && std::abs(c1 - c2) == 1) {
                int min_c = std::min(c1, c2);
                if (r1 >= 0 && r1 < size && min_c >= 0 && min_c < size - 1) {
                    h_clues[r1][min_c] = clue;
                }
            } else if (c1 == c2 && std::abs(r1 - r2) == 1) {
                int min_r = std::min(r1, r2);
                if (min_r >= 0 && min_r < size - 1 && c1 >= 0 && c1 < size) {
                    v_clues[min_r][c1] = clue;
                }
            } else {
                return {false, std::nullopt, "Invalid clue between non-adjacent cells (" +
                                             std::to_string(r1) + "," + std::to_string(c1) + ") and (" +
                                             std::to_string(r2) + "," + std::to_string(c2) + ")"};
            }
        }
    }

    // Check for contradictory givens and clues
    const int max_symbol_count = size / 2;

    // 1. Max count per row/col
    for (int r = 0; r < size; ++r) {
        int sun_cnt = 0, moon_cnt = 0;
        for (int c = 0; c < size; ++c) {
            if (grid[r][c] == TangoSymbol::SUN) ++sun_cnt;
            if (grid[r][c] == TangoSymbol::MOON) ++moon_cnt;
        }
        if (sun_cnt > max_symbol_count || moon_cnt > max_symbol_count) {
            return {false, std::nullopt, "Contradictory givens: row " + std::to_string(r + 1) + " exceeds max count of " + std::to_string(max_symbol_count)};
        }
    }

    for (int c = 0; c < size; ++c) {
        int sun_cnt = 0, moon_cnt = 0;
        for (int r = 0; r < size; ++r) {
            if (grid[r][c] == TangoSymbol::SUN) ++sun_cnt;
            if (grid[r][c] == TangoSymbol::MOON) ++moon_cnt;
        }
        if (sun_cnt > max_symbol_count || moon_cnt > max_symbol_count) {
            return {false, std::nullopt, "Contradictory givens: column " + std::to_string(c + 1) + " exceeds max count of " + std::to_string(max_symbol_count)};
        }
    }

    // 2. Three consecutive identical givens in any row/col
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size - 2; ++c) {
            if (grid[r][c] != TangoSymbol::EMPTY &&
                grid[r][c] == grid[r][c + 1] &&
                grid[r][c + 1] == grid[r][c + 2]) {
                return {false, std::nullopt, "Contradictory givens: 3 consecutive identical symbols in row " + std::to_string(r + 1)};
            }
        }
    }

    for (int c = 0; c < size; ++c) {
        for (int r = 0; r < size - 2; ++r) {
            if (grid[r][c] != TangoSymbol::EMPTY &&
                grid[r][c] == grid[r + 1][c] &&
                grid[r + 1][c] == grid[r + 2][c]) {
                return {false, std::nullopt, "Contradictory givens: 3 consecutive identical symbols in column " + std::to_string(c + 1)};
            }
        }
    }

    // 3. Contradictory edge clues between adjacent given cells
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size - 1; ++c) {
            EdgeClue clue = h_clues[r][c];
            TangoSymbol s1 = grid[r][c];
            TangoSymbol s2 = grid[r][c + 1];
            if (s1 != TangoSymbol::EMPTY && s2 != TangoSymbol::EMPTY) {
                if (clue == EdgeClue::SAME && s1 != s2) {
                    return {false, std::nullopt, "Contradictory edge clue: '=' between (" +
                                                 std::to_string(r) + "," + std::to_string(c) + ") and (" +
                                                 std::to_string(r) + "," + std::to_string(c + 1) + ") but givens differ"};
                }
                if (clue == EdgeClue::OPPOSITE && s1 == s2) {
                    return {false, std::nullopt, "Contradictory edge clue: 'x' between (" +
                                                 std::to_string(r) + "," + std::to_string(c) + ") and (" +
                                                 std::to_string(r) + "," + std::to_string(c + 1) + ") but givens are identical"};
                }
            }
        }
    }

    for (int r = 0; r < size - 1; ++r) {
        for (int c = 0; c < size; ++c) {
            EdgeClue clue = v_clues[r][c];
            TangoSymbol s1 = grid[r][c];
            TangoSymbol s2 = grid[r + 1][c];
            if (s1 != TangoSymbol::EMPTY && s2 != TangoSymbol::EMPTY) {
                if (clue == EdgeClue::SAME && s1 != s2) {
                    return {false, std::nullopt, "Contradictory edge clue: '=' between (" +
                                                 std::to_string(r) + "," + std::to_string(c) + ") and (" +
                                                 std::to_string(r + 1) + "," + std::to_string(c) + ") but givens differ"};
                }
                if (clue == EdgeClue::OPPOSITE && s1 == s2) {
                    return {false, std::nullopt, "Contradictory edge clue: 'x' between (" +
                                                 std::to_string(r) + "," + std::to_string(c) + ") and (" +
                                                 std::to_string(r + 1) + "," + std::to_string(c) + ") but givens are identical"};
                }
            }
        }
    }

    return {true, TangoBoard(size, std::move(grid), std::move(h_clues), std::move(v_clues)), ""};
}

std::string TangoBoard::to_string() const {
    std::ostringstream ss;
    ss << size_ << "\n";
    ss << "GRID\n";
    for (int r = 0; r < size_; ++r) {
        for (int c = 0; c < size_; ++c) {
            ss << symbol_to_char(grid_[r][c]) << (c + 1 < size_ ? " " : "");
        }
        ss << "\n";
    }

    ss << "HORIZONTAL\n";
    for (int r = 0; r < size_; ++r) {
        for (int c = 0; c < size_ - 1; ++c) {
            ss << clue_to_char(horizontal_clues_[r][c]) << (c + 2 < size_ ? " " : "");
        }
        ss << "\n";
    }

    ss << "VERTICAL\n";
    for (int r = 0; r < size_ - 1; ++r) {
        for (int c = 0; c < size_; ++c) {
            ss << clue_to_char(vertical_clues_[r][c]) << (c + 1 < size_ ? " " : "");
        }
        ss << "\n";
    }

    return ss.str();
}

} // namespace solvedin
