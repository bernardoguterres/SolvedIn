#include "solvedin/patches_logic.hpp"
#include "solvedin/patches_validator.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace solvedin {

namespace patches_logic {

namespace {

char letter(int k) { return k < 26 ? static_cast<char>('A' + k) : k < 52 ? static_cast<char>('a' + k - 26) : '#'; }

std::string clue_name(const PatchesBoard& b, int k) {
    const auto& cl = b.clues()[static_cast<size_t>(k)];
    return "clue " + std::string(1, letter(k)) + " ('" + cl.label() + "' at " + cell_name(cl.pos) + ")";
}

std::string rect_text(const Rect& r) {
    return "rows " + std::to_string(r.r0) + "-" + std::to_string(r.r0 + r.h - 1) + ", columns " + std::to_string(r.c0) + "-" +
           std::to_string(r.c0 + r.w - 1) + " (" + std::to_string(r.h) + "x" + std::to_string(r.w) + ")";
}

struct View {
    int R, C;
    std::vector<std::vector<int>> by_clue;  // live rect indices per unplaced clue
    std::vector<std::vector<int>> by_cell;  // live rect indices covering each uncovered cell
};

bool live(const State& s, int C, int i) {
    const Rect& r = s.rects[static_cast<size_t>(i)];
    if (!s.alive[static_cast<size_t>(i)] || s.placed[static_cast<size_t>(r.clue)] >= 0) return false;
    for (int y = r.r0; y < r.r0 + r.h; ++y)
        for (int x = r.c0; x < r.c0 + r.w; ++x)
            if (s.owner[static_cast<size_t>(y * C + x)] >= 0) return false;
    return true;
}

View view(const PatchesBoard& b, const State& s) {
    View v{b.rows(), b.cols(), std::vector<std::vector<int>>(b.num_clues()), std::vector<std::vector<int>>(static_cast<size_t>(b.num_cells()))};
    for (int i = 0; i < static_cast<int>(s.rects.size()); ++i) {
        if (!live(s, v.C, i)) continue;
        const Rect& r = s.rects[static_cast<size_t>(i)];
        v.by_clue[static_cast<size_t>(r.clue)].push_back(i);
        for (int y = r.r0; y < r.r0 + r.h; ++y)
            for (int x = r.c0; x < r.c0 + r.w; ++x) v.by_cell[static_cast<size_t>(y * v.C + x)].push_back(i);
    }
    return v;
}

} // namespace

State initial_state(const PatchesBoard& b) {
    State s;
    const int R = b.rows(), C = b.cols(), total = b.num_cells();
    const size_t K = b.num_clues();
    s.placed.assign(K, -1);
    s.owner.assign(static_cast<size_t>(total), -1);
    // Same size ranges and filters as the exact-cover solver.
    std::vector<int> min_size(K, 1);
    int total_min = 0, specified = 0;
    bool all_sized = true;
    for (size_t k = 0; k < K; ++k) {
        const auto& cl = b.clues()[k];
        if (cl.has_size()) { min_size[k] = *cl.size; specified += *cl.size; }
        else { all_sized = false; min_size[k] = (cl.shape == ShapeConstraint::TALL || cl.shape == ShapeConstraint::WIDE) ? 2 : 1; }
        total_min += min_size[k];
    }
    if (K == 0 || (all_sized && specified != total) || total_min > total) {
        s.enumeration_failed = true;
        s.failure = K == 0 ? "the board has no clues." : "the clue areas cannot add up to the " + std::to_string(total) + " cells of the board.";
        return s;
    }
    for (size_t k = 0; k < K; ++k) {
        const auto& cl = b.clues()[k];
        const int lo = cl.has_size() ? *cl.size : min_size[k];
        const int hi = cl.has_size() ? *cl.size : total - (total_min - min_size[k]);
        for (int h = 1; h <= R; ++h)
            for (int w = 1; w <= C; ++w) {
                const int a = h * w;
                if (a < lo || a > hi) continue;
                if (cl.shape == ShapeConstraint::SQUARE && w != h) continue;
                if (cl.shape == ShapeConstraint::TALL && h <= w) continue;
                if (cl.shape == ShapeConstraint::WIDE && w <= h) continue;
                for (int r0 = std::max(0, cl.pos.row - h + 1); r0 <= std::min(R - h, cl.pos.row); ++r0)
                    for (int c0 = std::max(0, cl.pos.col - w + 1); c0 <= std::min(C - w, cl.pos.col); ++c0) {
                        bool other = false;
                        for (int y = r0; y < r0 + h && !other; ++y)
                            for (int x = c0; x < c0 + w; ++x)
                                if ((y != cl.pos.row || x != cl.pos.col) && b.has_clue(y, x)) { other = true; break; }
                        if (!other) s.rects.push_back({static_cast<int>(k), r0, c0, h, w});
                    }
            }
    }
    s.alive.assign(s.rects.size(), 1);
    return s;
}

std::optional<Deduction> next_deduction(const PatchesBoard& b, const State& s) {
    if (s.enumeration_failed) return Deduction{"areas", s.failure, {}, {}, {}, true};
    View v = view(b, s);
    const int K = static_cast<int>(b.num_clues());
    for (int k = 0; k < K; ++k)
        if (s.placed[static_cast<size_t>(k)] < 0 && v.by_clue[static_cast<size_t>(k)].empty())
            return Deduction{"no-rectangle", clue_name(b, k) + " has no rectangle left that fits.", {b.clues()[static_cast<size_t>(k)].pos}, {}, {}, true};
    for (int q = 0; q < v.R * v.C; ++q)
        if (s.owner[static_cast<size_t>(q)] < 0 && v.by_cell[static_cast<size_t>(q)].empty())
            return Deduction{"stranded-cell", cell_name({q / v.C, q % v.C}) + " can no longer be covered by any clue's rectangle.", {{q / v.C, q % v.C}}, {}, {}, true};

    // a clue with one rectangle left
    for (int k = 0; k < K; ++k) {
        const auto& L = v.by_clue[static_cast<size_t>(k)];
        if (s.placed[static_cast<size_t>(k)] < 0 && L.size() == 1)
            return Deduction{"single-rectangle", clue_name(b, k) + " has only one rectangle left: " + rect_text(s.rects[static_cast<size_t>(L[0])]) + ".",
                             {b.clues()[static_cast<size_t>(k)].pos}, {L[0]}, {}, false};
    }
    // a cell only one rectangle can cover
    for (int q = 0; q < v.R * v.C; ++q) {
        const auto& L = v.by_cell[static_cast<size_t>(q)];
        if (s.owner[static_cast<size_t>(q)] >= 0 || L.size() != 1) continue;
        const Rect& r = s.rects[static_cast<size_t>(L[0])];
        return Deduction{"only-cover", cell_name({q / v.C, q % v.C}) + " can only be covered by one rectangle: " + clue_name(b, r.clue) + " as " + rect_text(r) + ".",
                         {{q / v.C, q % v.C}}, {L[0]}, {}, false};
    }
    // a cell every rectangle of a clue covers is reserved for that clue
    for (int k = 0; k < K; ++k) {
        const auto& L = v.by_clue[static_cast<size_t>(k)];
        if (s.placed[static_cast<size_t>(k)] >= 0 || L.empty()) continue;
        for (int q = 0; q < v.R * v.C; ++q) {
            if (s.owner[static_cast<size_t>(q)] >= 0) continue;
            const int y = q / v.C, x = q % v.C;
            if (!std::all_of(L.begin(), L.end(), [&](int i) { return s.rects[static_cast<size_t>(i)].covers(y, x); })) continue;
            Deduction D{"reserved-cell", "", {{y, x}}, {}, {}, false};
            for (int i : v.by_cell[static_cast<size_t>(q)]) if (s.rects[static_cast<size_t>(i)].clue != k) D.eliminate.push_back(i);
            if (D.eliminate.empty()) continue;
            D.explanation = "Every remaining rectangle of " + clue_name(b, k) + " (" + std::to_string(L.size()) + ") covers " + cell_name({y, x}) +
                            ", so that cell belongs to it: " + std::to_string(D.eliminate.size()) + " rectangle" + (D.eliminate.size() == 1 ? "" : "s") +
                            " of other clues through it " + (D.eliminate.size() == 1 ? "is" : "are") + " removed.";
            return D;
        }
    }
    // a cell only one clue can reach
    for (int q = 0; q < v.R * v.C; ++q) {
        const auto& L = v.by_cell[static_cast<size_t>(q)];
        if (s.owner[static_cast<size_t>(q)] >= 0 || L.empty()) continue;
        const int k = s.rects[static_cast<size_t>(L[0])].clue;
        if (!std::all_of(L.begin(), L.end(), [&](int i) { return s.rects[static_cast<size_t>(i)].clue == k; })) continue;
        const int y = q / v.C, x = q % v.C;
        Deduction D{"only-clue", "", {{y, x}}, {}, {}, false};
        for (int i : v.by_clue[static_cast<size_t>(k)]) if (!s.rects[static_cast<size_t>(i)].covers(y, x)) D.eliminate.push_back(i);
        if (D.eliminate.empty()) continue;
        D.explanation = "Only " + clue_name(b, k) + " can reach " + cell_name({y, x}) + ", so its rectangle must cover that cell: its " +
                        std::to_string(D.eliminate.size()) + " rectangle" + (D.eliminate.size() == 1 ? "" : "s") + " that miss it " +
                        (D.eliminate.size() == 1 ? "is" : "are") + " removed.";
        return D;
    }
    return std::nullopt;
}

void apply(const PatchesBoard& b, State& s, const Deduction& d) {
    for (int i : d.eliminate) s.alive[static_cast<size_t>(i)] = 0;
    for (int i : d.place) {
        const Rect& r = s.rects[static_cast<size_t>(i)];
        s.placed[static_cast<size_t>(r.clue)] = i;
        for (int y = r.r0; y < r.r0 + r.h; ++y)
            for (int x = r.c0; x < r.c0 + r.w; ++x) s.owner[static_cast<size_t>(y * b.cols() + x)] = r.clue;
    }
}

std::string render(const PatchesBoard& b, const State& s) {
    std::ostringstream os;
    for (int r = 0; r < b.rows(); ++r) {
        for (int c = 0; c < b.cols(); ++c) {
            if (c) os << ' ';
            int o = s.owner[static_cast<size_t>(r * b.cols() + c)];
            os << (o >= 0 ? letter(o) : '.');
        }
        os << "\n";
    }
    return os.str();
}

} // namespace patches_logic

