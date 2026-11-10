#include "solvedin/zip_logic.hpp"
#include "solvedin/zip_validator.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <numeric>
#include <sstream>

namespace solvedin {

namespace zip_logic {

namespace {

struct View {
    const ZipBoard& b;
    const State& s;
    int R, C, V, K;
    std::vector<std::vector<int>> inc;  // edges per cell
    std::vector<int> need, on_deg, avail_deg;

    View(const ZipBoard& board, const State& st) : b(board), s(st), R(board.rows()), C(board.cols()), V(R * C), K(board.max_checkpoint()) {
        inc.assign(V, {});
        for (int e = 0; e < static_cast<int>(s.edges.size()); ++e) { inc[s.edges[e].a].push_back(e); inc[s.edges[e].b].push_back(e); }
        need.assign(V, 2); on_deg.assign(V, 0); avail_deg.assign(V, 0);
        for (int v = 0; v < V; ++v) {
            int cp = b.checkpoint_at(v / C, v % C);
            if (cp == 1 || cp == K) need[v] = 1;
            for (int e : inc[v]) {
                if (s.links[e] == Link::ON) ++on_deg[v];
                if (s.links[e] != Link::OFF) ++avail_deg[v];
            }
        }
    }
    [[nodiscard]] Position pos(int v) const { return {v / C, v % C}; }
    [[nodiscard]] int other(int e, int v) const { return s.edges[e].a == v ? s.edges[e].b : s.edges[e].a; }
    [[nodiscard]] int cp(int v) const { return b.checkpoint_at(v / C, v % C); }
    [[nodiscard]] std::string cell(int v) const {
        int c = cp(v);
        return cell_name(pos(v)) + (c ? " [" + std::to_string(c) + "]" : "");
    }
    [[nodiscard]] std::string link(int e) const { return cell(s.edges[e].a) + "-" + cell(s.edges[e].b); }
};

// Fragments of ON links: ordered cells from one free end to the other (empty if the fragment is a loop).
struct Fragments {
    std::vector<int> comp;                    // per cell
    std::vector<std::vector<int>> ordered;    // per component
    std::vector<bool> loop;
};

Fragments fragments(const View& w) {
    Fragments F;
    F.comp.assign(w.V, -1);
    for (int v = 0; v < w.V; ++v) {
        if (F.comp[v] >= 0) continue;
        const int id = static_cast<int>(F.ordered.size());
        std::vector<int> cells, stack{v};
        F.comp[v] = id;
        int edges_on = 0;
        while (!stack.empty()) {
            int x = stack.back(); stack.pop_back();
            cells.push_back(x);
            for (int e : w.inc[x]) {
                if (w.s.links[e] != Link::ON) continue;
                ++edges_on;
                int y = w.other(e, x);
                if (F.comp[y] < 0) { F.comp[y] = id; stack.push_back(y); }
            }
        }
        edges_on /= 2;
        const bool is_loop = edges_on != static_cast<int>(cells.size()) - 1;
        std::vector<int> order;
        if (!is_loop) {
            int start = cells[0];
            for (int x : cells) if (w.on_deg[x] <= 1) { start = x; break; }
            int prev = -1, cur = start;
            while (cur >= 0) {
                order.push_back(cur);
                int nxt = -1;
                for (int e : w.inc[cur]) {
                    if (w.s.links[e] != Link::ON) continue;
                    int y = w.other(e, cur);
                    if (y != prev) { nxt = y; break; }
                }
                prev = cur; cur = nxt;
                if (order.size() > cells.size()) break;
            }
        }
        F.ordered.push_back(order);
        F.loop.push_back(is_loop);
    }
    return F;
}

std::vector<int> checkpoints_along(const View& w, const std::vector<int>& cells) {
    std::vector<int> v;
    for (int x : cells) if (w.cp(x)) v.push_back(w.cp(x));
    return v;
}

// A contiguous stretch of the final path carries consecutive checkpoints in order (either direction).
bool consecutive(const std::vector<int>& v) {
    if (v.size() < 2) return true;
    const int step = v[1] - v[0];
    if (step != 1 && step != -1) return false;
    for (size_t i = 1; i < v.size(); ++i) if (v[i] - v[i - 1] != step) return false;
    return true;
}

std::string list_text(const std::vector<int>& v) {
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? ", " : "") + std::to_string(v[i]);
    return s;
}

} // namespace

State initial_state(const ZipBoard& board) {
    State s;
    const int R = board.rows(), C = board.cols();
    for (int r = 0; r < R; ++r)
        for (int c = 0; c < C; ++c) {
            if (c + 1 < C && board.can_move({r, c}, {r, c + 1})) s.edges.push_back({r * C + c, r * C + c + 1});
            if (r + 1 < R && board.can_move({r, c}, {r + 1, c})) s.edges.push_back({r * C + c, (r + 1) * C + c});
        }
    s.links.assign(s.edges.size(), Link::UNKNOWN);
    return s;
}

std::optional<Deduction> next_deduction(const ZipBoard& board, const State& s) {
    View w(board, s);
    // dead end / overfull
    for (int v = 0; v < w.V; ++v) {
        if (w.on_deg[v] > w.need[v])
            return Deduction{"overfull", w.cell(v) + " has more path links than it can take.", {w.pos(v)}, {}, {}, true};
        if (w.avail_deg[v] < w.need[v])
            return Deduction{"dead-end", w.cell(v) + " is a dead end: it needs " + std::to_string(w.need[v]) + " link" + (w.need[v] > 1 ? "s" : "") +
                                 " but only " + std::to_string(w.avail_deg[v]) + " usable neighbour" + (w.avail_deg[v] == 1 ? " is" : "s are") + " left.",
                             {w.pos(v)}, {}, {}, true};
    }
    // full cell
    for (int v = 0; v < w.V; ++v) {
        if (w.on_deg[v] != w.need[v]) continue;
        Deduction D{"full-cell", "", {w.pos(v)}, {}, {}, false};
        for (int e : w.inc[v]) if (s.links[e] == Link::UNKNOWN) D.off.push_back(e);
        if (D.off.empty()) continue;
        D.explanation = w.cell(v) + " already has its " + std::to_string(w.need[v]) + " path link" + (w.need[v] > 1 ? "s" : "") +
                        ", so its other exits are closed.";
        return D;
    }
    // corridor
    for (int v = 0; v < w.V; ++v) {
        if (w.avail_deg[v] != w.need[v]) continue;
        Deduction D{"corridor", "", {w.pos(v)}, {}, {}, false};
        std::string to;
        for (int e : w.inc[v]) if (s.links[e] == Link::UNKNOWN) { D.on.push_back(e); }
        for (int e : w.inc[v]) if (s.links[e] != Link::OFF) to += (to.empty() ? "" : " and ") + w.cell(w.other(e, v));
        if (D.on.empty()) continue;
        D.explanation = w.need[v] == 1
            ? w.cell(v) + " is " + (w.cp(v) == 1 ? "the start" : "the end") + " and has a single usable exit, to " + to + ": the path must use it."
            : w.cell(v) + " has exactly two usable exits (to " + to + "); the path must enter and leave it, so both links are on the path.";
        return D;
    }
    // fragments: loops, checkpoint order, early start-to-end
    Fragments F = fragments(w);
    for (size_t id = 0; id < F.ordered.size(); ++id) {
        if (F.loop[id]) return Deduction{"loop", "the path links close a loop.", {}, {}, {}, true};
        auto seq = checkpoints_along(w, F.ordered[id]);
        if (!consecutive(seq))
            return Deduction{"checkpoint-order", "a stretch of path visits checkpoints " + list_text(seq) + " out of order.", {}, {}, {}, true};
        bool has1 = std::find(seq.begin(), seq.end(), 1) != seq.end(), hasK = std::find(seq.begin(), seq.end(), w.K) != seq.end();
        if (has1 && hasK && static_cast<int>(F.ordered[id].size()) < w.V)
            return Deduction{"early-finish", "the start and the end are joined while " + std::to_string(w.V - F.ordered[id].size()) + " cells are unvisited.", {}, {}, {}, true};
    }
    for (int e = 0; e < static_cast<int>(s.edges.size()); ++e) {
        if (s.links[e] != Link::UNKNOWN) continue;
        const int u = s.edges[e].a, v = s.edges[e].b;
        if (F.comp[u] == F.comp[v])
            return Deduction{"no-loop", "Linking " + w.link(e) + " would join the two ends of the same stretch of path into a closed loop.",
                             {w.pos(u), w.pos(v)}, {}, {e}, false};
        auto A = F.ordered[F.comp[u]], B = F.ordered[F.comp[v]];
        if (A.back() != u) std::reverse(A.begin(), A.end());
        if (B.front() != v) std::reverse(B.begin(), B.end());
        std::vector<int> merged = A;
        merged.insert(merged.end(), B.begin(), B.end());
        auto seq = checkpoints_along(w, merged);
        if (!consecutive(seq))
            return Deduction{"checkpoint-order", "Linking " + w.link(e) + " would put checkpoints " + list_text(seq) +
                                                     " on one stretch of path, out of order, so the link is closed.",
                             {w.pos(u), w.pos(v)}, {}, {e}, false};
        bool has1 = std::find(seq.begin(), seq.end(), 1) != seq.end(), hasK = std::find(seq.begin(), seq.end(), w.K) != seq.end();
        if (has1 && hasK && static_cast<int>(merged.size()) < w.V)
            return Deduction{"early-finish", "Linking " + w.link(e) + " would join the start to the end with " + std::to_string(w.V - merged.size()) +
                                                 " cells still unvisited, so the link is closed.",
                             {w.pos(u), w.pos(v)}, {}, {e}, false};
    }
    // bridges and stranded areas in the graph of usable links (Tarjan, iterative)
    {
        std::vector<int> disc(w.V, -1), low(w.V, 0), parent_edge(w.V, -1), sub(w.V, 1), pieces(w.V, 0);
        int timer = 0, roots = 0;
        std::vector<int> bridges;
        for (int root = 0; root < w.V; ++root) {
            if (disc[root] >= 0) continue;
            ++roots;
            std::vector<std::pair<int, size_t>> st{{root, 0}};
            disc[root] = low[root] = timer++;
            while (!st.empty()) {
                auto& [x, i] = st.back();
                if (i < w.inc[x].size()) {
                    int e = w.inc[x][i++];
                    if (s.links[e] == Link::OFF || e == parent_edge[x]) continue;
                    int y = w.other(e, x);
                    if (disc[y] < 0) { disc[y] = low[y] = timer++; parent_edge[y] = e; st.push_back({y, 0}); }
                    else low[x] = std::min(low[x], disc[y]);
                } else {
                    int xx = x;
                    st.pop_back();
                    if (!st.empty()) {
                        int p = st.back().first;
                        low[p] = std::min(low[p], low[xx]);
                        sub[p] += sub[xx];
                        if (low[xx] > disc[p]) bridges.push_back(parent_edge[xx]);
                        if (low[xx] >= disc[p]) ++pieces[p];  // removing p separates xx's subtree
                    }
                }
            }
        }
        if (roots > 1)
            return Deduction{"stranded-area", "the usable links split the board into " + std::to_string(roots) +
                                                  " separate areas; one path cannot cover them all.", {}, {}, {}, true};
        // A cell whose removal leaves more areas than it has path links strands one of them.
        for (int v = 0; v < w.V; ++v) {
            // DFS root (cell 0): one area per child subtree; any other cell: separated child subtrees + its parent's side.
            const int areas = v == 0 ? pieces[v] : pieces[v] + 1;
            if (areas > w.need[v])
                return Deduction{"stranded-area", "removing " + w.cell(v) + " would leave " + std::to_string(areas) +
                                                      " separate areas, but the path passes through it only " + std::to_string(w.need[v]) +
                                                      (w.need[v] > 1 ? " times" : " time") + ": one area would be stranded.",
                                 {w.pos(v)}, {}, {}, true};
        }
        for (int e : bridges) {
            if (s.links[e] != Link::UNKNOWN) continue;
            const int u = s.edges[e].a, v = s.edges[e].b;
            const int child = parent_edge[v] == e ? v : u;
            const int side = std::min(sub[child], w.V - sub[child]);
            return Deduction{"only-link", "The link " + w.link(e) + " is the only usable connection between a group of " + std::to_string(side) +
                                              " cell" + (side == 1 ? "" : "s") + " and the rest of the board; the path must cross it.",
                             {w.pos(u), w.pos(v)}, {e}, {}, false};
        }
    }
    return std::nullopt;
}

void apply(State& s, const Deduction& d) {
    for (int e : d.on) s.links[e] = Link::ON;
    for (int e : d.off) s.links[e] = Link::OFF;
}

std::string render(const ZipBoard& board, const State& s) {
    const int R = board.rows(), C = board.cols();
    auto on = [&](int a, int b) {
        for (size_t e = 0; e < s.edges.size(); ++e)
            if (s.links[e] == Link::ON && ((s.edges[e].a == a && s.edges[e].b == b) || (s.edges[e].a == b && s.edges[e].b == a))) return true;
        return false;
    };
    std::ostringstream os;
    for (int r = 0; r < R; ++r) {
        std::string line, below;
        for (int c = 0; c < C; ++c) {
            int cp = board.checkpoint_at(r, c);
            std::string t = cp ? std::to_string(cp) : "o";
            line += std::string(t.size() < 2 ? 2 - t.size() : 0, ' ') + t;
            if (c + 1 < C) line += on(r * C + c, r * C + c + 1) ? "--" : "  ";
            if (r + 1 < R) { below += on(r * C + c, (r + 1) * C + c) ? " |" : "  "; if (c + 1 < C) below += "  "; }
        }
        os << line << "\n";
        if (r + 1 < R) {
            while (!below.empty() && below.back() == ' ') below.pop_back();
            os << below << "\n";
        }
    }
    return os.str();
}

std::vector<Position> extract_path(const ZipBoard& board, const State& s) {
    View w(board, s);
    for (int v = 0; v < w.V; ++v) if (w.on_deg[v] != w.need[v]) return {};
    const Position st = board.checkpoint_pos(1);
    int cur = st.row * w.C + st.col, prev = -1;
    std::vector<Position> path;
    while (cur >= 0 && static_cast<int>(path.size()) <= w.V) {
        path.push_back(w.pos(cur));
        int nxt = -1;
        for (int e : w.inc[cur]) if (s.links[e] == Link::ON && w.other(e, cur) != prev) { nxt = w.other(e, cur); break; }
        prev = cur; cur = nxt;
    }
    if (static_cast<int>(path.size()) != w.V) return {};
    return path;
}

} // namespace zip_logic

