#include "solvedin/zip_board.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace solvedin {

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

} // namespace

ZipBoard::ZipBoard() : rows_(0), cols_(0), max_checkpoint_(0) {}

ZipBoard::ZipBoard(int rows, int cols,
                   std::vector<std::vector<int>> grid,
                   std::vector<std::vector<bool>> h_walls,
                   std::vector<std::vector<bool>> v_walls)
    : rows_(rows),
      cols_(cols),
      max_checkpoint_(0),
      grid_(std::move(grid)),
      h_walls_(std::move(h_walls)),
      v_walls_(std::move(v_walls)) {

    int max_cp = 0;
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (grid_[r][c] > max_cp) {
                max_cp = grid_[r][c];
            }
        }
    }
    max_checkpoint_ = max_cp;
    checkpoint_positions_.assign(max_checkpoint_ + 1, Position{-1, -1});
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            int cp = grid_[r][c];
            if (cp > 0 && cp <= max_checkpoint_) {
                checkpoint_positions_[cp] = Position{r, c};
            }
        }
    }
}

int ZipBoard::checkpoint_at(int r, int c) const {
    if (r < 0 || r >= rows_ || c < 0 || c >= cols_) {
        return 0;
    }
    return grid_[r][c];
}

Position ZipBoard::checkpoint_pos(int num) const {
    if (num <= 0 || num > max_checkpoint_) {
        return Position{-1, -1};
    }
    return checkpoint_positions_[num];
}

bool ZipBoard::has_h_wall(int r, int c) const {
    if (r < 0 || r >= rows_ - 1 || c < 0 || c >= cols_) {
        return false;
    }
    return h_walls_[r][c];
}

bool ZipBoard::has_v_wall(int r, int c) const {
    if (r < 0 || r >= rows_ || c < 0 || c >= cols_ - 1) {
        return false;
    }
    return v_walls_[r][c];
}

bool ZipBoard::can_move(Position from, Position to) const {
    if (!is_in_bounds(from) || !is_in_bounds(to)) {
        return false;
    }
    int dr = to.row - from.row;
    int dc = to.col - from.col;

    // Must be orthogonal adjacency
    if (std::abs(dr) + std::abs(dc) != 1) {
        return false;
    }

    if (dr == 1) { // Moving DOWN from (from.row, from.col) to (from.row + 1, from.col)
        return !has_h_wall(from.row, from.col);
    }
    if (dr == -1) { // Moving UP from (from.row, from.col) to (from.row - 1, from.col)
        return !has_h_wall(to.row, from.col);
    }
    if (dc == 1) { // Moving RIGHT from (from.row, from.col) to (from.row, from.col + 1)
        return !has_v_wall(from.row, from.col);
    }
    if (dc == -1) { // Moving LEFT from (from.row, from.col) to (from.row, from.col - 1)
        return !has_v_wall(from.row, to.col);
    }
    return false;
}

std::string ZipBoard::to_string() const {
    std::ostringstream oss;
    oss << rows_ << " " << cols_ << "\nGRID\n";
    for (int r = 0; r < rows_; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (grid_[r][c] > 0) {
                oss << grid_[r][c];
            } else {
                oss << ".";
            }
            if (c + 1 < cols_) oss << " ";
        }
        oss << "\n";
    }

    bool has_any_h = false;
    for (int r = 0; r < rows_ - 1 && !has_any_h; ++r) {
        for (int c = 0; c < cols_; ++c) {
            if (h_walls_[r][c]) { has_any_h = true; break; }
        }
    }
    if (has_any_h) {
        oss << "WALLS_H\n";
        for (int r = 0; r < rows_ - 1; ++r) {
            for (int c = 0; c < cols_; ++c) {
                oss << (h_walls_[r][c] ? "-" : ".") << (c + 1 < cols_ ? " " : "");
            }
            oss << "\n";
        }
    }

    bool has_any_v = false;
    for (int r = 0; r < rows_ && !has_any_v; ++r) {
        for (int c = 0; c < cols_ - 1; ++c) {
            if (v_walls_[r][c]) { has_any_v = true; break; }
        }
    }
    if (has_any_v) {
        oss << "WALLS_V\n";
        for (int r = 0; r < rows_; ++r) {
            for (int c = 0; c < cols_ - 1; ++c) {
                oss << (v_walls_[r][c] ? "|" : ".") << (c + 1 < cols_ - 1 ? " " : "");
            }
            oss << "\n";
        }
    }

    return oss.str();
}

ZipParseResult ZipBoard::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {false, std::nullopt, "Could not open file: " + filepath};
    }
    return parse(file);
}

ZipParseResult ZipBoard::parse_from_string(const std::string& content) {
    std::istringstream iss(content);
    return parse(iss);
}