namespace {

using namespace patches_logic;

struct Ctx {
    const PatchesBoard& board;
    const ReasoningOptions& opt;
    ReasoningReport& rep;
    std::chrono::steady_clock::time_point start, first_solution_at;
    SearchPhase phase{SearchPhase::SOLVE};
    std::vector<std::string> solutions;
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
        for (int i : D->place) {
            const Rect& r = s.rects[static_cast<size_t>(i)];
            st.changes.push_back({ctx.board.clues()[static_cast<size_t>(r.clue)].pos, "patch rows " + std::to_string(r.r0) + "-" + std::to_string(r.r0 + r.h - 1) +
                                                                                        " cols " + std::to_string(r.c0) + "-" + std::to_string(r.c0 + r.w - 1)});
        }
        apply(ctx.board, s, *D);
        ++ctx.rep.stats.deductions;
        ++ctx.rep.stats.rule_counts[D->rule];
        ctx.log(std::move(st));
        if (single) break;
    }
    return true;
}

bool complete(const State& s) {
    return std::all_of(s.placed.begin(), s.placed.end(), [](int p) { return p >= 0; }) &&
           std::all_of(s.owner.begin(), s.owner.end(), [](int o) { return o >= 0; });
}

PatchesSolution to_solution(const PatchesBoard& b, const State& s) {
    PatchesSolution sol;
    for (size_t k = 0; k < s.placed.size(); ++k) {
        const Rect& r = s.rects[static_cast<size_t>(s.placed[k])];
        Patch p;
        p.clue_pos = b.clues()[k].pos;
        for (int y = r.r0; y < r.r0 + r.h; ++y)
            for (int x = r.c0; x < r.c0 + r.w; ++x) p.cells.push_back({y, x});
        sol.patches.push_back(p);
    }
    return sol;
}

