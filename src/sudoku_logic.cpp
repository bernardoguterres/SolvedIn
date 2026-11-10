#include "solvedin/sudoku_logic.hpp"
#include "solvedin/sudoku_validator.hpp"

#include <bit>
#include <chrono>
#include <sstream>

namespace solvedin {

namespace sudoku_logic {

namespace {

constexpr uint8_t FULL = 0b1111110;  // digits 1..6
constexpr int BR = SudokuBoard::BOX_ROWS, BC = SudokuBoard::BOX_COLS;

// Units: rows 0..5, columns 6..11, boxes 12..17.
std::vector<Position> unit_cells(int u) {
    std::vector<Position> v;
    if (u < N) for (int c = 0; c < N; ++c) v.push_back({u, c});
    else if (u < 2 * N) for (int r = 0; r < N; ++r) v.push_back({r, u - N});
    else {
        const int b = u - 2 * N, r0 = (b / (N / BC)) * BR, c0 = (b % (N / BC)) * BC;
        for (int r = r0; r < r0 + BR; ++r)
            for (int c = c0; c < c0 + BC; ++c) v.push_back({r, c});
    }
    return v;
}

std::string unit_name(int u) {
    if (u < N) return "row " + std::to_string(u);
    if (u < 2 * N) return "column " + std::to_string(u - N);
    const int b = u - 2 * N, r0 = (b / (N / BC)) * BR, c0 = (b % (N / BC)) * BC;
    return "box " + std::to_string(b) + " (rows " + std::to_string(r0) + "-" + std::to_string(r0 + BR - 1) + ", columns " +
           std::to_string(c0) + "-" + std::to_string(c0 + BC - 1) + ")";
}

std::string digits_text(uint8_t m) {
    std::string s;
    for (int d = 1; d <= N; ++d) if (m >> d & 1) s += (s.empty() ? "" : ",") + std::to_string(d);
    return s.empty() ? "none" : s;
}

uint8_t unit_digits(const State& s, int u) {
    uint8_t m = 0;
    for (auto p : unit_cells(u)) if (s.grid[p.row][p.col]) m |= static_cast<uint8_t>(1 << s.grid[p.row][p.col]);
    return m;
}

int box_unit(int r, int c) { return 2 * N + SudokuBoard::box_index(r, c); }

} // namespace

State initial_state(const SudokuBoard& board) {
    State s;
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c) s.grid[r][c] = board.get(r, c);
    return s;
}

uint8_t candidates(const State& s, int r, int c) {
    if (s.grid[r][c]) return 0;
    uint8_t seen = static_cast<uint8_t>(unit_digits(s, r) | unit_digits(s, N + c) | unit_digits(s, box_unit(r, c)));
    return static_cast<uint8_t>(FULL & ~seen & ~s.removed[r][c]);
}

std::optional<Deduction> next_deduction(const State& s, bool basic) {
    // contradictions: repeated digit, empty cell, digit with no place in a unit
    for (int u = 0; u < 3 * N; ++u) {
        uint8_t m = 0;
        for (auto p : unit_cells(u)) {
            int d = s.grid[p.row][p.col];
            if (!d) continue;
            if (m >> d & 1) return Deduction{"repeat", unit_name(u) + " contains " + std::to_string(d) + " twice.", {}, {}, {}, true};
            m |= static_cast<uint8_t>(1 << d);
        }
    }
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c)
            if (!s.grid[r][c] && candidates(s, r, c) == 0)
                return Deduction{"no-candidate", cell_name({r, c}) + " has no digit left.", {{r, c}}, {}, {}, true};
    for (int u = 0; u < 3 * N; ++u) {
        uint8_t placed = unit_digits(s, u), possible = placed;
        for (auto p : unit_cells(u)) possible |= candidates(s, p.row, p.col);
        if ((possible & FULL) != FULL)
            return Deduction{"no-place", unit_name(u) + " has no place left for " + digits_text(static_cast<uint8_t>(FULL & ~possible)) + ".", {}, {}, {}, true};
    }