namespace {

using namespace zip_logic;

struct Ctx {
    const ZipBoard& board;
    const ReasoningOptions& opt;
    ReasoningReport& rep;
    std::chrono::steady_clock::time_point start, first_solution_at;
    SearchPhase phase{SearchPhase::SOLVE};
    std::vector<std::vector<Position>> solutions;
    std::vector<std::string> solution_boards;
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
    while (auto D = next_deduction(ctx.board, s)) {
        ReasoningStep st{D->contradiction ? StepKind::CONTRADICTION : StepKind::DEDUCTION, SearchPhase::SOLVE, depth, D->rule, D->explanation, D->involved, {}, ""};
        if (D->contradiction) { ctx.log(std::move(st)); return false; }
        const int C = ctx.board.cols();
        for (int e : D->on) st.changes.push_back({{s.edges[e].a / C, s.edges[e].a % C}, "linked to " + cell_name({s.edges[e].b / C, s.edges[e].b % C})});
        for (int e : D->off) st.changes.push_back({{s.edges[e].a / C, s.edges[e].a % C}, "not linked to " + cell_name({s.edges[e].b / C, s.edges[e].b % C})});
        apply(s, *D);
        ++ctx.rep.stats.deductions;
        ++ctx.rep.stats.rule_counts[D->rule];
        ctx.log(std::move(st));
        if (single) break;
    }
    return true;
}

void search(Ctx& ctx, State s, int depth) {
    if (ctx.stop) return;
    if (ctx.over_budget()) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return; }
    ++ctx.nodes();
    if (!deduce_all(ctx, s, depth)) { if (depth > 0) ++ctx.rep.stats.failed_branches; return; }
    auto path = extract_path(ctx.board, s);
    bool undecided = std::any_of(s.links.begin(), s.links.end(), [](Link l) { return l == Link::UNKNOWN; });
    if (!path.empty()) {
        auto v = validate_zip_solution(ctx.board, path);
        if (!v.is_valid) {
            ctx.log({StepKind::CONTRADICTION, SearchPhase::SOLVE, depth, "validator", v.error_message, {}, {}, ""});
            if (depth > 0) ++ctx.rep.stats.failed_branches;
            return;
        }
        ctx.log({StepKind::SOLUTION, SearchPhase::SOLVE, depth, "", "the links form one path through every cell and the independent validator accepts it.", {}, {}, render(ctx.board, s)});
        ctx.solutions.push_back(path);
        ctx.solution_boards.push_back(render(ctx.board, s));
        if (ctx.solutions.size() == 1) { ctx.first_solution_at = std::chrono::steady_clock::now(); ctx.phase = SearchPhase::VERIFY_UNIQUE; }
        if (ctx.solutions.size() >= ctx.max_solutions) ctx.stop = true;
        return;
    }
    if (!undecided) { if (depth > 0) ++ctx.rep.stats.failed_branches; return; }
    // branch on an undecided link of the cell that still needs links and has the fewest undecided ones
    const int C = ctx.board.cols(), V = ctx.board.num_cells(), K = ctx.board.max_checkpoint();
    std::vector<std::vector<int>> inc(V);
    for (int e = 0; e < static_cast<int>(s.edges.size()); ++e) { inc[s.edges[e].a].push_back(e); inc[s.edges[e].b].push_back(e); }
    int best_e = -1, best_u = 99, best_v = -1;
    for (int v = 0; v < V; ++v) {
        int cp = ctx.board.checkpoint_at(v / C, v % C), need = (cp == 1 || cp == K) ? 1 : 2, on = 0, unk = 0, first = -1;
        for (int e : inc[v]) { if (s.links[e] == Link::ON) ++on; if (s.links[e] == Link::UNKNOWN) { ++unk; if (first < 0) first = e; } }
        if (on < need && unk > 0 && unk < best_u) { best_u = unk; best_e = first; best_v = v; }
    }
    if (best_e < 0) return;
    const Position a{s.edges[best_e].a / C, s.edges[best_e].a % C}, b{s.edges[best_e].b / C, s.edges[best_e].b % C};
    for (int i = 0; i < 2 && !ctx.stop; ++i) {
        std::string why = i == 0
            ? "no rule applies. " + cell_name({best_v / C, best_v % C}) + " still needs a link and has the fewest open choices (" + std::to_string(best_u) +
                  "); try linking " + cell_name(a) + "-" + cell_name(b) + "."
            : (ctx.phase == SearchPhase::VERIFY_UNIQUE ? "a solution is known; to prove it unique, try without the link " + cell_name(a) + "-" + cell_name(b) + "."
                                                       : "that link led to a dead end; try without it.");
        ctx.log({StepKind::BRANCH, SearchPhase::SOLVE, depth, "guess", why, {a, b}, {{a, i == 0 ? "linked to " + cell_name(b) : "not linked to " + cell_name(b)}},
                 i == 0 ? render(ctx.board, s) : ""});
        ++ctx.rep.stats.branches;
        State next = s;
        next.links[best_e] = i == 0 ? Link::ON : Link::OFF;
        search(ctx, std::move(next), depth + 1);
    }
}

