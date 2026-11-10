#pragma once

// Explainable Tango solver: named deductions first, bounded line-pattern reasoning when they stall,
// then a fallback search that applies the same deductions inside every branch.

#include "solvedin/reasoning.hpp"
#include "solvedin/tango_board.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

namespace tango_logic {

constexpr uint8_t S = 1;     // sun still possible
constexpr uint8_t M = 2;     // moon still possible
constexpr uint8_t BOTH = 3;  // undecided

using Domains = std::vector<std::vector<uint8_t>>;

// Line-pattern enumeration stops (and the line is skipped by that rule) beyond this many legal patterns.
constexpr size_t kMaxLinePatterns = 20000;

struct Deduction {
    std::string rule;
    std::string explanation;
    std::vector<Position> involved;
    std::vector<std::pair<Position, uint8_t>> sets;  // cell -> S or M
    bool contradiction{false};
};

// Domains from the board's givens.
[[nodiscard]] Domains initial_domains(const TangoBoard& board);

// The first applicable rule, cheapest first: signs, pairs/gaps, counts, sign-aware counts, line patterns.
// Returns std::nullopt when no rule applies. `work` accumulates enumeration counters.
[[nodiscard]] std::optional<Deduction> next_deduction(const TangoBoard& board, const Domains& d,
                                                      ReasoningStats* work = nullptr);

// Rendered board with the edge signs drawn between cells ('.' = undecided).
[[nodiscard]] std::string render(const TangoBoard& board, const Domains& d);

} // namespace tango_logic

[[nodiscard]] ReasoningReport reason_tango(const TangoBoard& board, const ReasoningOptions& options = {});

} // namespace solvedin