    // naked single
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c) {
            uint8_t m = candidates(s, r, c);
            if (s.grid[r][c] || std::popcount(m) != 1) continue;
            int d = std::countr_zero(m);
            std::string why = cell_name({r, c}) + ": row " + std::to_string(r) + " has " + digits_text(unit_digits(s, r)) +
                              ", column " + std::to_string(c) + " has " + digits_text(unit_digits(s, N + c)) + ", " +
                              unit_name(box_unit(r, c)) + " has " + digits_text(unit_digits(s, box_unit(r, c)));
            if (s.removed[r][c]) why += ", and earlier eliminations removed " + digits_text(s.removed[r][c]);
            why += ". Only " + std::to_string(d) + " is left.";
            return Deduction{"naked-single", why, {{r, c}}, {{{r, c}, d}}, {}, false};
        }
    // hidden single
    for (int u = 0; u < 3 * N; ++u) {
        uint8_t placed = unit_digits(s, u);
        for (int d = 1; d <= N; ++d) {
            if (placed >> d & 1) continue;
            std::vector<Position> where;
            for (auto p : unit_cells(u)) if (candidates(s, p.row, p.col) >> d & 1) where.push_back(p);
            if (where.size() != 1) continue;
            return Deduction{"hidden-single",
                             "In " + unit_name(u) + ", " + std::to_string(d) + " fits only at " + cell_name(where[0]) +
                                 ": every other open cell of that " + (u < N ? "row" : u < 2 * N ? "column" : "box") +
                                 " already sees a " + std::to_string(d) + " in its row, column or box, or had it eliminated.",
                             where, {{where[0], d}}, {}, false};
        }
    }
    if (basic) return std::nullopt;

    // locked candidates, box -> line ("pointing") and line -> box ("claiming")
    for (int b = 0; b < N; ++b) {
        const int bu = 2 * N + b;
        uint8_t placed = unit_digits(s, bu);
        for (int d = 1; d <= N; ++d) {
            if (placed >> d & 1) continue;
            std::vector<Position> where;
            for (auto p : unit_cells(bu)) if (candidates(s, p.row, p.col) >> d & 1) where.push_back(p);
            if (where.size() < 2) continue;
            for (int dir = 0; dir < 2; ++dir) {
                const int line = dir ? where[0].col : where[0].row;
                bool same = true;
                for (auto p : where) same &= (dir ? p.col : p.row) == line;
                if (!same) continue;
                Deduction D{"locked-box-line", "", where, {}, {}, false};
                for (auto p : unit_cells(dir ? N + line : line))
                    if (SudokuBoard::box_index(p.row, p.col) != b && (candidates(s, p.row, p.col) >> d & 1)) D.eliminations.push_back({p, d});
                if (D.eliminations.empty()) continue;
                const std::string ln = (dir ? "column " : "row ") + std::to_string(line);
                D.explanation = "Every remaining place for " + std::to_string(d) + " in " + unit_name(bu) + " lies on " + ln + ", so the " +
                                std::to_string(d) + " of " + ln + " must be inside that box: eliminate " + std::to_string(d) + " from " + ln + " outside the box.";
                return D;
            }
        }
    }
    for (int u = 0; u < 2 * N; ++u) {
        uint8_t placed = unit_digits(s, u);
        for (int d = 1; d <= N; ++d) {
            if (placed >> d & 1) continue;
            std::vector<Position> where;
            for (auto p : unit_cells(u)) if (candidates(s, p.row, p.col) >> d & 1) where.push_back(p);
            if (where.size() < 2) continue;
            const int b = SudokuBoard::box_index(where[0].row, where[0].col);
            bool same = true;
            for (auto p : where) same &= SudokuBoard::box_index(p.row, p.col) == b;
            if (!same) continue;
            Deduction D{"locked-line-box", "", where, {}, {}, false};
            for (auto p : unit_cells(2 * N + b)) {
                bool on_line = u < N ? p.row == u : p.col == u - N;
                if (!on_line && (candidates(s, p.row, p.col) >> d & 1)) D.eliminations.push_back({p, d});
            }
            if (D.eliminations.empty()) continue;
            D.explanation = "Every remaining place for " + std::to_string(d) + " in " + unit_name(u) + " is inside " + unit_name(2 * N + b) +
                            ", so that box's " + std::to_string(d) + " is on " + unit_name(u) + ": eliminate " + std::to_string(d) +
                            " from the rest of the box.";
            return D;
        }
    }
    // naked pair
    for (int u = 0; u < 3 * N; ++u) {
        auto cells = unit_cells(u);
        for (size_t i = 0; i < cells.size(); ++i) {
            uint8_t a = candidates(s, cells[i].row, cells[i].col);
            if (std::popcount(a) != 2) continue;
            for (size_t j = i + 1; j < cells.size(); ++j) {
                if (candidates(s, cells[j].row, cells[j].col) != a) continue;
                Deduction D{"naked-pair", "", {cells[i], cells[j]}, {}, {}, false};
                for (size_t k = 0; k < cells.size(); ++k) {
                    if (k == i || k == j) continue;
                    uint8_t m = candidates(s, cells[k].row, cells[k].col) & a;
                    for (int d = 1; d <= N; ++d) if (m >> d & 1) D.eliminations.push_back({cells[k], d});
                }
                if (D.eliminations.empty()) continue;
                D.explanation = cell_name(cells[i]) + " and " + cell_name(cells[j]) + " in " + unit_name(u) + " can only hold " + digits_text(a) +
                                ", so those two digits are used up there: eliminate them from the other cells of " + unit_name(u) + ".";
                return D;
            }
        }
    }
    return std::nullopt;
}

