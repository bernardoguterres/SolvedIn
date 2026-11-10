#include "solvedin/crossclimb_logic.hpp"

#include <algorithm>
#include <chrono>
#include <set>
#include <sstream>

namespace solvedin {

namespace {

enum class Link : char { UNKNOWN, ON, OFF };

bool one_apart(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    int d = 0;
    for (size_t i = 0; i < a.size(); ++i) d += a[i] != b[i];
    return d == 1;
}

std::string change_text(const std::string& a, const std::string& b) {
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return "letter " + std::to_string(i + 1) + ": " + std::string(1, a[i]) + " -> " + std::string(1, b[i]);
    return "";
}

struct Graph {
    // node 0 = START, 1..R = rungs, R+1 = END
    int R{0};
    std::vector<std::string> rung;
    std::vector<std::string> start_pool, end_pool;
    bool start_given{false}, end_given{false};
    struct Edge { int a, b; };
    std::vector<Edge> edges;
    std::vector<std::vector<int>> inc;
    [[nodiscard]] int nodes() const { return R + 2; }
    [[nodiscard]] int need(int v) const { return (v == 0 || v == R + 1) ? 1 : 2; }
    [[nodiscard]] int other(int e, int v) const { return edges[static_cast<size_t>(e)].a == v ? edges[static_cast<size_t>(e)].b : edges[static_cast<size_t>(e)].a; }
    [[nodiscard]] std::vector<std::string> pool_next_to(int endpoint, int r) const {
        std::vector<std::string> out;
        for (const auto& w : endpoint == 0 ? start_pool : end_pool) if (one_apart(w, rung[static_cast<size_t>(r - 1)])) out.push_back(w);
        return out;
    }
    [[nodiscard]] std::string name(int v) const {
        if (v == 0) return start_given ? start_pool.empty() ? "START" : start_pool[0] : "the hidden top word";
        if (v == R + 1) return end_given ? end_pool.empty() ? "END" : end_pool[0] : "the hidden bottom word";
        return rung[static_cast<size_t>(v - 1)];
    }
};

Graph build(const CrossclimbBoard& b, const Trie& lex) {
    Graph g;
    for (auto w : b.rung_words()) { std::transform(w.begin(), w.end(), w.begin(), ::toupper); g.rung.push_back(w); }
    g.R = static_cast<int>(g.rung.size());
    std::set<std::string> rung_set(g.rung.begin(), g.rung.end());
    auto dict_neighbours = [&]() {
        std::set<std::string> out;
        for (const auto& w : g.rung) {
            std::string c = w;
            for (size_t i = 0; i < c.size(); ++i) {
                const char o = c[i];
                for (char x = 'A'; x <= 'Z'; ++x) {
                    if (x == o) continue;
                    c[i] = x;
                    if (lex.contains(c) && !rung_set.count(c)) out.insert(c);
                }
                c[i] = o;
            }
        }
        return std::vector<std::string>(out.begin(), out.end());
    };
    auto pool = [&](bool given, const std::optional<std::string>& w, const std::vector<std::string>& cands) {
        std::vector<std::string> p;
        auto up = [](std::string s) { std::transform(s.begin(), s.end(), s.begin(), ::toupper); return s; };
        if (given) { if (lex.contains(up(*w)) && !rung_set.count(up(*w))) p.push_back(up(*w)); }
        else if (!cands.empty()) { for (const auto& c : cands) if (lex.contains(up(c)) && !rung_set.count(up(c))) p.push_back(up(c)); }
        else p = dict_neighbours();
        return p;
    };
    g.start_given = b.has_start_word();
    g.end_given = b.has_end_word();
    g.start_pool = pool(g.start_given, b.start_word(), b.start_candidates());
    g.end_pool = pool(g.end_given, b.end_word(), b.end_candidates());
    g.inc.assign(static_cast<size_t>(g.nodes()), {});
    auto add = [&](int a, int c) { g.edges.push_back({a, c}); g.inc[static_cast<size_t>(a)].push_back(static_cast<int>(g.edges.size()) - 1); g.inc[static_cast<size_t>(c)].push_back(static_cast<int>(g.edges.size()) - 1); };
    for (int i = 1; i <= g.R; ++i) {
        if (!g.pool_next_to(0, i).empty()) add(0, i);
        for (int j = i + 1; j <= g.R; ++j) if (one_apart(g.rung[static_cast<size_t>(i - 1)], g.rung[static_cast<size_t>(j - 1)])) add(i, j);
        if (!g.pool_next_to(g.R + 1, i).empty()) add(i, g.R + 1);
    }
    return g;
}

struct Deduction {
    std::string rule, explanation;
    std::vector<int> on, off;
    bool contradiction{false};
};

std::optional<Deduction> next_deduction(const Graph& g, const std::vector<Link>& L) {
    const int V = g.nodes();
    std::vector<int> on(static_cast<size_t>(V), 0), avail(static_cast<size_t>(V), 0);
    for (int v = 0; v < V; ++v)
        for (int e : g.inc[static_cast<size_t>(v)]) { on[static_cast<size_t>(v)] += L[static_cast<size_t>(e)] == Link::ON; avail[static_cast<size_t>(v)] += L[static_cast<size_t>(e)] != Link::OFF; }
    for (int v = 0; v < V; ++v)
        if (avail[static_cast<size_t>(v)] < g.need(v))
            return Deduction{"dead-end", g.name(v) + " needs " + std::to_string(g.need(v)) + " ladder neighbour" + (g.need(v) > 1 ? "s" : "") +
                                             " one letter away but only " + std::to_string(avail[static_cast<size_t>(v)]) + " remain.", {}, {}, true};
    for (int v = 0; v < V; ++v) {
        if (on[static_cast<size_t>(v)] != g.need(v)) continue;
        Deduction D{"full-word", g.name(v) + " already has its " + std::to_string(g.need(v)) + " ladder neighbour" + (g.need(v) > 1 ? "s" : "") + ", so its other one-letter links are unused.", {}, {}, false};
        for (int e : g.inc[static_cast<size_t>(v)]) if (L[static_cast<size_t>(e)] == Link::UNKNOWN) D.off.push_back(e);
        if (!D.off.empty()) return D;
    }
    for (int v = 0; v < V; ++v) {
        if (avail[static_cast<size_t>(v)] != g.need(v)) continue;
        Deduction D{"corridor", "", {}, {}, false};
        std::string names;
        for (int e : g.inc[static_cast<size_t>(v)]) if (L[static_cast<size_t>(e)] != Link::OFF) names += (names.empty() ? "" : " and ") + g.name(g.other(e, v));
        for (int e : g.inc[static_cast<size_t>(v)]) if (L[static_cast<size_t>(e)] == Link::UNKNOWN) D.on.push_back(e);
        if (D.on.empty()) continue;
        D.explanation = g.need(v) == 2 ? g.name(v) + " is one letter from exactly two words (" + names + "), so they are its neighbours on the ladder."
                                       : g.name(v) + " can only attach to " + names + ", so that link is on the ladder.";
        return D;
    }
    // fragments
    std::vector<int> comp(static_cast<size_t>(V), -1), size;
    for (int v = 0; v < V; ++v) {
        if (comp[static_cast<size_t>(v)] >= 0) continue;
        int id = static_cast<int>(size.size()), n = 0, edges_on = 0;
        std::vector<int> st{v};
        comp[static_cast<size_t>(v)] = id;
        while (!st.empty()) {
            int x = st.back(); st.pop_back(); ++n;
            for (int e : g.inc[static_cast<size_t>(x)]) {
                if (L[static_cast<size_t>(e)] != Link::ON) continue;
                ++edges_on;
                int y = g.other(e, x);
                if (comp[static_cast<size_t>(y)] < 0) { comp[static_cast<size_t>(y)] = id; st.push_back(y); }
            }
        }
        if (edges_on / 2 != n - 1) return Deduction{"loop", "the chosen links close a loop.", {}, {}, true};
        size.push_back(n);
    }
    if (comp[0] == comp[static_cast<size_t>(g.R + 1)] && size[static_cast<size_t>(comp[0])] < V)
        return Deduction{"early-finish", "the top and bottom are joined before every rung is used.", {}, {}, true};
    for (int e = 0; e < static_cast<int>(g.edges.size()); ++e) {
        if (L[static_cast<size_t>(e)] != Link::UNKNOWN) continue;
        const int a = g.edges[static_cast<size_t>(e)].a, b = g.edges[static_cast<size_t>(e)].b;
        if (comp[static_cast<size_t>(a)] == comp[static_cast<size_t>(b)])
            return Deduction{"no-loop", "linking " + g.name(a) + " and " + g.name(b) + " would close the partial ladder into a loop.", {}, {e}, false};
        const bool joins = (comp[static_cast<size_t>(a)] == comp[0] && comp[static_cast<size_t>(b)] == comp[static_cast<size_t>(g.R + 1)]) ||
                           (comp[static_cast<size_t>(b)] == comp[0] && comp[static_cast<size_t>(a)] == comp[static_cast<size_t>(g.R + 1)]);
        if (joins && size[static_cast<size_t>(comp[static_cast<size_t>(a)])] + size[static_cast<size_t>(comp[static_cast<size_t>(b)])] < V)
            return Deduction{"early-finish", "linking " + g.name(a) + " and " + g.name(b) + " would join top and bottom before every rung is used.", {}, {e}, false};
    }
    return std::nullopt;
}

struct Ctx {
    const Graph& g;
    const ReasoningOptions& opt;
    ReasoningReport& rep;
    std::chrono::steady_clock::time_point start, first_solution_at;
    SearchPhase phase{SearchPhase::SOLVE};
    std::vector<std::vector<std::string>> ladders;
    size_t max_solutions{2};
    bool stop{false};
    void log(ReasoningStep s) { s.phase = phase; if (rep.steps.size() < opt.max_logged_steps) rep.steps.push_back(std::move(s)); }
    size_t& nodes() { return phase == SearchPhase::SOLVE ? rep.stats.solution_nodes : rep.stats.uniqueness_nodes; }
};

bool deduce_all(Ctx& ctx, std::vector<Link>& L, int depth, bool single = false) {
    while (auto D = next_deduction(ctx.g, L)) {
        ReasoningStep st{D->contradiction ? StepKind::CONTRADICTION : StepKind::DEDUCTION, SearchPhase::SOLVE, depth, D->rule, D->explanation, {}, {}, ""};
        if (D->contradiction) { ctx.log(std::move(st)); return false; }
        for (int e : D->on) st.changes.push_back({{-1, -1}, ctx.g.name(ctx.g.edges[static_cast<size_t>(e)].a) + " - " + ctx.g.name(ctx.g.edges[static_cast<size_t>(e)].b)});
        for (int e : D->off) st.changes.push_back({{-1, -1}, "not " + ctx.g.name(ctx.g.edges[static_cast<size_t>(e)].a) + " - " + ctx.g.name(ctx.g.edges[static_cast<size_t>(e)].b)});
        for (int e : D->on) L[static_cast<size_t>(e)] = Link::ON;
        for (int e : D->off) L[static_cast<size_t>(e)] = Link::OFF;
        ++ctx.rep.stats.deductions;
        ++ctx.rep.stats.rule_counts[D->rule];
        ctx.log(std::move(st));
        if (single) break;
    }
    return true;
}

std::string ladder_text(const std::vector<std::string>& lad) {
    std::string t;
    for (size_t i = 0; i < lad.size(); ++i) {
        t += lad[i];
        if (i) t += "   (" + change_text(lad[i - 1], lad[i]) + ")";
        t += "\n";
    }
    return t;
}

std::string partial_text(const Graph& g, const std::vector<Link>& L) {
    std::string t;
    for (int e = 0; e < static_cast<int>(g.edges.size()); ++e)
        if (L[static_cast<size_t>(e)] == Link::ON) t += g.name(g.edges[static_cast<size_t>(e)].a) + " - " + g.name(g.edges[static_cast<size_t>(e)].b) + "\n";
    return t.empty() ? "(no links fixed)\n" : "fixed links:\n" + t;
}

// Complete link set -> ordered rung path -> every endpoint word combination.
void emit_solutions(Ctx& ctx, const std::vector<Link>& L, int depth) {
    const Graph& g = ctx.g;
    std::vector<int> order;
    int prev = -1, cur = 0;
    while (cur >= 0 && static_cast<int>(order.size()) <= g.nodes()) {
        order.push_back(cur);
        int nxt = -1;
        for (int e : g.inc[static_cast<size_t>(cur)]) if (L[static_cast<size_t>(e)] == Link::ON && g.other(e, cur) != prev) { nxt = g.other(e, cur); break; }
        prev = cur; cur = nxt;
    }
    if (static_cast<int>(order.size()) != g.nodes() || order.back() != g.R + 1) return;
    auto tops = g.pool_next_to(0, order[1]), bottoms = g.pool_next_to(g.R + 1, order[static_cast<size_t>(g.R)]);
    for (const auto& t : tops)
        for (const auto& bt : bottoms) {
            if (t == bt) continue;
            std::vector<std::string> lad{t};
            for (int k = 1; k <= g.R; ++k) lad.push_back(g.rung[static_cast<size_t>(order[static_cast<size_t>(k)] - 1)]);
            lad.push_back(bt);
            ctx.log({StepKind::SOLUTION, SearchPhase::SOLVE, depth, "", "every rung is used once and each neighbour differs in one letter.", {}, {}, ladder_text(lad)});
            ctx.ladders.push_back(lad);
            if (ctx.ladders.size() == 1) { ctx.first_solution_at = std::chrono::steady_clock::now(); ctx.phase = SearchPhase::VERIFY_UNIQUE; }
            if (ctx.ladders.size() >= ctx.max_solutions) { ctx.stop = true; return; }
        }
}

void search(Ctx& ctx, std::vector<Link> L, int depth) {
    if (ctx.stop) return;
    ++ctx.nodes();
    if (ctx.opt.max_nodes && ctx.rep.stats.solution_nodes + ctx.rep.stats.uniqueness_nodes > ctx.opt.max_nodes) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return; }
    if (!deduce_all(ctx, L, depth)) { if (depth > 0) ++ctx.rep.stats.failed_branches; return; }
    int pick = -1;
    for (int e = 0; e < static_cast<int>(L.size()); ++e) if (L[static_cast<size_t>(e)] == Link::UNKNOWN) { pick = e; break; }
    if (pick < 0) { emit_solutions(ctx, L, depth); return; }
    const auto& E = ctx.g.edges[static_cast<size_t>(pick)];
    for (int i = 0; i < 2 && !ctx.stop; ++i) {
        ctx.log({StepKind::BRANCH, SearchPhase::SOLVE, depth, "guess",
                 i == 0 ? "no rule applies; try putting " + ctx.g.name(E.a) + " next to " + ctx.g.name(E.b) + "."
                        : "now try " + ctx.g.name(E.a) + " and " + ctx.g.name(E.b) + " apart.",
                 {}, {}, i == 0 ? partial_text(ctx.g, L) : ""});
        ++ctx.rep.stats.branches;
        auto next = L;
        next[static_cast<size_t>(pick)] = i == 0 ? Link::ON : Link::OFF;
        search(ctx, std::move(next), depth + 1);
    }
}

} // namespace

