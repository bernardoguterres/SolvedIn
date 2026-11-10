#include "solvedin/queens_logic.hpp"
#include "solvedin/queens_validator.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdint>
#include <sstream>

namespace solvedin {

namespace queens_logic {

namespace {

// Group g: rows [0, n), columns [n, 2n), regions [2n, 3n) by region index.
struct Groups {
    int n;
    std::vector<std::vector<int>> open;  // open cell indices per group
    std::vector<int> queen;              // queen cell index per group, or -1
};

std::string region_label(const Board& b, int region_index) {
    for (int r = 0; r < b.size(); ++r)
        for (int c = 0; c < b.size(); ++c)
            if (b.get_region_index(r, c) == region_index) return "region " + std::to_string(b.get_region(r, c));
    return "region ?";
}

std::string group_name(const Board& b, int g) {
    const int n = b.size();
    if (g < n) return "row " + std::to_string(g);
    if (g < 2 * n) return "column " + std::to_string(g - n);
    return region_label(b, g - 2 * n);
}

std::string cap(std::string s) { if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0]))); return s; }

Position pos(int n, int idx) { return {idx / n, idx % n}; }

Groups groups_of(const Board& b, const State& s) {
    const int n = s.n;
    Groups G{n, std::vector<std::vector<int>>(3 * n), std::vector<int>(3 * n, -1)};
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) {
            const int idx = r * n + c, reg = 2 * n + b.get_region_index(r, c);
            const Cell v = s.at(r, c);
            for (int g : {r, n + c, reg}) {
                if (v == Cell::OPEN) G.open[g].push_back(idx);
                else if (v == Cell::QUEEN) G.queen[g] = idx;
            }
        }
    return G;
}

// Does a queen at cell a attack cell b (same row, column, region, or touching)?
bool attacks(const Board& b, int n, int a, int c) {
    if (a == c) return false;
    const int ar = a / n, ac = a % n, br = c / n, bc = c % n;
    if (ar == br || ac == bc) return true;
    if (std::abs(ar - br) <= 1 && std::abs(ac - bc) <= 1) return true;
    return b.get_region_index(ar, ac) == b.get_region_index(br, bc);
}

// Cells a queen at `a` attacks: its row, column, region and the (up to 8) touching cells. May repeat cells.
template <class F>
void for_each_attacked(const State& s, const Board& b, int a, F&& f) {
    const int n = s.n, r = a / n, c = a % n;
    for (int k = 0; k < n; ++k) { if (k != c) f(r * n + k); if (k != r) f(k * n + c); }
    for (int dr = -1; dr <= 1; ++dr)
        for (int dc = -1; dc <= 1; ++dc) {
            int rr = r + dr, cc = c + dc;
            if ((dr || dc) && rr >= 0 && cc >= 0 && rr < n && cc < n) f(rr * n + cc);
        }
    for (int i : s.region_cells[b.get_region_index(r, c)]) if (i != a) f(i);
}

std::string cells_text(int n, const std::vector<int>& v, size_t max = 6) {
    std::string s;
    for (size_t i = 0; i < v.size() && i < max; ++i) s += (i ? ", " : "") + cell_name(pos(n, v[i]));
    if (v.size() > max) s += ", ...";
    return s;
}

std::optional<Deduction> rule_queen_attacks(const Board& b, const State& s) {
    const int n = s.n;
    for (int q = 0; q < n * n; ++q) {
        if (s.cells[q] != Cell::QUEEN) continue;
        Deduction D{"queen-attacks", "", {pos(n, q)}, {}, {}, false};
        std::vector<char> seen(static_cast<size_t>(n * n), 0);
        for_each_attacked(s, b, q, [&](int c) {
            if (!seen[c] && s.cells[c] == Cell::OPEN) { seen[c] = 1; D.eliminated.push_back(pos(n, c)); }
        });
        if (D.eliminated.empty()) continue;
        std::sort(D.eliminated.begin(), D.eliminated.end(), [](Position x, Position y) { return x.row != y.row ? x.row < y.row : x.col < y.col; });
        D.explanation = "The queen at " + cell_name(pos(n, q)) + " rules out the rest of its row, column and " +
                        region_label(b, b.get_region_index(q / n, q % n)) + ", and the cells touching it (diagonals included).";
        return D;
    }
    return std::nullopt;
}

