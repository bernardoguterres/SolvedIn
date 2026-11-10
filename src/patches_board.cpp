#include "solvedin/patches_board.hpp"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace solvedin {

std::string PatchClue::label() const {
    if (has_size() && has_shape()) {
        return std::to_string(*size) + static_cast<char>(shape);
    }
    if (has_size()) {
        return std::to_string(*size);
    }
    if (has_shape()) {
        return std::string(1, static_cast<char>(shape));
    }
    return "?";
}

namespace {

std::string trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r')) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) {
        --end;
    }
    return std::string(s.substr(start, end - start));
}

std::vector<std::string> split_tokens(std::string_view s) {
    std::vector<std::string> tokens;
    std::istringstream iss{std::string(s)};
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Parses a clue token:
// "." -> empty (not a clue)
// "?" -> neither size nor shape
// "4" -> size 4, no shape
// "S", "T", "W", "F" -> shape only
// "4S", "6T", "5F", "6W" -> both size and shape
bool parse_clue_token(const std::string& token, Position pos, std::optional<PatchClue>& out_clue, std::string& err) {
    if (token == "." || token == "-") {
        out_clue = std::nullopt;
        return true;
    }
    if (token == "?") {
        out_clue = PatchClue{pos, std::nullopt, ShapeConstraint::NONE};
        return true;
    }

    // Check if token ends with a shape letter 'S', 'T', 'W', 'F'
    char last_char = static_cast<char>(std::toupper(token.back()));
    bool ends_with_shape = (last_char == 'S' || last_char == 'T' || last_char == 'W' || last_char == 'F');

    if (token.size() == 1 && ends_with_shape) {
        // Shape only
        ShapeConstraint sc = ShapeConstraint::NONE;
        if (last_char == 'S') sc = ShapeConstraint::SQUARE;
        else if (last_char == 'T') sc = ShapeConstraint::TALL;
        else if (last_char == 'W') sc = ShapeConstraint::WIDE;
        else if (last_char == 'F') sc = ShapeConstraint::FREEFORM;
        out_clue = PatchClue{pos, std::nullopt, sc};
        return true;
    }

    if (ends_with_shape) {
        // Must have digits before the shape letter
        std::string num_part = token.substr(0, token.size() - 1);
        size_t parsed_len = 0;
        int sz = 0;
        try {
            sz = std::stoi(num_part, &parsed_len);
        } catch (...) {
            err = "Invalid clue token '" + token + "'";
            return false;
        }
        if (parsed_len != num_part.size() || sz <= 0) {
            err = "Clue size must be positive: got " + token;
            return false;
        }
        ShapeConstraint sc = ShapeConstraint::NONE;
        if (last_char == 'S') sc = ShapeConstraint::SQUARE;
        else if (last_char == 'T') sc = ShapeConstraint::TALL;
        else if (last_char == 'W') sc = ShapeConstraint::WIDE;
        else if (last_char == 'F') sc = ShapeConstraint::FREEFORM;
        out_clue = PatchClue{pos, sz, sc};
        return true;
    }

    // Only digits: size only
    size_t parsed_len = 0;
    int sz = 0;
    try {
        sz = std::stoi(token, &parsed_len);
    } catch (...) {
        err = "Invalid clue token '" + token + "'";
        return false;
    }
    if (parsed_len != token.size() || sz <= 0) {
        err = "Invalid clue token '" + token + "'";
        return false;
    }
    out_clue = PatchClue{pos, sz, ShapeConstraint::NONE};
    return true;
}

} // namespace

PatchesBoard::PatchesBoard() : rows_(0), cols_(0) {}

PatchesBoard::PatchesBoard(int rows, int cols, std::vector<PatchClue> clues)
    : rows_(rows), cols_(cols), clues_(std::move(clues)) {
    clue_index_.assign(rows_, std::vector<int>(cols_, -1));
    for (size_t i = 0; i < clues_.size(); ++i) {
        const auto& c = clues_[i];
        if (c.pos.row >= 0 && c.pos.row < rows_ && c.pos.col >= 0 && c.pos.col < cols_) {
            clue_index_[c.pos.row][c.pos.col] = static_cast<int>(i);
        }
    }
}

std::optional<PatchClue> PatchesBoard::clue_at(int r, int c) const {
    if (r < 0 || r >= rows_ || c < 0 || c >= cols_) {
        return std::nullopt;
    }
    int idx = clue_index_[r][c];
    if (idx < 0 || idx >= static_cast<int>(clues_.size())) {
        return std::nullopt;
    }
    return clues_[idx];
}

std::string PatchesBoard::to_string() const {
    std::ostringstream oss;
    oss << rows_ << " " << cols_ << "\nGRID\n";
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            auto cl = clue_at(r, c);
            if (cl.has_value()) {
                oss << std::setw(3) << cl->label();
            } else {
                oss << std::setw(3) << ".";
            }
            if (c + 1 < cols_) oss << " ";
        }
        oss << "\n";
    }
    return oss.str();
}

PatchesParseResult PatchesBoard::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {false, std::nullopt, "Could not open file: " + filepath};
    }
    return parse(file);
}

PatchesParseResult PatchesBoard::parse_from_string(const std::string& content) {
    std::istringstream iss(content);
    return parse(iss);
}

PatchesParseResult PatchesBoard::parse(std::istream& in) {
    std::string line;
    std::vector<std::string> clean_lines;

    while (std::getline(in, line)) {
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        std::string trimmed = trim(line);
        if (!trimmed.empty()) {
            clean_lines.push_back(trimmed);
        }
    }

    if (clean_lines.empty()) {
        return {false, std::nullopt, "Input is empty"};
    }

    auto dim_tokens = split_tokens(clean_lines[0]);
    if (dim_tokens.empty() || dim_tokens.size() > 2) {
        return {false, std::nullopt, "Invalid dimension line: expected 'R C' or 'N'"};
    }

    int rows = 0;
    int cols = 0;
    try {
        rows = std::stoi(dim_tokens[0]);
        cols = (dim_tokens.size() == 2) ? std::stoi(dim_tokens[1]) : rows;
    } catch (...) {
        return {false, std::nullopt, "Dimensions must be valid integers"};
    }

    if (rows <= 0 || cols <= 0) {
        return {false, std::nullopt, "Dimensions must be strictly positive"};
    }

    size_t line_idx = 1;
    if (line_idx < clean_lines.size() && clean_lines[line_idx] == "GRID") {
        line_idx++;
    }

    std::vector<PatchClue> clues;
    for (int r = 0; r < rows; ++r) {
        if (line_idx >= clean_lines.size()) {
            return {false, std::nullopt, "Unexpected end of input reading grid at row " + std::to_string(r)};
        }
        auto tokens = split_tokens(clean_lines[line_idx++]);
        if (static_cast<int>(tokens.size()) != cols) {
            return {false, std::nullopt, "Grid row " + std::to_string(r) + " expected " +
                    std::to_string(cols) + " tokens, got " + std::to_string(tokens.size())};
        }
        for (int c = 0; c < cols; ++c) {
            std::optional<PatchClue> clue_opt;
            std::string err;
            if (!parse_clue_token(tokens[c], Position{r, c}, clue_opt, err)) {
                return {false, std::nullopt, err + " at row " + std::to_string(r) + ", col " + std::to_string(c)};
            }
            if (clue_opt.has_value()) {
                clues.push_back(*clue_opt);
            }
        }
    }

    if (clues.empty()) {
        return {false, std::nullopt, "Patches puzzle must have at least one clue"};
    }

    return {true, PatchesBoard(rows, cols, std::move(clues)), ""};
}

} // namespace solvedin
