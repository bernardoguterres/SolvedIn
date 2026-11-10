#include "solvedin/tango_logic.hpp"
#include "solvedin/tango_validator.hpp"

#include <chrono>
#include <sstream>

namespace solvedin {

namespace tango_logic {

namespace {

const char* word(uint8_t v) { return v == S ? "sun" : "moon"; }
const char* plural(uint8_t v) { return v == S ? "suns" : "moons"; }
const char* letter(uint8_t v) { return v == S ? "S" : "M"; }
uint8_t other(uint8_t v) { return v == S ? M : S; }
char sign_char(EdgeClue e) { return e == EdgeClue::SAME ? '=' : e == EdgeClue::OPPOSITE ? 'x' : ' '; }

// A line is a row (index < n) or a column (index >= n).
struct Line {
    int n;
    int index;
    [[nodiscard]] bool is_row() const { return index < n; }
    [[nodiscard]] Position at(int k) const { return is_row() ? Position{index, k} : Position{k, index - n}; }
    [[nodiscard]] std::string name() const {
        return is_row() ? "Row " + std::to_string(index) : "Column " + std::to_string(index - n);
    }
};

// Sign between positions k and k+1 of a line.
EdgeClue sign_in_line(const TangoBoard& b, const Line& L, int k) {
    return L.is_row() ? b.get_horizontal_clue(L.index, k) : b.get_vertical_clue(k, L.index - L.n);
}

uint8_t at(const Domains& d, Position p) { return d[p.row][p.col]; }

std::string line_text(const TangoBoard& b, const Domains& d, const Line& L) {
    std::string s;
    for (int k = 0; k < L.n; ++k) {
        uint8_t v = at(d, L.at(k));
        s += v == S ? 'S' : v == M ? 'M' : '.';
        if (k + 1 < L.n) {
            char sc = sign_char(sign_in_line(b, L, k));
            s += sc == ' ' ? std::string(" ") : std::string(" ") + sc + " ";
        }
    }
    return s;
}

struct Enumeration {
    size_t count{0};
    bool capped{false};
    std::vector<uint8_t> seen;          // OR of values used at each position by legal patterns
    std::vector<size_t> sun_count;      // number of legal patterns with a sun at each position
    std::vector<std::string> examples;  // first few legal patterns
};

// Legal completions of a line: givens/current values, in-line signs, exact n/2 counts and (unless
// ignore_triples) no three equal in a row. Optionally pins one position to a value.
Enumeration enumerate_line(const TangoBoard& b, const Domains& d, const Line& L, size_t cap,
                           int pin_k = -1, uint8_t pin_v = 0, bool ignore_triples = false) {
    const int n = L.n, half = n / 2;
    Enumeration e;
    e.seen.assign(n, 0);
    e.sun_count.assign(n, 0);
    std::vector<uint8_t> cur(n, 0);
    std::vector<uint8_t> dom(n);
    std::vector<EdgeClue> sg(n > 0 ? n - 1 : 0);
    for (int k = 0; k < n; ++k) dom[k] = (k == pin_k) ? pin_v : at(d, L.at(k));
    for (int k = 0; k + 1 < n; ++k) sg[k] = sign_in_line(b, L, k);

    auto rec = [&](auto&& self, int k, int suns, int moons) -> void {
        if (e.capped) return;
        if (k == n) {
            if (++e.count > cap) { e.capped = true; return; }
            std::string txt;
            for (int i = 0; i < n; ++i) {
                e.seen[i] |= cur[i];
                if (cur[i] == S) ++e.sun_count[i];
                txt += letter(cur[i]);
            }
            if (e.examples.size() < 4) e.examples.push_back(txt);
            return;
        }
        for (uint8_t v : {S, M}) {
            if (!(dom[k] & v)) continue;
            if (v == S ? suns + 1 > half : moons + 1 > half) continue;
            if (k > 0) {
                if (sg[k - 1] == EdgeClue::SAME && cur[k - 1] != v) continue;
                if (sg[k - 1] == EdgeClue::OPPOSITE && cur[k - 1] == v) continue;
            }
            if (!ignore_triples && k >= 2 && cur[k - 1] == v && cur[k - 2] == v) continue;
            cur[k] = v;
            self(self, k + 1, suns + (v == S), moons + (v == M));
        }
    };
    rec(rec, 0, 0, 0);
    return e;
}

std::optional<Deduction> rule_signs(const TangoBoard& b, const Domains& d) {
    const int n = b.size();
    for (int dir = 0; dir < 2; ++dir) {
        for (int r = 0; r < (dir ? n - 1 : n); ++r) {
            for (int c = 0; c < (dir ? n : n - 1); ++c) {
                EdgeClue e = dir ? b.get_vertical_clue(r, c) : b.get_horizontal_clue(r, c);
                if (e == EdgeClue::NONE) continue;
                Position p{r, c}, q = dir ? Position{r + 1, c} : Position{r, c + 1};
                uint8_t a = at(d, p), bb = at(d, q);
                const char* sname = e == EdgeClue::SAME ? "'='" : "'x'";
                if (a != BOTH && bb != BOTH) {
                    if ((e == EdgeClue::SAME) != (a == bb)) {
                        return Deduction{"sign", cell_name(p) + " and " + cell_name(q) + " break the " + sname + " sign between them.",
                                         {p, q}, {}, true};
                    }
                    continue;
                }
                if (a == BOTH && bb == BOTH) continue;
                Position known = a != BOTH ? p : q, open = a != BOTH ? q : p;
                uint8_t kv = a != BOTH ? a : bb;
                uint8_t nv = e == EdgeClue::SAME ? kv : other(kv);
                return Deduction{e == EdgeClue::SAME ? "equal-sign" : "opposite-sign",
                                 cell_name(known) + " is a " + word(kv) + " and the " + sname + " sign joining it to " +
                                     cell_name(open) + " makes that cell a " + word(nv) + ".",
                                 {known, open}, {{open, nv}}, false};
            }
        }
    }
    return std::nullopt;
}

std::optional<Deduction> rule_no_three(const TangoBoard& b, const Domains& d) {
    const int n = b.size();
    for (int li = 0; li < 2 * n; ++li) {
        Line L{n, li};
        for (int k = 0; k + 2 < n; ++k) {
            Position p0 = L.at(k), p1 = L.at(k + 1), p2 = L.at(k + 2);
            uint8_t v0 = at(d, p0), v1 = at(d, p1), v2 = at(d, p2);
            if (v0 != BOTH && v0 == v1 && v1 == v2)
                return Deduction{"no-three", L.name() + " has three " + plural(v0) + " in a row at " + cell_name(p0) + ".." + cell_name(p2) + ".",
                                 {p0, p1, p2}, {}, true};
            // pair followed or preceded by an open cell
            if (v0 != BOTH && v0 == v1 && v2 == BOTH)
                return Deduction{"pair", cell_name(p0) + " and " + cell_name(p1) + " are both " + plural(v0) + ", so " + cell_name(p2) +
                                     " must be a " + word(other(v0)) + " (never three in a row).",
                                 {p0, p1}, {{p2, other(v0)}}, false};
            if (v1 != BOTH && v1 == v2 && v0 == BOTH)
                return Deduction{"pair", cell_name(p1) + " and " + cell_name(p2) + " are both " + plural(v1) + ", so " + cell_name(p0) +
                                     " must be a " + word(other(v1)) + " (never three in a row).",
                                 {p1, p2}, {{p0, other(v1)}}, false};
            if (v0 != BOTH && v0 == v2 && v1 == BOTH)
                return Deduction{"gap", cell_name(p0) + " and " + cell_name(p2) + " are both " + plural(v0) + ", so the gap " + cell_name(p1) +
                                     " must be a " + word(other(v0)) + " (never three in a row).",
                                 {p0, p2}, {{p1, other(v0)}}, false};
        }
    }
    return std::nullopt;
}

std::optional<Deduction> rule_counts(const TangoBoard& b, const Domains& d) {
    const int n = b.size(), half = n / 2;
    for (int li = 0; li < 2 * n; ++li) {
        Line L{n, li};
        int cnt[3] = {0, 0, 0};
        std::vector<Position> open, known;
        for (int k = 0; k < n; ++k) {
            uint8_t v = at(d, L.at(k));
            if (v == BOTH) open.push_back(L.at(k)); else { ++cnt[v]; known.push_back(L.at(k)); }
        }
        for (uint8_t v : {S, M}) {
            if (cnt[v] > half)
                return Deduction{"count", L.name() + " has " + std::to_string(cnt[v]) + " " + plural(v) + ", more than " + std::to_string(half) + ".",
                                 known, {}, true};
        }
        if (open.empty()) continue;
        for (uint8_t v : {S, M}) {
            if (cnt[v] == half) {
                Deduction D{"count", L.name() + " already has its " + std::to_string(half) + " " + plural(v) + ", so its remaining cells are " +
                                         plural(other(v)) + ".", known, {}, false};
                for (auto p : open) D.sets.push_back({p, other(v)});
                return D;
            }
        }
    }
    return std::nullopt;
}

// Counts that take open sign pairs into account: an open 'x' pair supplies one sun and one moon;
// an open '=' pair supplies two of the same symbol.
std::optional<Deduction> rule_sign_counts(const TangoBoard& b, const Domains& d) {
    const int n = b.size(), half = n / 2;
    for (int li = 0; li < 2 * n; ++li) {
        Line L{n, li};
        int cnt[3] = {0, 0, 0};
        for (int k = 0; k < n; ++k) { uint8_t v = at(d, L.at(k)); if (v != BOTH) ++cnt[v]; }
        // disjoint open 'x' pairs, greedily left to right (maximum for adjacent pairs on a path)
        std::vector<int> xpair_start, eqpair_start;
        std::vector<bool> in_x(n, false);
        for (int k = 0; k + 1 < n; ++k) {
            if (at(d, L.at(k)) != BOTH || at(d, L.at(k + 1)) != BOTH) continue;
            EdgeClue e = sign_in_line(b, L, k);
            if (e == EdgeClue::OPPOSITE && !in_x[k]) { xpair_start.push_back(k); in_x[k] = in_x[k + 1] = true; }
        }
        for (int k = 0; k + 1 < n; ++k) {
            if (at(d, L.at(k)) == BOTH && at(d, L.at(k + 1)) == BOTH && sign_in_line(b, L, k) == EdgeClue::SAME && !in_x[k] && !in_x[k + 1])
                eqpair_start.push_back(k);
        }
        const int p = static_cast<int>(xpair_start.size());
        auto pairs_text = [&]() {
            std::string s;
            for (size_t i = 0; i < xpair_start.size(); ++i) {
                if (i) s += ", ";
                s += cell_name(L.at(xpair_start[i])) + "x" + cell_name(L.at(xpair_start[i] + 1));
            }
            return s;
        };
        for (uint8_t v : {S, M}) {
            if (p > 0 && cnt[v] + p == half) {
                Deduction D{"x-pair-count", "", {}, {}, false};
                for (int k = 0; k < n; ++k)
                    if (at(d, L.at(k)) == BOTH && !in_x[k]) D.sets.push_back({L.at(k), other(v)});
                if (D.sets.empty()) continue;
                D.explanation = L.name() + " has " + std::to_string(cnt[v]) + " " + plural(v) + " and its open 'x' pair" + (p > 1 ? "s " : " ") +
                                pairs_text() + " must supply " + std::to_string(p) + " more (one of each symbol per pair), which makes " +
                                std::to_string(half) + "; every other open cell is a " + word(other(v)) + ".";
                for (int s : xpair_start) { D.involved.push_back(L.at(s)); D.involved.push_back(L.at(s + 1)); }
                return D;
            }
            for (int s : eqpair_start) {
                if (cnt[v] + p + 2 > half) {
                    Deduction D{"equal-pair-count", "", {L.at(s), L.at(s + 1)}, {{L.at(s), other(v)}, {L.at(s + 1), other(v)}}, false};
                    D.explanation = L.name() + " has " + std::to_string(cnt[v]) + " " + plural(v) +
                                    (p ? " plus " + std::to_string(p) + " from open 'x' pair" + (p > 1 ? "s" : "") : std::string()) +
                                    "; the open '=' pair " + cell_name(L.at(s)) + "=" + cell_name(L.at(s + 1)) + " cannot be two " + plural(v) +
                                    " without exceeding " + std::to_string(half) + ", so both are " + plural(other(v)) + ".";
                    return D;
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<Deduction> rule_line_patterns(const TangoBoard& b, const Domains& d, ReasoningStats* work) {
    const int n = b.size();
    for (int li = 0; li < 2 * n; ++li) {
        Line L{n, li};
        bool has_open = false;
        for (int k = 0; k < n; ++k) has_open |= at(d, L.at(k)) == BOTH;
        if (!has_open) continue;
        Enumeration e = enumerate_line(b, d, L, kMaxLinePatterns);
        if (work) work->work["line_patterns_enumerated"] += e.count;
        if (e.capped) { if (work) ++work->work["line_pattern_lines_skipped_over_cap"]; continue; }
        if (e.count == 0)
            return Deduction{"line-patterns", L.name() + " (" + line_text(b, d, L) + ") has no legal arrangement.", {}, {}, true};
        Deduction D{"line-patterns", "", {}, {}, false};
        std::string why;
        for (int k = 0; k < n; ++k) {
            Position p = L.at(k);
            if (at(d, p) != BOTH || e.seen[k] == BOTH) continue;
            uint8_t v = e.seen[k];
            D.sets.push_back({p, v});
            // why the other symbol fails: does relaxing no-three bring completions back?
            Enumeration alt = enumerate_line(b, d, L, kMaxLinePatterns, k, other(v), true);
            if (work) work->work["line_patterns_enumerated"] += alt.count;
            why += " A " + std::string(word(other(v))) + " at " + cell_name(p) +
                   (alt.count > 0 ? " would force three in a row." : " would break the count or a sign.");
        }
        if (D.sets.empty()) continue;
        for (int k = 0; k < n; ++k) if (at(d, L.at(k)) != BOTH) D.involved.push_back(L.at(k));
        std::string list;
        for (size_t i = 0; i < e.examples.size(); ++i) list += (i ? ", " : "") + e.examples[i];
        const std::string rules = " given its symbols, signs, " + std::to_string(n / 2) + "-and-" + std::to_string(n / 2) + " count and the no-three rule";
        const std::string fixed = std::to_string(D.sets.size()) + " open cell" + (D.sets.size() == 1 ? "" : "s");
        D.explanation = e.count == 1
            ? L.name() + " (" + line_text(b, d, L) + ") has only one legal arrangement" + rules + ": " + list + ". It fixes " + fixed + "." + why
            : L.name() + " (" + line_text(b, d, L) + ") has " + std::to_string(e.count) + " legal arrangements" + rules +
                  (e.count <= 4 ? ": " + list : std::string()) + ". All of them agree on " + fixed + "." + why;
        return D;
    }
    return std::nullopt;
}

} // namespace

Domains initial_domains(const TangoBoard& board) {
    const int n = board.size();
    Domains d(n, std::vector<uint8_t>(n, BOTH));
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) {
            auto s = board.get_symbol(r, c);
            if (s == TangoSymbol::SUN) d[r][c] = S;
            else if (s == TangoSymbol::MOON) d[r][c] = M;
        }
    return d;
}

std::optional<Deduction> next_deduction(const TangoBoard& board, const Domains& d, ReasoningStats* work) {
    if (auto x = rule_signs(board, d)) return x;
    if (auto x = rule_no_three(board, d)) return x;
    if (auto x = rule_counts(board, d)) return x;
    if (auto x = rule_sign_counts(board, d)) return x;
    return rule_line_patterns(board, d, work);
}

std::string render(const TangoBoard& board, const Domains& d) {
    const int n = board.size();
    std::ostringstream os;
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            os << (d[r][c] == S ? 'S' : d[r][c] == M ? 'M' : '.');
            if (c + 1 < n) os << sign_char(board.get_horizontal_clue(r, c));
        }
        os << "\n";
        if (r + 1 < n) {
            std::string vline;
            bool any = false;
            for (int c = 0; c < n; ++c) {
                char sc = sign_char(board.get_vertical_clue(r, c));
                any |= sc != ' ';
                vline += sc;
                if (c + 1 < n) vline += ' ';
            }
            if (any) {
                while (!vline.empty() && vline.back() == ' ') vline.pop_back();
                os << vline << "\n";
            }
        }
    }
    return os.str();
}

} // namespace tango_logic

namespace {

using namespace tango_logic;

struct Ctx {
    const TangoBoard& board;
    const ReasoningOptions& opt;
    ReasoningReport& rep;
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point first_solution_at;
    SearchPhase phase{SearchPhase::SOLVE};
    std::vector<std::vector<std::string>> solutions;
    size_t max_solutions{1};
    bool stop{false};

