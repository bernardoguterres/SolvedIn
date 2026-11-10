#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace solvedin {

struct Position {
    int row{0};
    int col{0};

    constexpr bool operator==(const Position& other) const = default;
};

enum class SolveStatus {
    NO_SOLUTION,
    UNIQUE_SOLUTION,
    MULTIPLE_SOLUTIONS,
    INCONCLUSIVE
};

[[nodiscard]] constexpr std::string_view to_string(SolveStatus s) noexcept {
    switch (s) {
    case SolveStatus::NO_SOLUTION:        return "NO_SOLUTION";
    case SolveStatus::UNIQUE_SOLUTION:    return "UNIQUE_SOLUTION";
    case SolveStatus::MULTIPLE_SOLUTIONS: return "MULTIPLE_SOLUTIONS";
    case SolveStatus::INCONCLUSIVE:       return "INCONCLUSIVE";
    }
    return "UNKNOWN";
}

struct SolveStats {
    size_t nodes_visited{0};
    double elapsed_ms{0.0};
    bool budget_exhausted{false};
};

struct SolveOptions {
    size_t max_nodes{0};      // 0 = unlimited
    double max_time_ms{0.0};  // 0.0 = unlimited
};

struct ValidationResult {
    bool is_valid{false};
    std::string error_message;

    explicit operator bool() const noexcept { return is_valid; }
};

} // namespace solvedin