ZipParseResult ZipBoard::parse(std::istream& in) {
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

    // First line must contain dimensions
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
    if (rows * cols < 2) {
        return {false, std::nullopt, "Board must have at least 2 cells"};
    }

    size_t line_idx = 1;

    // Check for optional "GRID" header
    if (line_idx < clean_lines.size() && clean_lines[line_idx] == "GRID") {
        line_idx++;
    }

    std::vector<std::vector<int>> grid(rows, std::vector<int>(cols, 0));
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
            if (tokens[c] == ".") {
                grid[r][c] = 0;
            } else {
                try {
                    int cp = std::stoi(tokens[c]);
                    if (cp <= 0) {
                        return {false, std::nullopt, "Checkpoints must be positive integers: got " + tokens[c]};
                    }
                    grid[r][c] = cp;
                } catch (...) {
                    return {false, std::nullopt, "Invalid grid token '" + tokens[c] + "' at row " +
                            std::to_string(r) + ", col " + std::to_string(c)};
                }
            }
        }
    }

    // Default walls: all false
    std::vector<std::vector<bool>> h_walls(rows > 1 ? rows - 1 : 0, std::vector<bool>(cols, false));
    std::vector<std::vector<bool>> v_walls(rows, std::vector<bool>(cols > 1 ? cols - 1 : 0, false));

    // Parse optional wall sections
    while (line_idx < clean_lines.size()) {
        const std::string& section = clean_lines[line_idx++];
        if (section == "WALLS_H" || section == "HORIZONTAL" || section == "HORIZONTAL_WALLS") {
            for (int r = 0; r < rows - 1; ++r) {
                if (line_idx >= clean_lines.size()) {
                    return {false, std::nullopt, "Unexpected end of input reading horizontal walls"};
                }
                auto tokens = split_tokens(clean_lines[line_idx++]);
                // Allow either space-separated tokens or single combined string of length cols
                if (static_cast<int>(tokens.size()) == 1 && static_cast<int>(tokens[0].size()) == cols) {
                    for (int c = 0; c < cols; ++c) {
                        char ch = tokens[0][c];
                        h_walls[r][c] = (ch == '-' || ch == '1' || ch == 'W');
                    }
                } else if (static_cast<int>(tokens.size()) == cols) {
                    for (int c = 0; c < cols; ++c) {
                        const std::string& tok = tokens[c];
                        h_walls[r][c] = (tok == "-" || tok == "1" || tok == "W");
                    }
                } else {
                    return {false, std::nullopt, "Horizontal walls row " + std::to_string(r) +
                            " expected " + std::to_string(cols) + " entries"};
                }
            }
        } else if (section == "WALLS_V" || section == "VERTICAL" || section == "VERTICAL_WALLS") {
            for (int r = 0; r < rows; ++r) {
                if (line_idx >= clean_lines.size()) {
                    return {false, std::nullopt, "Unexpected end of input reading vertical walls"};
                }
                auto tokens = split_tokens(clean_lines[line_idx++]);
                int expected_cols = cols - 1;
                if (static_cast<int>(tokens.size()) == 1 && static_cast<int>(tokens[0].size()) == expected_cols) {
                    for (int c = 0; c < expected_cols; ++c) {
                        char ch = tokens[0][c];
                        v_walls[r][c] = (ch == '|' || ch == '1' || ch == 'W');
                    }
                } else if (static_cast<int>(tokens.size()) == expected_cols) {
                    for (int c = 0; c < expected_cols; ++c) {
                        const std::string& tok = tokens[c];
                        v_walls[r][c] = (tok == "|" || tok == "1" || tok == "W");
                    }
                } else {
                    return {false, std::nullopt, "Vertical walls row " + std::to_string(r) +
                            " expected " + std::to_string(expected_cols) + " entries"};
                }
            }
        } else {
            return {false, std::nullopt, "Unrecognized section header: " + section};
        }
    }

    // Validate checkpoints:
    // Checkpoints must contain at least 1 and 2, and form a continuous sequence 1..K with no duplicates
    int max_cp = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (grid[r][c] > max_cp) {
                max_cp = grid[r][c];
            }
        }
    }

    if (max_cp < 2) {
        return {false, std::nullopt, "Zip puzzle must have at least checkpoints 1 and 2 (found max checkpoint " +
                std::to_string(max_cp) + ")"};
    }

    std::vector<int> counts(max_cp + 1, 0);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int cp = grid[r][c];
            if (cp > 0) {
                counts[cp]++;
            }
        }
    }

    for (int k = 1; k <= max_cp; ++k) {
        if (counts[k] == 0) {
            return {false, std::nullopt, "Missing checkpoint number: " + std::to_string(k)};
        }
        if (counts[k] > 1) {
            return {false, std::nullopt, "Duplicate checkpoint number " + std::to_string(k) +
                    " appears " + std::to_string(counts[k]) + " times"};
        }
    }

    return {true, ZipBoard(rows, cols, std::move(grid), std::move(h_walls), std::move(v_walls)), ""};
}

} // namespace solvedin