    void log(ReasoningStep s) {
        s.phase = phase;
        if (rep.steps.size() < opt.max_logged_steps) rep.steps.push_back(std::move(s));
    }
    size_t& nodes() { return phase == SearchPhase::SOLVE ? rep.stats.solution_nodes : rep.stats.uniqueness_nodes; }
    bool over_budget() {
        size_t total = rep.stats.solution_nodes + rep.stats.uniqueness_nodes;
        if (opt.max_nodes && total >= opt.max_nodes) return true;
        if (opt.max_time_ms > 0) {
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            if (ms >= opt.max_time_ms) return true;
        }
        return false;
    }
};

enum class DeduceResult { OK, CONTRADICTION };

DeduceResult deduce_all(Ctx& ctx, Domains& d, int depth, bool single = false) {
    while (true) {
        auto D = next_deduction(ctx.board, d, &ctx.rep.stats);
        if (!D) return DeduceResult::OK;
        if (D->contradiction) {
            ReasoningStep s{StepKind::CONTRADICTION, SearchPhase::SOLVE, depth, D->rule, D->explanation, D->involved, {}, ""};
            ctx.log(std::move(s));
            return DeduceResult::CONTRADICTION;
        }
        ReasoningStep s{StepKind::DEDUCTION, SearchPhase::SOLVE, depth, D->rule, D->explanation, D->involved, {}, ""};
        for (auto& [p, v] : D->sets) {
            d[p.row][p.col] = v;
            s.changes.push_back({p, v == S ? "S" : "M"});
        }
        ++ctx.rep.stats.deductions;
        ++ctx.rep.stats.rule_counts[D->rule];
        ctx.log(std::move(s));
        if (single) return DeduceResult::OK;
    }
}

bool complete(const Domains& d) {
    for (auto& row : d) for (auto v : row) if (v == BOTH) return false;
    return true;
}

std::vector<std::string> as_grid(const Domains& d) {
    std::vector<std::string> g;
    for (auto& row : d) {
        std::string s;
        for (auto v : row) s += v == S ? 'S' : 'M';
        g.push_back(s);
    }
    return g;
}

// Branch on the open cell of the line with the fewest legal arrangements; try the value most of them use first.
bool choose_branch(const TangoBoard& b, const Domains& d, Position& cell, uint8_t order[2], std::string& why, ReasoningStats& st) {
    const int n = b.size();
    size_t best = SIZE_MAX;
    for (int li = 0; li < 2 * n; ++li) {
        Line L{n, li};
        bool open = false;
        for (int k = 0; k < n; ++k) open |= at(d, L.at(k)) == BOTH;
        if (!open) continue;
        Enumeration e = enumerate_line(b, d, L, kMaxLinePatterns);
        st.work["line_patterns_enumerated"] += e.count;
        if (e.capped || e.count >= best) continue;
        for (int k = 0; k < n; ++k) {
            if (at(d, L.at(k)) != BOTH || e.seen[k] != BOTH) continue;
            best = e.count;
            cell = L.at(k);
            size_t suns = e.sun_count[k];
            order[0] = suns * 2 >= e.count ? S : M;
            order[1] = other(order[0]);
            why = L.name() + " (" + line_text(b, d, L) + ") has the fewest legal arrangements of any line (" + std::to_string(e.count) + "); " +
                  std::to_string(order[0] == S ? suns : e.count - suns) + " of them put a " + word(order[0]) + " at " + cell_name(cell) + ".";
            break;
        }
    }
    if (best != SIZE_MAX) return true;
    for (int r = 0; r < n; ++r)  // every line over the pattern cap: plain first open cell
        for (int c = 0; c < n; ++c)
            if (d[r][c] == BOTH) { cell = {r, c}; order[0] = S; order[1] = M; why = "first open cell (all lines exceed the pattern cap)."; return true; }
    return false;
}

void search(Ctx& ctx, Domains d, int depth) {
    if (ctx.stop) return;
    if (ctx.over_budget()) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return; }
    ++ctx.nodes();
    if (deduce_all(ctx, d, depth) == DeduceResult::CONTRADICTION) {
        if (depth > 0) ++ctx.rep.stats.failed_branches;
        return;
    }
    if (complete(d)) {
        auto g = as_grid(d);
        auto v = validate_tango_solution(ctx.board, g);
        if (!v.is_valid) {
            ctx.log({StepKind::CONTRADICTION, SearchPhase::SOLVE, depth, "validator", v.error_message, {}, {}, ""});
            if (depth > 0) ++ctx.rep.stats.failed_branches;
            return;
        }
        ctx.log({StepKind::SOLUTION, SearchPhase::SOLVE, depth, "", "every cell is decided and the independent validator accepts the grid.", {}, {}, render(ctx.board, d)});
        ctx.solutions.push_back(g);
        if (ctx.solutions.size() == 1) {
            ctx.first_solution_at = std::chrono::steady_clock::now();
            ctx.phase = SearchPhase::VERIFY_UNIQUE;
        }
        if (ctx.solutions.size() >= ctx.max_solutions) ctx.stop = true;
        return;
    }
    Position cell{};
    uint8_t order[2];
    std::string why;
    if (!choose_branch(ctx.board, d, cell, order, why, ctx.rep.stats)) return;
    for (int i = 0; i < 2 && !ctx.stop; ++i) {
        ReasoningStep s{StepKind::BRANCH, SearchPhase::SOLVE, depth,
                        i == 0 ? "guess" : "guess (other value)",
                        i == 0 ? "no rule applies. " + why
                               : (ctx.phase == SearchPhase::VERIFY_UNIQUE
                                      ? "a solution is known; to prove it unique, try the other value at " + cell_name(cell) + "."
                                      : "the first value led to a dead end; try the other value at " + cell_name(cell) + "."),
                        {cell}, {{cell, order[i] == S ? "S" : "M"}}, i == 0 ? render(ctx.board, d) : ""};
        ctx.log(std::move(s));
        ++ctx.rep.stats.branches;
        Domains next = d;
        next[cell.row][cell.col] = order[i];
        search(ctx, std::move(next), depth + 1);
    }
}

} // namespace

ReasoningReport reason_tango(const TangoBoard& board, const ReasoningOptions& opt) {
    ReasoningReport rep;
    Ctx ctx{board, opt, rep, std::chrono::steady_clock::now(), {}, SearchPhase::SOLVE, {}, opt.check_unique ? size_t{2} : size_t{1}, false};
    Domains d = initial_domains(board);
    auto elapsed = [&](auto from) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };

    if (opt.mode == ReasoningMode::HINT) {
        ctx.nodes() = 1;
        auto r = deduce_all(ctx, d, 0, true);
        rep.outcome = rep.steps.empty() ? ReasoningOutcome::NO_HINT : (r == DeduceResult::CONTRADICTION ? ReasoningOutcome::NO_SOLUTION : ReasoningOutcome::HINT);
        rep.final_board = render(board, d);
        rep.stats.solution_ms = elapsed(ctx.start);
        return rep;
    }
    if (opt.mode == ReasoningMode::DEDUCE) {
        ctx.nodes() = 1;
        auto r = deduce_all(ctx, d, 0);
        rep.final_board = render(board, d);
        if (r == DeduceResult::CONTRADICTION) rep.outcome = ReasoningOutcome::NO_SOLUTION;
        else if (complete(d)) {
            auto v = validate_tango_solution(board, as_grid(d));
            rep.outcome = v.is_valid ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::NO_SOLUTION;
        } else {
            rep.outcome = ReasoningOutcome::STALLED;
            rep.steps.push_back({StepKind::STALL, SearchPhase::SOLVE, 0, "", "no rule can prove another move; the board below is partial.", {}, {}, rep.final_board});
        }
        rep.stats.solution_ms = elapsed(ctx.start);
        return rep;
    }