void search(Ctx& ctx, State s, int depth) {
    if (ctx.stop) return;
    if (ctx.over_budget()) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return; }
    ++ctx.nodes();
    if (!deduce_all(ctx, s, depth)) { if (depth > 0) ++ctx.rep.stats.failed_branches; return; }
    if (complete(s)) {
        auto v = validate_patches_solution(ctx.board, to_solution(ctx.board, s));
        if (!v.is_valid) {
            ctx.log({StepKind::CONTRADICTION, SearchPhase::SOLVE, depth, "validator", v.error_message, {}, {}, ""});
            if (depth > 0) ++ctx.rep.stats.failed_branches;
            return;
        }
        ctx.log({StepKind::SOLUTION, SearchPhase::SOLVE, depth, "", "every clue has a rectangle, the board is covered, and the independent validator accepts it.", {}, {}, render(ctx.board, s)});
        ctx.solutions.push_back(render(ctx.board, s));
        if (ctx.solutions.size() == 1) { ctx.first_solution_at = std::chrono::steady_clock::now(); ctx.phase = SearchPhase::VERIFY_UNIQUE; }
        if (ctx.solutions.size() >= ctx.max_solutions) ctx.stop = true;
        return;
    }
    // most constrained clue or cell
    View v = view(ctx.board, s);
    std::vector<int> options;
    std::string what;
    for (size_t k = 0; k < v.by_clue.size(); ++k)
        if (s.placed[k] < 0 && (options.empty() || v.by_clue[k].size() < options.size())) { options = v.by_clue[k]; what = clue_name(ctx.board, static_cast<int>(k)); }
    for (int q = 0; q < v.R * v.C; ++q)
        if (s.owner[static_cast<size_t>(q)] < 0 && v.by_cell[static_cast<size_t>(q)].size() < options.size()) {
            options = v.by_cell[static_cast<size_t>(q)];
            what = "cell " + cell_name({q / v.C, q % v.C});
        }
    if (options.empty()) return;
    for (size_t i = 0; i < options.size() && !ctx.stop; ++i) {
        const Rect& r = s.rects[static_cast<size_t>(options[i])];
        std::string why = i == 0 ? "no rule applies. " + what + " has the fewest options (" + std::to_string(options.size()) + "); try " +
                                       clue_name(ctx.board, r.clue) + " as " + rect_text(r) + "."
                                 : (ctx.phase == SearchPhase::VERIFY_UNIQUE ? "a solution is known; to prove it unique, try the next option for " + what + "."
                                                                            : "the previous option failed; try the next one for " + what + ".");
        ctx.log({StepKind::BRANCH, SearchPhase::SOLVE, depth, "guess", why, {ctx.board.clues()[static_cast<size_t>(r.clue)].pos}, {}, i == 0 ? render(ctx.board, s) : ""});
        ++ctx.rep.stats.branches;
        State next = s;
        apply(ctx.board, next, Deduction{"", "", {}, {options[i]}, {}, false});
        search(ctx, std::move(next), depth + 1);
    }
}

} // namespace