std::optional<Deduction> rule_empty_or_lone(const Board& b, const Groups& G) {
    const int n = G.n;
    for (int g = 0; g < 3 * n; ++g)
        if (G.queen[g] < 0 && G.open[g].empty())
            return Deduction{"no-candidate", cap(group_name(b, g)) + " has no cell left for its queen.", {}, {}, {}, true};
    for (int g = 0; g < 3 * n; ++g) {
        if (G.queen[g] >= 0 || G.open[g].size() != 1) continue;
        const Position p = pos(n, G.open[g][0]);
        return Deduction{"lone-candidate", cap(group_name(b, g)) + " has only one cell left that can hold a queen: " + cell_name(p) + ".",
                         {p}, {p}, {}, false};
    }
    return std::nullopt;
}

// k = 1 locks in both directions: a region confined to one line, and a line confined to one region.
std::optional<Deduction> rule_single_locks(const Board& b, [[maybe_unused]] const State& s, const Groups& G) {
    const int n = G.n;
    for (int reg = 2 * n; reg < 3 * n; ++reg) {
        if (G.queen[reg] >= 0 || G.open[reg].empty()) continue;
        for (int dir = 0; dir < 2; ++dir) {  // 0 = row, 1 = column
            const int line = dir ? G.open[reg][0] % n : G.open[reg][0] / n;
            bool same = std::all_of(G.open[reg].begin(), G.open[reg].end(), [&](int i) { return (dir ? i % n : i / n) == line; });
            if (!same) continue;
            Deduction D{"region-line-lock", "", {}, {}, {}, false};
            for (int i : G.open[dir ? n + line : line])
                if (b.get_region_index(i / n, i % n) != reg - 2 * n) D.eliminated.push_back(pos(n, i));
            if (D.eliminated.empty()) continue;
            for (int i : G.open[reg]) D.involved.push_back(pos(n, i));
            const std::string ln = (dir ? "column " : "row ") + std::to_string(line);
            D.explanation = "Every remaining cell of " + group_name(b, reg) + " (" + cells_text(n, G.open[reg]) + ") lies in " + ln + ", so " +
                            group_name(b, reg) + "'s queen will be in " + ln + ": the other cells of " + ln + " are eliminated.";
            return D;
        }
    }
    for (int g = 0; g < 2 * n; ++g) {
        if (G.queen[g] >= 0 || G.open[g].empty()) continue;
        const int reg = b.get_region_index(G.open[g][0] / n, G.open[g][0] % n);
        bool same = std::all_of(G.open[g].begin(), G.open[g].end(), [&](int i) { return b.get_region_index(i / n, i % n) == reg; });
        if (!same) continue;
        Deduction D{"line-region-lock", "", {}, {}, {}, false};
        for (int i : G.open[2 * n + reg]) {
            const int line = g < n ? i / n : i % n;
            if (line != (g < n ? g : g - n)) D.eliminated.push_back(pos(n, i));
        }
        if (D.eliminated.empty()) continue;
        for (int i : G.open[g]) D.involved.push_back(pos(n, i));
        D.explanation = "Every remaining cell of " + group_name(b, g) + " (" + cells_text(n, G.open[g]) + ") is in " + region_label(b, reg) +
                        ", so that region's queen sits in " + group_name(b, g) + ": its cells outside " + group_name(b, g) + " are eliminated.";
        return D;
    }
    return std::nullopt;
}

// A candidate that would attack every remaining cell of another group cannot hold a queen.
std::optional<Deduction> rule_would_empty_group(const Board& b, const State& s, const Groups& G) {
    const int n = G.n;
    for (int g = 0; g < 3 * n; ++g) {
        const auto& cand = G.open[g];
        if (G.queen[g] >= 0 || cand.size() < 2 || cand.size() > static_cast<size_t>(kMaxEmptyGroupCandidates)) continue;
        Deduction D{"would-empty-group", "", {}, {}, {}, false};
        // attack is symmetric: only cells attacking the first candidate can attack them all
        std::vector<char> seen(static_cast<size_t>(n * n), 0);
        for_each_attacked(s, b, cand[0], [&](int x) {
            if (seen[x] || s.cells[x] != Cell::OPEN || std::find(cand.begin(), cand.end(), x) != cand.end()) return;
            seen[x] = 1;
            bool all = true;
            for (int c : cand) if (!attacks(b, n, x, c)) { all = false; break; }
            if (all) D.eliminated.push_back(pos(n, x));
        });
        std::sort(D.eliminated.begin(), D.eliminated.end(), [](Position x, Position y) { return x.row != y.row ? x.row < y.row : x.col < y.col; });
        if (D.eliminated.empty()) continue;
        for (int c : cand) D.involved.push_back(pos(n, c));
        D.explanation = "A queen at " + (D.eliminated.size() == 1 ? cell_name(D.eliminated[0]) : std::string("any of these cells")) +
                        " would attack every remaining cell of " + group_name(b, g) + " (" + cells_text(n, cand) +
                        "), leaving it with no queen, so " + (D.eliminated.size() == 1 ? "it is" : "they are") + " eliminated.";
        return D;
    }
    return std::nullopt;
}