ReasoningReport reason_crossclimb(const CrossclimbBoard& board, const Trie& lexicon, const ReasoningOptions& opt) {
    ReasoningReport rep;
    Graph g = build(board, lexicon);
    Ctx ctx{g, opt, rep, std::chrono::steady_clock::now(), {}, SearchPhase::SOLVE, {}, opt.check_unique ? size_t{2} : size_t{1}, false};
    auto elapsed = [&](auto from) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };
    rep.notes.push_back("supplied-word reconstruction: the rung words are given, so this orders them; it does not solve clues.");
    if (!g.start_given || !g.end_given)
        rep.notes.push_back(std::string("hidden endpoints use ") + (board.has_start_candidates() || board.has_end_candidates() ? "the supplied candidate lists" : "every dictionary word one letter from a rung") +
                            "; top pool " + std::to_string(g.start_pool.size()) + ", bottom pool " + std::to_string(g.end_pool.size()) + " words.");
    // a teaching step: the one-letter graph
    std::string graph;
    for (int v = 1; v <= g.R; ++v) {
        graph += g.rung[static_cast<size_t>(v - 1)] + ":";
        for (int e : g.inc[static_cast<size_t>(v)]) graph += " " + g.name(g.other(e, v));
        graph += "\n";
    }
    rep.notes.push_back("one-letter neighbours of each rung (the only possible ladder links):\n" + graph);
    std::vector<Link> L(g.edges.size(), Link::UNKNOWN);
    if (g.start_pool.empty() || g.end_pool.empty()) { rep.outcome = ReasoningOutcome::NO_SOLUTION; return rep; }
    if (opt.mode != ReasoningMode::EXPLAIN) {
        ctx.nodes() = 1;
        bool ok = deduce_all(ctx, L, 0, opt.mode == ReasoningMode::HINT);
        rep.final_board = partial_text(g, L);
        rep.stats.solution_ms = elapsed(ctx.start);
        if (opt.mode == ReasoningMode::HINT) { rep.outcome = rep.steps.empty() ? ReasoningOutcome::NO_HINT : (ok ? ReasoningOutcome::HINT : ReasoningOutcome::NO_SOLUTION); return rep; }
        if (!ok) { rep.outcome = ReasoningOutcome::NO_SOLUTION; return rep; }
        const bool decided = std::none_of(L.begin(), L.end(), [](Link l) { return l == Link::UNKNOWN; });
        if (decided) {
            emit_solutions(ctx, L, 0);
            rep.outcome = ctx.ladders.size() == 1 ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ctx.ladders.empty() ? ReasoningOutcome::NO_SOLUTION : ReasoningOutcome::STALLED;
            if (ctx.ladders.size() == 1) rep.final_board = ladder_text(ctx.ladders[0]);
        } else {
            rep.outcome = ReasoningOutcome::STALLED;
            rep.steps.push_back({StepKind::STALL, SearchPhase::SOLVE, 0, "", "no rule can fix another link.", {}, {}, rep.final_board});
        }
        return rep;
    }
    search(ctx, L, 0);
    if (ctx.ladders.empty()) { rep.stats.solution_ms = elapsed(ctx.start); rep.outcome = rep.stats.budget_exhausted ? ReasoningOutcome::INCONCLUSIVE : ReasoningOutcome::NO_SOLUTION; return rep; }
    rep.stats.solution_ms = std::chrono::duration<double, std::milli>(ctx.first_solution_at - ctx.start).count();
    rep.stats.uniqueness_ms = elapsed(ctx.first_solution_at);
    for (auto& l : ctx.ladders) rep.solutions_text.push_back(ladder_text(l));
    rep.final_board = rep.solutions_text[0];
    rep.outcome = ctx.ladders.size() >= 2 ? ReasoningOutcome::MULTIPLE_SOLUTIONS
                  : rep.stats.branches == 0 ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::SOLVED_WITH_FALLBACK;
    return rep;
}

} // namespace solvedin