    search(ctx, d, 0);
    if (ctx.solutions.empty()) {
        rep.stats.solution_ms = elapsed(ctx.start);
        rep.outcome = rep.stats.budget_exhausted ? ReasoningOutcome::INCONCLUSIVE : ReasoningOutcome::NO_SOLUTION;
        return rep;
    }
    rep.stats.solution_ms = std::chrono::duration<double, std::milli>(ctx.first_solution_at - ctx.start).count();
    rep.stats.uniqueness_ms = elapsed(ctx.first_solution_at);
    for (auto& g : ctx.solutions) {
        std::string t;
        for (auto& row : g) { for (size_t i = 0; i < row.size(); ++i) { t += row[i]; if (i + 1 < row.size()) t += ' '; } t += '\n'; }
        rep.solutions_text.push_back(t);
    }
    rep.final_board = rep.solutions_text[0];
    if (ctx.solutions.size() >= 2) rep.outcome = ReasoningOutcome::MULTIPLE_SOLUTIONS;
    else if (rep.stats.budget_exhausted && opt.check_unique) rep.outcome = ReasoningOutcome::INCONCLUSIVE;
    else rep.outcome = rep.stats.branches == 0 ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::SOLVED_WITH_FALLBACK;
    return rep;
}

} // namespace solvedin