// k regions confined to k lines (or k lines confined to k regions), k = 2, 3.
std::optional<Deduction> rule_group_locks(const Board& b, [[maybe_unused]] const State& s, const Groups& G) {
    const int n = G.n;
    if (n > kMaxLockBoard2) return std::nullopt;
    auto line_mask = [&](int g, int dir) {  // which rows (dir 0) or columns (dir 1) a region's open cells use
        uint64_t m = 0;
        for (int i : G.open[g]) m |= uint64_t{1} << (dir ? i % n : i / n);
        return m;
    };
    auto region_mask = [&](int g) {
        uint64_t m = 0;
        for (int i : G.open[g]) m |= uint64_t{1} << b.get_region_index(i / n, i % n);
        return m;
    };
    auto mask_text = [&](uint64_t m, const char* what) {
        std::string t;
        for (int i = 0; i < n; ++i) if (m >> i & 1) t += (t.empty() ? std::string(what) + " " : std::string(" and ")) + std::to_string(i);
        return t;
    };
    for (int k = 2; k <= (n <= kMaxLockBoard3 ? 3 : 2); ++k) {
        // regions -> lines
        std::vector<int> live;
        for (int reg = 2 * n; reg < 3 * n; ++reg) if (G.queen[reg] < 0 && !G.open[reg].empty()) live.push_back(reg);
        for (int dir = 0; dir < 2; ++dir) {
            std::vector<int> idx(k);
            auto try_combo = [&](const std::vector<int>& combo) -> std::optional<Deduction> {
                uint64_t u = 0, regs = 0;
                for (int g : combo) { u |= line_mask(g, dir); regs |= uint64_t{1} << (g - 2 * n); }
                if (std::popcount(u) != k) return std::nullopt;
                Deduction D{"region-group-lock", "", {}, {}, {}, false};
                for (int line = 0; line < n; ++line) {
                    if (!(u >> line & 1)) continue;
                    for (int i : G.open[dir ? n + line : line])
                        if (!(regs >> b.get_region_index(i / n, i % n) & 1)) D.eliminated.push_back(pos(n, i));
                }
                if (D.eliminated.empty()) return std::nullopt;
                std::string names;
                for (size_t j = 0; j < combo.size(); ++j) names += (j ? (j + 1 == combo.size() ? " and " : ", ") : "") + group_name(b, combo[j]);
                for (int g : combo) for (int i : G.open[g]) D.involved.push_back(pos(n, i));
                const std::string lines = mask_text(u, dir ? "columns" : "rows");
                D.explanation = cap(names) + " have all their remaining cells in " + lines + ", so those " + std::to_string(k) +
                                " lines' queens belong to these " + std::to_string(k) + " regions: other regions' cells in " + lines + " are eliminated.";
                return D;
            };
            // combinations of k live regions
            const int L = static_cast<int>(live.size());
            if (L < k) continue;
            std::vector<int> c(k);
            for (int i = 0; i < k; ++i) c[i] = i;
            while (true) {
                std::vector<int> combo;
                for (int i : c) combo.push_back(live[i]);
                if (auto d = try_combo(combo)) return d;
                int i = k - 1;
                while (i >= 0 && c[i] == L - k + i) --i;
                if (i < 0) break;
                ++c[i];
                for (int j = i + 1; j < k; ++j) c[j] = c[j - 1] + 1;
            }
        }
        // lines -> regions
        for (int dir = 0; dir < 2; ++dir) {
            std::vector<int> lines;
            for (int l = 0; l < n; ++l) { int g = dir ? n + l : l; if (G.queen[g] < 0 && !G.open[g].empty()) lines.push_back(g); }
            const int L = static_cast<int>(lines.size());
            if (L < k) continue;
            std::vector<int> c(k);
            for (int i = 0; i < k; ++i) c[i] = i;
            while (true) {
                uint64_t regs = 0, linemask = 0;
                for (int i : c) { regs |= region_mask(lines[i]); linemask |= uint64_t{1} << (dir ? lines[i] - n : lines[i]); }
                if (std::popcount(regs) == k) {
                    Deduction D{"line-group-lock", "", {}, {}, {}, false};
                    for (int reg = 0; reg < n; ++reg) {
                        if (!(regs >> reg & 1)) continue;
                        for (int i : G.open[2 * n + reg])
                            if (!(linemask >> (dir ? i % n : i / n) & 1)) D.eliminated.push_back(pos(n, i));
                    }
                    if (!D.eliminated.empty()) {
                        std::string regnames;
                        int cnt = 0;
                        for (int reg = 0; reg < n; ++reg)
                            if (regs >> reg & 1) regnames += (cnt++ ? " and " : "") + region_label(b, reg);
                        for (int i : c) for (int x : G.open[lines[i]]) D.involved.push_back(pos(n, x));
                        const std::string ln = mask_text(linemask, dir ? "columns" : "rows");
                        D.explanation = cap(ln) + " have all their remaining cells in " + regnames + ", so those regions' queens are in " + ln +
                                        ": their cells outside " + ln + " are eliminated.";
                        return D;
                    }
                }
                int i = k - 1;
                while (i >= 0 && c[i] == L - k + i) --i;
                if (i < 0) break;
                ++c[i];
                for (int j = i + 1; j < k; ++j) c[j] = c[j - 1] + 1;
            }
        }
    }
    return std::nullopt;
}

} // namespace

