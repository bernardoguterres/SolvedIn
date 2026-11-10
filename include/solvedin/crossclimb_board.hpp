#pragma once

#include "solvedin/common.hpp"

#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

struct CrossclimbSolution {
    std::vector<std::string> ladder;

    [[nodiscard]] std::string to_string() const;
};

struct CrossclimbParseResult;

class CrossclimbBoard {
public:
    CrossclimbBoard();
    CrossclimbBoard(int word_length,
                    std::optional<std::string> start_word,
                    std::vector<std::string> start_candidates,
                    std::vector<std::string> rung_words,
                    std::optional<std::string> end_word,
                    std::vector<std::string> end_candidates);

    [[nodiscard]] int word_length() const noexcept { return word_length_; }
    [[nodiscard]] int num_rungs() const noexcept { return static_cast<int>(rung_words_.size()); }

    [[nodiscard]] bool has_start_word() const noexcept { return start_word_.has_value(); }
    [[nodiscard]] const std::optional<std::string>& start_word() const noexcept { return start_word_; }
    [[nodiscard]] const std::vector<std::string>& start_candidates() const noexcept { return start_candidates_; }
    [[nodiscard]] bool has_start_candidates() const noexcept { return !start_candidates_.empty(); }

    [[nodiscard]] const std::vector<std::string>& rung_words() const noexcept { return rung_words_; }

    [[nodiscard]] bool has_end_word() const noexcept { return end_word_.has_value(); }
    [[nodiscard]] const std::optional<std::string>& end_word() const noexcept { return end_word_; }
    [[nodiscard]] const std::vector<std::string>& end_candidates() const noexcept { return end_candidates_; }
    [[nodiscard]] bool has_end_candidates() const noexcept { return !end_candidates_.empty(); }

    static CrossclimbParseResult load_from_file(const std::string& filepath);
    static CrossclimbParseResult parse(std::istream& in);
    static CrossclimbParseResult parse_from_string(const std::string& content);

    [[nodiscard]] std::string to_string() const;

private:
    int word_length_{0};
    std::optional<std::string> start_word_;
    std::vector<std::string> start_candidates_;
    std::vector<std::string> rung_words_;
    std::optional<std::string> end_word_;
    std::vector<std::string> end_candidates_;
};

struct CrossclimbParseResult {
    bool success{false};
    std::optional<CrossclimbBoard> board;
    std::string error_message;
};

} // namespace solvedin
