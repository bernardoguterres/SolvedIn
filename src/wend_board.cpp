#include "solvedin/wend_board.hpp"

#include <cctype>
#include <fstream>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace solvedin {

namespace {

std::string trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return std::string(s.substr(start, end - start));
}

} // namespace

WendBoard::WendBoard() : rows_(0), cols_(0) {}

WendBoard::WendBoard(int rows, int cols,
                     std::vector<std::string> grid,
                     std::vector<int> target_lengths)
    : rows_(rows), cols_(cols),
      grid_(std::move(grid)),
      target_lengths_(std::move(target_lengths)) {
    if (rows_ <= 0 || cols_ <= 0) {
        throw std::invalid_argument("Grid dimensions must be positive");
    }
    if (static_cast<int>(grid_.size()) != rows_) {
        throw std::invalid_argument("Grid row count does not match rows dimension");
    }
    for (const auto& row_str : grid_) {
        if (static_cast<int>(row_str.size()) != cols_) {
            throw std::invalid_argument("Grid column count does not match cols dimension");
        }
        for (char c : row_str) {
            if (!std::isalpha(static_cast<unsigned char>(c)) && c != '.') {
                throw std::invalid_argument("Grid cell must be an alphabetic character or '.'");
            }
        }
    }
    int sum_len = std::accumulate(target_lengths_.begin(), target_lengths_.end(), 0);
    int let_cnt = letter_count();
    if (sum_len != let_cnt) {
        throw std::invalid_argument("Sum of word lengths must equal total grid letter cells");
    }
    for (int len : target_lengths_) {
        if (len <= 0) {
            throw std::invalid_argument("Word length must be positive");
        }
    }
}

char WendBoard::letter_at(int r, int c) const {
    if (r < 0 || r >= rows_ || c < 0 || c >= cols_) {
        throw std::out_of_range("Grid coordinates out of bounds");
    }
    return grid_[r][c];
}

std::string WendBoard::to_string() const {
    std::ostringstream oss;
    oss << rows_ << " " << cols_ << "\nGRID\n";
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            oss << grid_[r][c] << (c + 1 < cols_ ? " " : "");
        }
        oss << "\n";
    }
    oss << "LENGTHS:";
    for (int len : target_lengths_) {
        oss << " " << len;
    }
    oss << "\n";
    return oss.str();
}

std::string WendSolution::to_string(int rows, int cols) const {
    std::ostringstream oss;
    oss << "Wend Solution (" << words.size() << " words):\n";
    std::vector<std::vector<int>> cell_word_idx(rows, std::vector<int>(cols, -1));

    for (size_t i = 0; i < words.size(); ++i) {
        const auto& w = words[i];
        oss << "  [" << (i + 1) << "] " << w.word << " (" << w.length() << "):";
        for (const auto& pos : w.path) {
            oss << " (" << pos.row << "," << pos.col << ")";
            if (pos.row >= 0 && pos.row < rows && pos.col >= 0 && pos.col < cols) {
                cell_word_idx[pos.row][pos.col] = static_cast<int>(i + 1);
            }
        }
        oss << "\n";
    }

    oss << "Cover Map:\n";
    for (int r = 0; r < rows; ++r) {
        oss << "  ";
        for (int c = 0; c < cols; ++c) {
            if (cell_word_idx[r][c] > 0) {
                oss << cell_word_idx[r][c] << " ";
            } else {
                oss << ". ";
            }
        }
        oss << "\n";
    }
    return oss.str();
}

WendParseResult WendBoard::load_from_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        return WendParseResult{false, std::nullopt, "Could not open file: " + filepath};
    }
    return parse(ifs);
}

WendParseResult WendBoard::parse_from_string(const std::string& content) {
    std::istringstream iss(content);
    return parse(iss);
}