State initial_state(const Board& board) {
    State s;
    s.n = board.size();
    s.cells.assign(static_cast<size_t>(s.n * s.n), Cell::OPEN);
    s.region_cells.assign(static_cast<size_t>(s.n), {});
    for (int r = 0; r < s.n; ++r)
        for (int c = 0; c < s.n; ++c) s.region_cells[static_cast<size_t>(board.get_region_index(r, c))].push_back(r * s.n + c);
    return s;
}

std::optional<Deduction> next_deduction(const Board& b, const State& s) {
    if (auto d = rule_queen_attacks(b, s)) return d;
    Groups G = groups_of(b, s);
    if (auto d = rule_empty_or_lone(b, G)) return d;
    if (auto d = rule_single_locks(b, s, G)) return d;
    if (auto d = rule_would_empty_group(b, s, G)) return d;
    return rule_group_locks(b, s, G);
}

void apply(State& s, const Deduction& d) {
    for (auto p : d.queens) s.cells[static_cast<size_t>(p.row * s.n + p.col)] = Cell::QUEEN;
    for (auto p : d.eliminated) s.cells[static_cast<size_t>(p.row * s.n + p.col)] = Cell::OUT;
}

std::string render(const Board& b, const State& s) {
    // open cells show their region id (base 36, lower case), eliminated cells '.', queens 'Q'
    const char* digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    std::ostringstream os;
    for (int r = 0; r < s.n; ++r) {
        for (int c = 0; c < s.n; ++c) {
            if (c) os << ' ';
            Cell v = s.at(r, c);
            int reg = b.get_region(r, c);
            os << (v == Cell::QUEEN ? 'Q' : v == Cell::OUT ? '.' : (reg >= 0 && reg < 36 ? digits[reg] : '#'));
        }
        os << '\n';
    }
    return os.str();
}

} // namespace queens_logic

namespace {

using namespace queens_logic;

struct Ctx {
    const Board& board;
    const ReasoningOptions& opt;
    ReasoningReport& rep;
    std::chrono::steady_clock::time_point start, first_solution_at;
    SearchPhase phase{SearchPhase::SOLVE};
    std::vector<std::vector<Position>> solutions;
    size_t max_solutions{2};
    bool stop{false};
    void log(ReasoningStep s) { s.phase = phase; if (rep.steps.size() < opt.max_logged_steps) rep.steps.push_back(std::move(s)); }
    size_t& nodes() { return phase == SearchPhase::SOLVE ? rep.stats.solution_nodes : rep.stats.uniqueness_nodes; }
    bool over_budget() const {
        if (opt.max_nodes && rep.stats.solution_nodes + rep.stats.uniqueness_nodes >= opt.max_nodes) return true;
        return opt.max_time_ms > 0 &&
               std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= opt.max_time_ms;
    }
};

bool deduce_all(Ctx& ctx, State& s, int depth, bool single = false) {
    while (true) {
        if (ctx.over_budget()) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return true; }
        auto D = next_deduction(ctx.board, s);
        if (!D) return true;
        ReasoningStep st{D->contradiction ? StepKind::CONTRADICTION : StepKind::DEDUCTION, SearchPhase::SOLVE, depth, D->rule, D->explanation, D->involved, {}, ""};
        if (D->contradiction) { ctx.log(std::move(st)); return false; }
        for (auto p : D->queens) st.changes.push_back({p, "Q"});
        for (auto p : D->eliminated) st.changes.push_back({p, "x"});
        apply(s, *D);
        ++ctx.rep.stats.deductions;
        ++ctx.rep.stats.rule_counts[D->rule];
        ctx.log(std::move(st));
        if (single) return true;
    }
}

