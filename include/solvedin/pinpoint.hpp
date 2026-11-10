#pragma once

#include "solvedin/common.hpp"

#include <istream>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

struct PinpointCategory {
    std::string name;
    std::string description;
    std::vector<std::string> terms;
};

struct PinpointGuess {
    std::string category_name;
    double confidence{0.0}; // 0.0 to 1.0
    std::vector<std::string> supported_clues;
    std::vector<std::string> unsupported_clues;
};

struct PinpointResult {
    SolveStatus status{SolveStatus::NO_SOLUTION};
    std::vector<PinpointGuess> ranked_guesses;
    std::vector<std::string> input_clues;
    bool is_unknown{true};

    [[nodiscard]] std::string to_string() const;
};

class PinpointLexicon {
public:
    PinpointLexicon() = default;

    void add_category(PinpointCategory cat);
    [[nodiscard]] const std::vector<PinpointCategory>& categories() const noexcept { return categories_; }
    [[nodiscard]] size_t size() const noexcept { return categories_.size(); }
    [[nodiscard]] bool empty() const noexcept { return categories_.empty(); }

    static PinpointLexicon load_from_file(const std::string& filepath);
    static PinpointLexicon load_from_stream(std::istream& in);
    static PinpointLexicon get_default_lexicon();

private:
    std::vector<PinpointCategory> categories_;
};

class PinpointGuesser {
public:
    explicit PinpointGuesser(PinpointLexicon lexicon, double confidence_threshold = 0.50);

    [[nodiscard]] PinpointResult guess(const std::vector<std::string>& clues) const;

    [[nodiscard]] double confidence_threshold() const noexcept { return confidence_threshold_; }
    void set_confidence_threshold(double thresh) noexcept { confidence_threshold_ = thresh; }

    static std::vector<std::string> load_clues_from_file(const std::string& filepath);

private:
    PinpointLexicon lexicon_;
    double confidence_threshold_{0.50};
};

} // namespace solvedin