std::string path_text(const std::vector<Position>& p) {
    std::string t;
    for (size_t i = 0; i < p.size(); ++i) { t += cell_name(p[i]); t += (i + 1 < p.size()) ? ((i + 1) % 8 == 0 ? " ->\n" : " -> ") : "\n"; }
    return t;
}

} // namespace

ReasoningReport reason_zip(const ZipBoard& board, const ReasoningOptions& opt) {
    ReasoningReport rep;
    Ctx ctx{board, opt, rep, std::chrono::steady_clock::now(), {}, SearchPhase::SOLVE, {}, {}, opt.check_unique ? size_t{2} : size_t{1}, false};
    auto elapsed = [&](auto from) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };
    if (board.num_cells() < 2 || board.max_checkpoint() < 2) { rep.outcome = ReasoningOutcome::NO_SOLUTION; return rep; }
    State s = initial_state(board);
    if (opt.mode != ReasoningMode::EXPLAIN) {
        ctx.nodes() = 1;
        bool ok = deduce_all(ctx, s, 0, opt.mode == ReasoningMode::HINT);
        rep.final_board = render(board, s);
        rep.stats.solution_ms = elapsed(ctx.start);
        if (opt.mode == ReasoningMode::HINT) {
            rep.outcome = rep.steps.empty() ? ReasoningOutcome::NO_HINT : (ok ? ReasoningOutcome::HINT : ReasoningOutcome::NO_SOLUTION);
            return rep;
        }
        auto path = extract_path(board, s);
        if (!ok) rep.outcome = ReasoningOutcome::NO_SOLUTION;
        else if (!path.empty()) rep.outcome = validate_zip_solution(board, path).is_valid ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::NO_SOLUTION;
        else {
            rep.outcome = ReasoningOutcome::STALLED;
            rep.steps.push_back({StepKind::STALL, SearchPhase::SOLVE, 0, "", "no rule can prove another link; the board below is partial.", {}, {}, rep.final_board});
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
    for (size_t i = 0; i < ctx.solutions.size(); ++i) rep.solutions_text.push_back(ctx.solution_boards[i] + "Path: " + path_text(ctx.solutions[i]));
    rep.final_board = rep.solutions_text[0];
    if (ctx.solutions.size() >= 2) rep.outcome = ReasoningOutcome::MULTIPLE_SOLUTIONS;
    else if (rep.stats.budget_exhausted && opt.check_unique) rep.outcome = ReasoningOutcome::INCONCLUSIVE;
    else rep.outcome = rep.stats.branches == 0 ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::SOLVED_WITH_FALLBACK;
    return rep;
}

} // namespace solvedin