std::vector<Position> queens_of(const State& s) {
    std::vector<Position> q;
    for (int i = 0; i < s.n * s.n; ++i) if (s.cells[i] == Cell::QUEEN) q.push_back({i / s.n, i % s.n});
    return q;
}

void search(Ctx& ctx, State s, int depth) {
    if (ctx.stop) return;
    if (ctx.over_budget()) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return; }
    ++ctx.nodes();
    if (!deduce_all(ctx, s, depth)) { if (depth > 0) ++ctx.rep.stats.failed_branches; return; }
    if (ctx.stop) return;
    auto q = queens_of(s);
    if (static_cast<int>(q.size()) == s.n) {
        auto v = validate_queens_solution(ctx.board, q);
        if (!v.is_valid) {
            ctx.log({StepKind::CONTRADICTION, SearchPhase::SOLVE, depth, "validator", v.error_message, {}, {}, ""});
            if (depth > 0) ++ctx.rep.stats.failed_branches;
            return;
        }
        ctx.log({StepKind::SOLUTION, SearchPhase::SOLVE, depth, "", "all queens are placed and the independent validator accepts them.", {}, {}, render(ctx.board, s)});
        ctx.solutions.push_back(q);
        if (ctx.solutions.size() == 1) { ctx.first_solution_at = std::chrono::steady_clock::now(); ctx.phase = SearchPhase::VERIFY_UNIQUE; }
        if (ctx.solutions.size() >= ctx.max_solutions) ctx.stop = true;
        return;
    }
    // Most constrained group: fewest candidates (ties: rows, then columns, then regions).
    const int n = s.n;
    int best = -1;
    size_t best_size = SIZE_MAX;
    std::vector<std::vector<int>> open(3 * n);
    std::vector<bool> filled(3 * n, false);
    for (int r = 0; r < n; ++r)
        for (int c = 0; c < n; ++c) {
            int idx = r * n + c, reg = 2 * n + ctx.board.get_region_index(r, c);
            for (int g : {r, n + c, reg}) {
                if (s.cells[idx] == Cell::OPEN) open[g].push_back(idx);
                if (s.cells[idx] == Cell::QUEEN) filled[g] = true;
            }
        }
    // Same group choice as the bitset solver: fewest candidates, or (boards >= 64 whose regions are all at least
    // half a line) fewest candidates relative to the group's original size.
    size_t min_reg = SIZE_MAX;
    for (auto& rc : s.region_cells) min_reg = std::min(min_reg, rc.size());
    const bool use_ratio = n >= 64 && min_reg >= static_cast<size_t>(n) / 2;
    const double w_other = (n <= 32 || use_ratio) ? 0.0 : 1.0;
    double best_ratio = 1e18;
    for (int g = 0; g < 3 * n; ++g) {
        if (filled[g]) continue;
        if (open[g].empty()) return;
        const double denom = g < 2 * n ? n : static_cast<double>(s.region_cells[static_cast<size_t>(g - 2 * n)].size());
        const double key = use_ratio ? open[g].size() / denom : static_cast<double>(open[g].size());
        if (key < best_ratio) { best_ratio = key; best_size = open[g].size(); best = g; }
    }
    if (best < 0 || best_size == 0) return;
    auto gname = [&](int g) {
        if (g < n) return "row " + std::to_string(g);
        if (g < 2 * n) return "column " + std::to_string(g - n);
        for (int i : open[g]) return "region " + std::to_string(ctx.board.get_region(i / n, i % n));
        return std::string("region");
    };
    // Least-constraining candidate first (the ordering the bitset solver uses): the queen that removes the
    // fewest other candidates keeps the most room for the remaining groups.
    std::vector<std::pair<size_t, int>> order;
    for (int cell : open[best]) {
        size_t removed = 0;
        std::vector<char> seen(static_cast<size_t>(n * n), 0);
        for_each_attacked(s, ctx.board, cell, [&](int x) { if (!seen[x] && s.cells[x] == Cell::OPEN) { seen[x] = 1; ++removed; } });
        if (w_other != 0.0 && best >= 2 * n) {  // bitset's tie-breaker: touching candidates of other open regions
            const int r = cell / n, c = cell % n, reg = ctx.board.get_region_index(r, c);
            for (int dr = -1; dr <= 1; ++dr)
                for (int dc = -1; dc <= 1; ++dc) {
                    int rr = r + dr, cc = c + dc;
                    if (rr < 0 || cc < 0 || rr >= n || cc >= n) continue;
                    int o = ctx.board.get_region_index(rr, cc);
                    if (s.cells[rr * n + cc] == Cell::OPEN && o != reg && !filled[2 * n + o]) ++removed;
                }
        }
        order.push_back({removed, cell});
    }
    std::stable_sort(order.begin(), order.end(), [](auto& a, auto& b2) { return a.first < b2.first; });
    for (size_t i = 0; i < order.size() && !ctx.stop; ++i) {
        Position p{order[i].second / n, order[i].second % n};
        std::string why = i == 0
            ? "no rule applies. " + gname(best) + " has the fewest candidates (" + std::to_string(best_size) + "); try a queen at " + cell_name(p) +
                  ", the candidate that removes the fewest others (" + std::to_string(order[i].first) + ")."
            : (ctx.phase == SearchPhase::VERIFY_UNIQUE ? "a solution is known; to prove it unique, try " + gname(best) + "'s next candidate " + cell_name(p) + "."
                                                       : "the previous candidate failed; try " + gname(best) + "'s next candidate " + cell_name(p) + ".");
        ctx.log({StepKind::BRANCH, SearchPhase::SOLVE, depth, "guess", why, {p}, {{p, "Q"}}, i == 0 ? render(ctx.board, s) : ""});
        ++ctx.rep.stats.branches;
        State next = s;
        next.cells[order[i].second] = Cell::QUEEN;
        search(ctx, std::move(next), depth + 1);
    }
}