WendParseResult WendBoard::parse(std::istream& in) {
    std::string line;
    int rows = 0;
    int cols = 0;
    bool found_dims = false;
    std::vector<std::string> grid;
    std::vector<int> target_lengths;

    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }

        if (!found_dims) {
            std::istringstream iss(t);
            std::string token;
            if (iss >> token) {
                if (token == "DIMENSIONS:" || token == "DIMENSIONS") {
                    iss >> rows >> cols;
                } else {
                    try {
                        rows = std::stoi(token);
                        iss >> cols;
                    } catch (...) {
                        return WendParseResult{false, std::nullopt, "Failed to parse board dimensions: " + t};
                    }
                }
                if (rows <= 0 || cols <= 0) {
                    return WendParseResult{false, std::nullopt, "Board dimensions must be positive: " + t};
                }
                found_dims = true;
                continue;
            }
        }

        if (t == "GRID") {
            continue;
        }

        if (t.rfind("LENGTHS:", 0) == 0 || t.rfind("LENGTHS", 0) == 0 ||
            t.rfind("TARGETS:", 0) == 0 || t.rfind("TARGETS", 0) == 0) {
            size_t colon_pos = t.find(':');
            std::string lengths_str = (colon_pos != std::string::npos) ? t.substr(colon_pos + 1) : t.substr(7);
            std::istringstream liss(lengths_str);
            int len = 0;
            while (liss >> len) {
                target_lengths.push_back(len);
            }
            continue;
        }

        // Reading grid rows
        if (static_cast<int>(grid.size()) < rows) {
            std::string row_letters;
            std::istringstream iss_row(t);
            std::string tok;
            std::vector<std::string> tokens;
            while (iss_row >> tok) {
                tokens.push_back(tok);
            }
            if (tokens.size() == static_cast<size_t>(cols)) {
                for (const auto& tk : tokens) {
                    if (tk.size() == 1) {
                        char c = tk[0];
                        if (std::isalpha(static_cast<unsigned char>(c))) {
                            row_letters.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                        } else if (c == '.') {
                            row_letters.push_back('.');
                        }
                    }
                }
            } else {
                for (char c : t) {
                    if (std::isspace(static_cast<unsigned char>(c))) continue;
                    if (std::isalpha(static_cast<unsigned char>(c))) {
                        row_letters.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                    } else if (c == '.') {
                        row_letters.push_back('.');
                    }
                }
            }
            if (static_cast<int>(row_letters.size()) != cols) {
                return WendParseResult{
                    false, std::nullopt,
                    "Row " + std::to_string(grid.size()) + " has " + std::to_string(row_letters.size()) +
                    " letters, expected " + std::to_string(cols)};
            }
            grid.push_back(row_letters);
            continue;
        }

        // If we already read rows, check if line contains lengths
        std::istringstream iss(t);
        int len = 0;
        std::vector<int> extra_lens;
        while (iss >> len) {
            extra_lens.push_back(len);
        }
        if (!extra_lens.empty()) {
            target_lengths.insert(target_lengths.end(), extra_lens.begin(), extra_lens.end());
        }
    }

    if (!found_dims) {
        return WendParseResult{false, std::nullopt, "Missing board dimensions"};
    }
    if (static_cast<int>(grid.size()) != rows) {
        return WendParseResult{false, std::nullopt, "Expected " + std::to_string(rows) + " grid rows, got " + std::to_string(grid.size())};
    }
    if (target_lengths.empty()) {
        return WendParseResult{false, std::nullopt, "Missing target word lengths"};
    }

    int sum_len = std::accumulate(target_lengths.begin(), target_lengths.end(), 0);
    int letter_cells = 0;
    for (const auto& r : grid) {
        for (char c : r) {
            if (c != '.') letter_cells++;
        }
    }
    if (sum_len != letter_cells) {
        return WendParseResult{
            false, std::nullopt,
            "Sum of target lengths (" + std::to_string(sum_len) + ") does not match grid letter cells (" +
            std::to_string(letter_cells) + ")"};
    }

    for (int len : target_lengths) {
        if (len <= 0) {
            return WendParseResult{false, std::nullopt, "Target lengths must be positive"};
        }
    }

    return WendParseResult{true, WendBoard(rows, cols, std::move(grid), std::move(target_lengths)), ""};
}

} // namespace solvedin