void apply(State& s, const Deduction& d) {
    for (auto& [p, v] : d.placements) s.grid[p.row][p.col] = v;
    for (auto& [p, v] : d.eliminations) s.removed[p.row][p.col] |= static_cast<uint8_t>(1 << v);
}

std::string render(const State& s) {
    std::ostringstream os;
    for (int r = 0; r < N; ++r) {
        if (r && r % BR == 0) os << "------+------\n";
        for (int c = 0; c < N; ++c) {
            if (c && c % BC == 0) os << "| ";
            os << (s.grid[r][c] ? static_cast<char>('0' + s.grid[r][c]) : '.') << (c + 1 < N ? " " : "");
        }
        os << "\n";
    }
    return os.str();
}

} // namespace sudoku_logic

namespace {

using namespace sudoku_logic;

struct Ctx {
    const SudokuBoard& board;
    const ReasoningOptions& opt;
    ReasoningReport& rep;
    std::chrono::steady_clock::time_point start, first_solution_at;
    SearchPhase phase{SearchPhase::SOLVE};
    std::vector<State> solutions;
    size_t max_solutions{2};
    bool stop{false};
    void log(ReasoningStep s) { s.phase = phase; if (rep.steps.size() < opt.max_logged_steps) rep.steps.push_back(std::move(s)); }
    size_t& nodes() { return phase == SearchPhase::SOLVE ? rep.stats.solution_nodes : rep.stats.uniqueness_nodes; }
    bool over_budget() const {
        if (opt.max_nodes && rep.stats.solution_nodes + rep.stats.uniqueness_nodes >= opt.max_nodes) return true;
        return opt.max_time_ms > 0 && std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= opt.max_time_ms;
    }
};

bool deduce_all(Ctx& ctx, State& s, int depth, bool single = false) {
    while (auto D = next_deduction(s, ctx.opt.basic_rules)) {
        ReasoningStep st{D->contradiction ? StepKind::CONTRADICTION : StepKind::DEDUCTION, SearchPhase::SOLVE, depth, D->rule, D->explanation, D->involved, {}, ""};
        if (D->contradiction) { ctx.log(std::move(st)); return false; }
        for (auto& [p, v] : D->placements) st.changes.push_back({p, std::to_string(v)});
        for (auto& [p, v] : D->eliminations) st.changes.push_back({p, "not " + std::to_string(v)});
        apply(s, *D);
        ++ctx.rep.stats.deductions;
        ++ctx.rep.stats.rule_counts[D->rule];
        ctx.log(std::move(st));
        if (single) break;
    }
    return true;
}

bool complete(const State& s) {
    for (auto& row : s.grid) for (int v : row) if (!v) return false;
    return true;
}

void search(Ctx& ctx, State s, int depth) {
    if (ctx.stop) return;
    if (ctx.over_budget()) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return; }
    ++ctx.nodes();
    if (!deduce_all(ctx, s, depth)) { if (depth > 0) ++ctx.rep.stats.failed_branches; return; }
    if (complete(s)) {
        auto v = validate_sudoku_solution(ctx.board, s.grid);
        if (!v.is_valid) {
            ctx.log({StepKind::CONTRADICTION, SearchPhase::SOLVE, depth, "validator", v.error_message, {}, {}, ""});
            if (depth > 0) ++ctx.rep.stats.failed_branches;
            return;
        }
        ctx.log({StepKind::SOLUTION, SearchPhase::SOLVE, depth, "", "every cell is filled and the independent validator accepts the grid.", {}, {}, render(s)});
        ctx.solutions.push_back(s);
        if (ctx.solutions.size() == 1) { ctx.first_solution_at = std::chrono::steady_clock::now(); ctx.phase = SearchPhase::VERIFY_UNIQUE; }
        if (ctx.solutions.size() >= ctx.max_solutions) ctx.stop = true;
        return;
    }
    Position best{-1, -1};
    int best_n = 99;
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c) {
            if (s.grid[r][c]) continue;
            int k = std::popcount(candidates(s, r, c));
            if (k < best_n) { best_n = k; best = {r, c}; }
        }
    if (best.row < 0) return;
    const uint8_t m = candidates(s, best.row, best.col);
    bool first = true;
    for (int d = 1; d <= N && !ctx.stop; ++d) {
        if (!(m >> d & 1)) continue;
        std::string why = first ? "no rule applies. " + cell_name(best) + " has the fewest candidates (" + std::to_string(best_n) + ": " +
                                      [&] { std::string t; for (int x = 1; x <= N; ++x) if (m >> x & 1) t += (t.empty() ? "" : ",") + std::to_string(x); return t; }() +
                                      "); try " + std::to_string(d) + "."
                                : (ctx.phase == SearchPhase::VERIFY_UNIQUE ? "a solution is known; to prove it unique, try " + std::to_string(d) + " at " + cell_name(best) + "."
                                                                           : "the previous digit failed; try " + std::to_string(d) + " at " + cell_name(best) + ".");
        ctx.log({StepKind::BRANCH, SearchPhase::SOLVE, depth, "guess", why, {best}, {{best, std::to_string(d)}}, first ? render(s) : ""});
        first = false;
        ++ctx.rep.stats.branches;
        State next = s;
        next.grid[best.row][best.col] = d;
        search(ctx, next, depth + 1);
    }
}

} // namespace