std::string solution_text(const Board& b, const std::vector<Position>& q) {
    State s = initial_state(b);
    for (auto& c : s.cells) c = Cell::OUT;
    for (auto p : q) s.cells[p.row * s.n + p.col] = Cell::QUEEN;
    return render(b, s);
}

} // namespace

ReasoningReport reason_queens(const Board& board, const ReasoningOptions& opt) {
    ReasoningReport rep;
    Ctx ctx{board, opt, rep, std::chrono::steady_clock::now(), {}, SearchPhase::SOLVE, {}, opt.check_unique ? size_t{2} : size_t{1}, false};
    State s = initial_state(board);
    auto elapsed = [&](auto from) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };
    if (opt.mode != ReasoningMode::EXPLAIN) {
        ctx.nodes() = 1;
        bool ok = deduce_all(ctx, s, 0, opt.mode == ReasoningMode::HINT);
        rep.final_board = render(board, s);
        rep.stats.solution_ms = elapsed(ctx.start);
        if (opt.mode == ReasoningMode::HINT) {
            rep.outcome = rep.steps.empty() ? ReasoningOutcome::NO_HINT : (ok ? ReasoningOutcome::HINT : ReasoningOutcome::NO_SOLUTION);
            return rep;
        }
        auto q = queens_of(s);
        if (!ok) rep.outcome = ReasoningOutcome::NO_SOLUTION;
        else if (static_cast<int>(q.size()) == s.n)
            rep.outcome = validate_queens_solution(board, q).is_valid ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::NO_SOLUTION;
        else if (rep.stats.budget_exhausted) rep.outcome = ReasoningOutcome::INCONCLUSIVE;
        else {
            rep.outcome = ReasoningOutcome::STALLED;
            rep.steps.push_back({StepKind::STALL, SearchPhase::SOLVE, 0, "", "no rule can prove another queen or elimination; the board below is partial.", {}, {}, rep.final_board});
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
    for (auto& q : ctx.solutions) rep.solutions_text.push_back(solution_text(board, q));
    rep.final_board = rep.solutions_text[0];
    if (ctx.solutions.size() >= 2) rep.outcome = ReasoningOutcome::MULTIPLE_SOLUTIONS;
    else if (rep.stats.budget_exhausted && opt.check_unique) rep.outcome = ReasoningOutcome::INCONCLUSIVE;
    else rep.outcome = rep.stats.branches == 0 ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::SOLVED_WITH_FALLBACK;
    return rep;
}

} // namespace solvedin
