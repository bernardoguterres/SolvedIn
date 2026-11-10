#pragma once

#include "solvedin/common.hpp"

#include <cstddef>
#include <istream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace solvedin {

class Board;

struct BoardParseResult;

class Board {
public:
    Board() = default;
    Board(int size, std::vector<int> regions);

    [[nodiscard]] int size() const noexcept { return size_; }
    [[nodiscard]] int get_region(int r, int c) const;
    [[nodiscard]] int get_region_index(int r, int c) const;
    [[nodiscard]] const std::vector<int>& regions() const noexcept { return regions_; }
    [[nodiscard]] const std::vector<int>& distinct_regions() const noexcept { return distinct_regions_; }

    static BoardParseResult load_from_file(const std::string& filepath);
    static BoardParseResult parse(std::istream& in);
    static BoardParseResult parse_from_string(const std::string& content);

private:
    int size_{0};
    std::vector<int> regions_;
    std::vector<int> distinct_regions_;
    std::unordered_map<int, int> region_to_index_;
};

struct BoardParseResult {
    bool success{false};
    std::optional<Board> board;
    std::string error_message;
};

} // namespace solvedin