ReasoningReport reason_patches(const PatchesBoard& board, const ReasoningOptions& opt) {
    ReasoningReport rep;
    Ctx ctx{board, opt, rep, std::chrono::steady_clock::now(), {}, SearchPhase::SOLVE, {}, opt.check_unique ? size_t{2} : size_t{1}, false};
    auto elapsed = [&](auto from) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };
    State s = initial_state(board);
    rep.stats.work["candidate_rectangles"] = s.rects.size();
    if (opt.mode != ReasoningMode::EXPLAIN) {
        ctx.nodes() = 1;
        bool ok = deduce_all(ctx, s, 0, opt.mode == ReasoningMode::HINT);
        rep.final_board = render(board, s);
        rep.stats.solution_ms = elapsed(ctx.start);
        if (opt.mode == ReasoningMode::HINT) {
            rep.outcome = rep.steps.empty() ? ReasoningOutcome::NO_HINT : (ok ? ReasoningOutcome::HINT : ReasoningOutcome::NO_SOLUTION);
            return rep;
        }
        if (!ok) rep.outcome = ReasoningOutcome::NO_SOLUTION;
        else if (complete(s)) rep.outcome = validate_patches_solution(board, to_solution(board, s)).is_valid ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::NO_SOLUTION;
        else {
            rep.outcome = ReasoningOutcome::STALLED;
            rep.steps.push_back({StepKind::STALL, SearchPhase::SOLVE, 0, "", "no rule can prove another rectangle or elimination; the board below is partial.", {}, {}, rep.final_board});
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
    rep.solutions_text = ctx.solutions;
    rep.final_board = rep.solutions_text[0];
    if (ctx.solutions.size() >= 2) rep.outcome = ReasoningOutcome::MULTIPLE_SOLUTIONS;
    else if (rep.stats.budget_exhausted && opt.check_unique) rep.outcome = ReasoningOutcome::INCONCLUSIVE;
    else rep.outcome = rep.stats.branches == 0 ? ReasoningOutcome::SOLVED_BY_DEDUCTION : ReasoningOutcome::SOLVED_WITH_FALLBACK;
    return rep;
}

} // namespace solvedin
