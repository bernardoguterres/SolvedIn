#include "solvedin/reasoning.hpp"

#include <iomanip>

namespace solvedin {

const char* to_string(StepKind k) noexcept {
    switch (k) {
    case StepKind::DEDUCTION:     return "FORCED";
    case StepKind::BRANCH:        return "GUESS";
    case StepKind::CONTRADICTION: return "DEAD END";
    case StepKind::SOLUTION:      return "SOLUTION";
    case StepKind::STALL:         return "STALLED";
    case StepKind::HYPOTHESIS:    return "HYPOTHESIS";
    }
    return "?";
}

const char* to_string(SearchPhase p) noexcept {
    return p == SearchPhase::SOLVE ? "solve" : "verify-unique";
}

const char* to_string(ReasoningOutcome o) noexcept {
    switch (o) {
    case ReasoningOutcome::SOLVED_BY_DEDUCTION:  return "SOLVED_BY_DEDUCTION";
    case ReasoningOutcome::SOLVED_WITH_FALLBACK: return "SOLVED_WITH_FALLBACK";
    case ReasoningOutcome::MULTIPLE_SOLUTIONS:   return "MULTIPLE_SOLUTIONS";
    case ReasoningOutcome::NO_SOLUTION:          return "NO_SOLUTION";
    case ReasoningOutcome::STALLED:              return "STALLED";
    case ReasoningOutcome::HINT:                 return "HINT";
    case ReasoningOutcome::NO_HINT:              return "NO_HINT";
    case ReasoningOutcome::INCONCLUSIVE:         return "INCONCLUSIVE";
    case ReasoningOutcome::ANSWERED:             return "ANSWERED";
    case ReasoningOutcome::ABSTAINED:            return "ABSTAINED";
    }
    return "?";
}

std::string cell_name(Position p) {
    return "(" + std::to_string(p.row) + ", " + std::to_string(p.col) + ")";
}

namespace {

void print_step(std::ostream& os, size_t index, const ReasoningStep& s) {
    const std::string indent(static_cast<size_t>(2 * s.depth), ' ');
    os << indent << "[" << index << "] " << to_string(s.kind);
    if (s.phase == SearchPhase::VERIFY_UNIQUE) os << " (verify-unique)";
    if (!s.rule.empty()) os << " " << s.rule;
    os << ": " << s.explanation << "\n";
    if (!s.changes.empty()) {
        os << indent << "    => ";
        for (size_t i = 0; i < s.changes.size(); ++i) {
            if (i) os << ", ";
            if (s.changes[i].cell.row >= 0) os << cell_name(s.changes[i].cell) << " ";
            os << s.changes[i].value;
        }
        os << "\n";
    }
    if (!s.board.empty()) {
        std::string line;
        for (char ch : s.board) {
            if (ch == '\n') { os << indent << "    " << line << "\n"; line.clear(); }
            else line += ch;
        }
        if (!line.empty()) os << indent << "    " << line << "\n";
    }
}

} // namespace

void print_reasoning_report(std::ostream& os, const ReasoningReport& r, ReasoningMode mode, size_t max_print) {
    for (const auto& note : r.notes) os << "Note: " << note << "\n";
    if (!r.notes.empty()) os << "\n";
    const size_t n = r.steps.size();
    const size_t shown = (max_print == 0 || max_print >= n) ? n : max_print;
    for (size_t i = 0; i < shown; ++i) print_step(os, i + 1, r.steps[i]);
    if (shown < n) os << "... " << (n - shown) << " more steps not printed (use --max-steps 0 to print all)\n";

    if (mode != ReasoningMode::HINT) {
        os << "\nOutcome: " << to_string(r.outcome) << "\n";
        if (!r.final_board.empty() && r.outcome != ReasoningOutcome::MULTIPLE_SOLUTIONS) {
            const bool semantic = r.outcome == ReasoningOutcome::ANSWERED || r.outcome == ReasoningOutcome::ABSTAINED;
            os << (r.outcome == ReasoningOutcome::STALLED ? "Partial board:\n" : semantic ? "Answer: " : "Board:\n") << r.final_board;
            if (r.final_board.back() != '\n') os << "\n";
        }
        if (r.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS) {
            for (size_t i = 0; i < r.solutions_text.size(); ++i) {
                os << "--- Solution " << (i + 1) << " ---\n" << r.solutions_text[i];
                if (!r.solutions_text[i].empty() && r.solutions_text[i].back() != '\n') os << "\n";
            }
        }
        const auto& st = r.stats;
        os << "\n--- Reasoning Metrics ---\n"
           << "Forced deductions : " << st.deductions << "\n";
        for (const auto& [rule, count] : st.rule_counts) os << "  " << std::left << std::setw(22) << rule << count << "\n";
        os << "Guesses (branches): " << st.branches << "  (dead ends: " << st.failed_branches << ")\n"
           << "Search nodes      : " << st.solution_nodes << " to first solution, "
           << st.uniqueness_nodes << " verifying uniqueness\n"
           << std::fixed << std::setprecision(3)
           << "Elapsed time      : " << st.solution_ms << " ms solve, " << st.uniqueness_ms << " ms verify\n";
        for (const auto& [k, v] : st.work) os << "Work: " << k << " = " << v << "\n";
        os << "Budget status     : " << (st.budget_exhausted ? "EXHAUSTED" : "WITHIN_BUDGET") << "\n";
    } else if (r.outcome == ReasoningOutcome::NO_HINT) {
        os << "No rule applies to the board as it stands: the next move would have to be a guess.\n";
    }
}

} // namespace solvedin
