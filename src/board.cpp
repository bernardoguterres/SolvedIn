#include "solvedin/board.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace solvedin {

Board::Board(int size, std::vector<int> regions)
    : size_(size), regions_(std::move(regions)) {
    if (size_ <= 0) {
        throw std::invalid_argument("Board size must be positive");
    }
    if (static_cast<int>(regions_.size()) != size_ * size_) {
        throw std::invalid_argument("Region array size does not match N * N");
    }

    std::unordered_set<int> seen;
    for (int r : regions_) {
        if (seen.insert(r).second) {
            distinct_regions_.push_back(r);
        }
    }

    if (static_cast<int>(distinct_regions_.size()) != size_) {
        throw std::invalid_argument("Board must have exactly N distinct regions");
    }

    for (size_t i = 0; i < distinct_regions_.size(); ++i) {
        region_to_index_[distinct_regions_[i]] = static_cast<int>(i);
    }
}

int Board::get_region(int r, int c) const {
    if (r < 0 || r >= size_ || c < 0 || c >= size_) {
        throw std::out_of_range("Coordinates out of board bounds");
    }
    return regions_[r * size_ + c];
}

int Board::get_region_index(int r, int c) const {
    int reg = get_region(r, c);
    auto it = region_to_index_.find(reg);
    if (it == region_to_index_.end()) {
        throw std::runtime_error("Unknown region ID encountered");
    }
    return it->second;
}

BoardParseResult Board::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return {false, std::nullopt, "Failed to open file: " + filepath};
    }
    return parse(file);
}

BoardParseResult Board::parse_from_string(const std::string& content) {
    std::istringstream stream(content);
    return parse(stream);
}

BoardParseResult Board::parse(std::istream& in) {
    std::string token;
    if (!(in >> token)) {
        return {false, std::nullopt, "Malformed board input: empty or missing board size N"};
    }

    int n = 0;
    try {
        size_t idx = 0;
        n = std::stoi(token, &idx);
        if (idx != token.size()) {
            return {false, std::nullopt, "Malformed board input: size N is not a valid integer ('" + token + "')"};
        }
    } catch (const std::exception&) {
        return {false, std::nullopt, "Malformed board input: size N is not a valid integer ('" + token + "')"};
    }

    if (n <= 0) {
        return {false, std::nullopt, "Malformed board input: board size N must be positive (got " + std::to_string(n) + ")"};
    }

    const int total_cells = n * n;
    std::vector<int> regions;
    regions.reserve(total_cells);

    for (int i = 0; i < total_cells; ++i) {
        if (!(in >> token)) {
            return {false, std::nullopt, "Malformed board input: expected " + std::to_string(total_cells) +
                                             " region values for " + std::to_string(n) + "x" + std::to_string(n) +
                                             " board, but found only " + std::to_string(i)};
        }

        try {
            size_t idx = 0;
            int reg_id = std::stoi(token, &idx);
            if (idx != token.size()) {
                return {false, std::nullopt, "Malformed board input: non-integer token encountered in region data: '" + token + "'"};
            }
            regions.push_back(reg_id);
        } catch (const std::exception&) {
            return {false, std::nullopt, "Malformed board input: non-integer token encountered in region data: '" + token + "'"};
        }
    }

    if (in >> token) {
        return {false, std::nullopt, "Malformed board input: extra trailing token '" + token + "' after reading " +
                                         std::to_string(total_cells) + " cells"};
    }

    std::unordered_set<int> unique_regions;
    for (int reg : regions) {
        unique_regions.insert(reg);
    }

    if (static_cast<int>(unique_regions.size()) != n) {
        return {false, std::nullopt, "Malformed board input: board size is " + std::to_string(n) +
                                         ", requiring exactly " + std::to_string(n) + " distinct regions, but found " +
                                         std::to_string(unique_regions.size())};
    }

    return {true, Board(n, std::move(regions)), ""};
}

} // namespace solvedin