ReasoningReport reason_sudoku(const SudokuBoard& board, const ReasoningOptions& opt) {
    ReasoningReport rep;
    Ctx ctx{board, opt, rep, std::chrono::steady_clock::now(), {}, SearchPhase::SOLVE, {}, opt.check_unique ? size_t{2} : size_t{1}, false};
    State s = initial_state(board);
    auto elapsed = [&](auto from) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };
    if (opt.mode != ReasoningMode::EXPLAIN) {
        ctx.nodes() = 1;
        bool ok = deduce_all(ctx, s, 0, opt.mode == ReasoningMode::HINT);
        rep.final_board = render(s);
        rep.stats.solution_ms = elapsed(ctx.start);
        if (opt.mode == ReasoningMode::HINT) {
            rep.outcome = rep.steps.empty() ? ReasoningOutcome::NO_HINT : (ok ? ReasoningOutcome::HINT : ReasoningOutcome::NO_SOLUTION);
            return rep;
        }
        if (!ok) rep.outcome = ReasoningOutcome::NO_SOLUTION;
        else if (complete(s)) rep.outcome = validate_sudoku_solution(board, s.grid).is_valid ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::NO_SOLUTION;
        else {
            rep.outcome = ReasoningOutcome::STALLED;
            rep.steps.push_back({StepKind::STALL, SearchPhase::SOLVE, 0, "", "no rule can prove another digit or elimination; the board below is partial.", {}, {}, rep.final_board});
        }
        return rep;
    }
    search(ctx, s, 0);
    if (ctx.solutions.empty()) {
        rep.stats.solution_ms = elapsed(ctx.start);
        rep.outcome = rep.stats.budget_exhausted ? ReasoningOutcome::INCONCLUSIVE : ReasoningOutcome::NO_SOLUTION;
        return rep;
    }
    rep.stats.solution_ms = std::chrono::duration<double, std::milli>(ctx.first_solution_at - ctx.start).count();
    rep.stats.uniqueness_ms = elapsed(ctx.first_solution_at);
    for (auto& x : ctx.solutions) rep.solutions_text.push_back(render(x));
    rep.final_board = rep.solutions_text[0];
    if (ctx.solutions.size() >= 2) rep.outcome = ReasoningOutcome::MULTIPLE_SOLUTIONS;
    else if (rep.stats.budget_exhausted && opt.check_unique) rep.outcome = ReasoningOutcome::INCONCLUSIVE;
    else rep.outcome = rep.stats.branches == 0 ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::SOLVED_WITH_FALLBACK;
    return rep;
}

} // namespace solvedin
