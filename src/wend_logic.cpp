#include "solvedin/wend_logic.hpp"
#include "solvedin/wend_solver.hpp"
#include "solvedin/wend_validator.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace solvedin {

namespace wend_logic {

namespace {

std::string path_text(const WendWordPath& p) {
    std::string s = p.word + " ";
    for (size_t i = 0; i < p.path.size(); ++i) s += (i ? "-" : "") + cell_name(p.path[i]);
    return s;
}

std::string lengths_text(const std::map<int, int>& m) {
    std::string s;
    for (auto [len, k] : m) for (int i = 0; i < k; ++i) s += (s.empty() ? "" : ", ") + std::to_string(len);
    return "{" + s + "}";
}

struct View {
    int R, C;
    std::vector<std::vector<int>> by_cell;  // live paths covering each uncovered cell
    std::map<int, std::vector<int>> by_len;  // live paths per length
};

bool live_in(const State& s, int i, const std::vector<int>& owner, const std::map<int, int>& rem) {
    if (!s.alive[static_cast<size_t>(i)]) return false;
    auto it = rem.find(s.paths[static_cast<size_t>(i)].length());
    if (it == rem.end() || it->second <= 0) return false;
    for (int c : s.path_cells[static_cast<size_t>(i)]) if (owner[static_cast<size_t>(c)] != -1) return false;
    return true;
}

View view(const WendBoard& b, const State& s, const std::vector<int>& owner, const std::map<int, int>& rem, int skip = -1) {
    View v{b.rows(), b.cols(), std::vector<std::vector<int>>(static_cast<size_t>(b.rows() * b.cols())), {}};
    for (int i = 0; i < static_cast<int>(s.paths.size()); ++i) {
        if (i == skip || !live_in(s, i, owner, rem)) continue;
        v.by_len[s.paths[static_cast<size_t>(i)].length()].push_back(i);
        for (int c : s.path_cells[static_cast<size_t>(i)]) v.by_cell[static_cast<size_t>(c)].push_back(i);
    }
    return v;
}

// Can `size` be written as a sum of some of the remaining lengths (respecting multiplicity)?
bool subset_sum(int size, const std::map<int, int>& rem) {
    std::vector<char> ok(static_cast<size_t>(size + 1), 0);
    ok[0] = 1;
    for (auto [len, k] : rem)
        for (int t = 0; t < k; ++t)
            for (int x = size; x >= len; --x) if (ok[static_cast<size_t>(x - len)]) ok[static_cast<size_t>(x)] = 1;
    return ok[static_cast<size_t>(size)];
}

// Why the board (given owner/rem) is impossible, or "" if no obvious reason.
std::string dead_reason(const WendBoard& b, const State& s, const std::vector<int>& owner, const std::map<int, int>& rem, int skip,
                        Position* where) {
    View v = view(b, s, owner, rem, skip);
    const int R = b.rows(), C = b.cols();
    for (int q = 0; q < R * C; ++q)
        if (owner[static_cast<size_t>(q)] == -1 && v.by_cell[static_cast<size_t>(q)].empty()) {
            *where = {q / C, q % C};
            return cell_name(*where) + " '" + std::string(1, b.letter_at(*where)) + "' would have no dictionary word path left";
        }
    for (auto [len, k] : rem)
        if (k > 0 && static_cast<int>(v.by_len[len].size()) < k)
            return "the board still needs " + std::to_string(k) + " word" + (k > 1 ? "s" : "") + " of length " + std::to_string(len) + " but only " +
                   std::to_string(v.by_len[len].size()) + " path" + (v.by_len[len].size() == 1 ? "" : "s") + " would remain";
    // leftover areas
    std::vector<char> seen(static_cast<size_t>(R * C), 0);
    for (int q = 0; q < R * C; ++q) {
        if (owner[static_cast<size_t>(q)] != -1 || seen[static_cast<size_t>(q)]) continue;
        std::vector<int> stack{q}, area;
        seen[static_cast<size_t>(q)] = 1;
        while (!stack.empty()) {
            int x = stack.back(); stack.pop_back(); area.push_back(x);
            const int r = x / C, c = x % C, dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
            for (int d = 0; d < 4; ++d) {
                int rr = r + dr[d], cc = c + dc[d];
                if (rr < 0 || cc < 0 || rr >= R || cc >= C) continue;
                int y = rr * C + cc;
                if (owner[static_cast<size_t>(y)] == -1 && !seen[static_cast<size_t>(y)]) { seen[static_cast<size_t>(y)] = 1; stack.push_back(y); }
            }
        }
        if (!subset_sum(static_cast<int>(area.size()), rem)) {
            *where = {area[0] / C, area[0] % C};
            return "the open area of " + std::to_string(area.size()) + " cells around " + cell_name(*where) +
                   " could not be filled: no combination of the remaining lengths " + lengths_text(rem) + " adds up to " + std::to_string(area.size());
        }
    }
    return "";
}

} // namespace

State initial_state(const WendBoard& board, const Trie& lexicon) {
    State s;
    WendSolver solver(board, lexicon);
    s.paths = solver.enumerate_candidate_paths();
    const int C = board.cols();
    for (auto& p : s.paths) {
        std::vector<int> cells;
        for (auto q : p.path) cells.push_back(q.row * C + q.col);
        s.path_cells.push_back(cells);
    }
    s.alive.assign(s.paths.size(), 1);
    s.owner.assign(static_cast<size_t>(board.rows() * C), -1);
    for (int r = 0; r < board.rows(); ++r)
        for (int c = 0; c < C; ++c) if (board.is_void(r, c)) s.owner[static_cast<size_t>(r * C + c)] = -2;
    for (int len : board.target_lengths()) ++s.remaining[len];
    return s;
}

std::optional<Deduction> next_deduction(const WendBoard& b, const State& s) {
    Position where{};
    std::string dead = dead_reason(b, s, s.owner, s.remaining, -1, &where);
    if (!dead.empty()) { dead[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(dead[0]))); return Deduction{"dead", dead + ".", {where}, {}, {}, true}; }
    View v = view(b, s, s.owner, s.remaining);
    const int R = b.rows(), C = b.cols();
    // scarce cell
    for (int q = 0; q < R * C; ++q) {
        const auto& L = v.by_cell[static_cast<size_t>(q)];
        if (s.owner[static_cast<size_t>(q)] != -1 || L.size() != 1) continue;
        const Position p{q / C, q % C};
        int open = 0;
        const int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
        for (int d = 0; d < 4; ++d) {
            Position n{p.row + dr[d], p.col + dc[d]};
            if (b.is_in_bounds(n) && !b.is_void(n) && s.owner[static_cast<size_t>(n.row * C + n.col)] == -1) ++open;
        }
        return Deduction{"scarce-cell", cell_name(p) + " '" + std::string(1, b.letter_at(p)) + "' has " + std::to_string(open) +
                                            " open neighbour" + (open == 1 ? "" : "s") + ", and only one dictionary word path still covers it: " +
                                            path_text(s.paths[static_cast<size_t>(L[0])]) + ".",
                         {p}, {L[0]}, {}, false};
    }
    // forced length
    for (auto [len, k] : s.remaining) {
        if (k <= 0) continue;
        const auto& L = v.by_len[len];
        if (static_cast<int>(L.size()) != k) continue;
        bool disjoint = true;
        for (size_t i = 0; i < L.size() && disjoint; ++i)
            for (size_t j = i + 1; j < L.size() && disjoint; ++j)
                for (int c : s.path_cells[static_cast<size_t>(L[i])])
                    if (std::find(s.path_cells[static_cast<size_t>(L[j])].begin(), s.path_cells[static_cast<size_t>(L[j])].end(), c) != s.path_cells[static_cast<size_t>(L[j])].end()) { disjoint = false; break; }
        if (!disjoint) continue;
        return Deduction{"forced-length", "The board still needs " + std::to_string(k) + " word" + (k > 1 ? "s" : "") + " of length " + std::to_string(len) +
                                              " and exactly that many paths of that length remain, so " + path_text(s.paths[static_cast<size_t>(L[0])]) + " is placed.",
                         {s.paths[static_cast<size_t>(L[0])].path[0]}, {L[0]}, {}, false};
    }
    // one-step look-ahead
    size_t live_count = 0;
    for (auto& [len, L] : v.by_len) live_count += L.size();
    if (live_count > kMaxLookaheadPaths) return std::nullopt;
    for (auto& [len, L] : v.by_len) {
        for (int i : L) {
            std::vector<int> owner = s.owner;
            for (int c : s.path_cells[static_cast<size_t>(i)]) owner[static_cast<size_t>(c)] = i;
            std::map<int, int> rem = s.remaining;
            --rem[len];
            Position w{};
            std::string why = dead_reason(b, s, owner, rem, i, &w);
            if (why.empty()) continue;
            return Deduction{"would-strand", "Placing " + path_text(s.paths[static_cast<size_t>(i)]) + " is impossible: " + why + ".",
                             {w}, {}, {i}, false};
        }
    }
    return std::nullopt;
}

void apply(const WendBoard&, State& s, const Deduction& d) {
    for (int i : d.eliminate) s.alive[static_cast<size_t>(i)] = 0;
    for (int i : d.place) {
        s.placed.push_back(i);
        --s.remaining[s.paths[static_cast<size_t>(i)].length()];
        for (int c : s.path_cells[static_cast<size_t>(i)]) s.owner[static_cast<size_t>(c)] = i;
    }
}

std::string render(const WendBoard& b, const State& s) {
    // placed words: lower-case letters of the k-th placed word shown as digits/letters a, b, c...; open cells keep their letter
    std::ostringstream os;
    for (int r = 0; r < b.rows(); ++r) {
        for (int c = 0; c < b.cols(); ++c) {
            if (c) os << ' ';
            const int o = s.owner[static_cast<size_t>(r * b.cols() + c)];
            if (o == -2) { os << '#'; continue; }
            if (o == -1) { os << b.letter_at({r, c}); continue; }
            const auto it = std::find(s.placed.begin(), s.placed.end(), o);
            os << static_cast<char>('1' + (it - s.placed.begin()) % 9);
        }
        os << "\n";
    }
    for (size_t k = 0; k < s.placed.size(); ++k) os << (k % 9 + 1) << ": " << path_text(s.paths[static_cast<size_t>(s.placed[k])]) << "\n";
    return os.str();
}

} // namespace wend_logic

namespace {

using namespace wend_logic;

struct Ctx {
    const WendBoard& board;
    const Trie& lexicon;
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
    while (true) {
        if (ctx.over_budget()) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return true; }
        auto D = next_deduction(ctx.board, s);
        if (!D) break;
        ReasoningStep st{D->contradiction ? StepKind::CONTRADICTION : StepKind::DEDUCTION, SearchPhase::SOLVE, depth, D->rule, D->explanation, D->involved, {}, ""};
        if (D->contradiction) { ctx.log(std::move(st)); return false; }
        for (int i : D->place) st.changes.push_back({s.paths[static_cast<size_t>(i)].path[0], "word " + s.paths[static_cast<size_t>(i)].word});
        for (int i : D->eliminate) st.changes.push_back({s.paths[static_cast<size_t>(i)].path[0], "not " + s.paths[static_cast<size_t>(i)].word + " along this path"});
        apply(ctx.board, s, *D);
        ++ctx.rep.stats.deductions;
        ++ctx.rep.stats.rule_counts[D->rule];
        ctx.log(std::move(st));
        if (single) break;
    }
    return true;
}

bool complete(const State& s) {
    for (int o : s.owner) if (o == -1) return false;
    for (auto [len, k] : s.remaining) if (k != 0) return false;
    return true;
}

void search(Ctx& ctx, State s, int depth) {
    if (ctx.stop) return;
    if (ctx.over_budget()) { ctx.rep.stats.budget_exhausted = true; ctx.stop = true; return; }
    ++ctx.nodes();
    if (!deduce_all(ctx, s, depth)) { if (depth > 0) ++ctx.rep.stats.failed_branches; return; }
    if (ctx.stop) return;
    if (complete(s)) {
        WendSolution sol;
        for (int i : s.placed) sol.words.push_back(s.paths[static_cast<size_t>(i)]);
        auto v = WendValidator::validate(ctx.board, sol, ctx.lexicon);
        if (!v.is_valid) {
            ctx.log({StepKind::CONTRADICTION, SearchPhase::SOLVE, depth, "validator", v.error_message, {}, {}, ""});
            if (depth > 0) ++ctx.rep.stats.failed_branches;
            return;
        }
        ctx.log({StepKind::SOLUTION, SearchPhase::SOLVE, depth, "", "every cell is covered by dictionary words of the shown lengths; the independent validator accepts it.", {}, {}, render(ctx.board, s)});
        ctx.solutions.push_back(render(ctx.board, s));
        if (ctx.solutions.size() == 1) { ctx.first_solution_at = std::chrono::steady_clock::now(); ctx.phase = SearchPhase::VERIFY_UNIQUE; }
        if (ctx.solutions.size() >= ctx.max_solutions) ctx.stop = true;
        return;
    }
    // fewest-option open cell
    const int R = ctx.board.rows(), C = ctx.board.cols();
    std::vector<std::vector<int>> by_cell(static_cast<size_t>(R * C));
    for (int i = 0; i < static_cast<int>(s.paths.size()); ++i) {
        if (!live_in(s, i, s.owner, s.remaining)) continue;
        for (int c : s.path_cells[static_cast<size_t>(i)]) by_cell[static_cast<size_t>(c)].push_back(i);
    }
    int best = -1;
    for (int q = 0; q < R * C; ++q)
        if (s.owner[static_cast<size_t>(q)] == -1 && (best < 0 || by_cell[static_cast<size_t>(q)].size() < by_cell[static_cast<size_t>(best)].size())) best = q;
    if (best < 0) return;
    const auto options = by_cell[static_cast<size_t>(best)];
    for (size_t i = 0; i < options.size() && !ctx.stop; ++i) {
        const auto& p = s.paths[static_cast<size_t>(options[i])];
        std::string why = i == 0 ? "no rule applies. " + cell_name({best / C, best % C}) + " has the fewest word paths (" + std::to_string(options.size()) +
                                       "); try " + p.word + "."
                                 : (ctx.phase == SearchPhase::VERIFY_UNIQUE ? "a solution is known; to prove it unique, try " + p.word + "."
                                                                            : "the previous word failed; try " + p.word + ".");
        ctx.log({StepKind::BRANCH, SearchPhase::SOLVE, depth, "guess", why, {p.path[0]}, {{p.path[0], "word " + p.word}}, i == 0 ? render(ctx.board, s) : ""});
        ++ctx.rep.stats.branches;
        State next = s;
        apply(ctx.board, next, Deduction{"", "", {}, {options[i]}, {}, false});
        search(ctx, std::move(next), depth + 1);
    }
}

} // namespace

ReasoningReport reason_wend(const WendBoard& board, const Trie& lexicon, const std::string& lexicon_name, const ReasoningOptions& opt) {
    ReasoningReport rep;
    Ctx ctx{board, lexicon, opt, rep, std::chrono::steady_clock::now(), {}, SearchPhase::SOLVE, {}, opt.check_unique ? size_t{2} : size_t{1}, false};
    auto elapsed = [&](auto from) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count(); };
    State s = initial_state(board, lexicon);
    rep.stats.work["candidate_word_paths"] = s.paths.size();
    rep.notes.push_back("every deduction assumes the dictionary " + lexicon_name + " is complete for this board: a word it lacks is treated as impossible, "
                        "so a missing word shows up as NO_SOLUTION or a stall, never as a wrong placement. Candidate word paths: " +
                        std::to_string(s.paths.size()) + ".");
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
        else if (complete(s)) rep.outcome = ReasoningOutcome::SOLVED_BY_DEDUCTION;
        else if (rep.stats.budget_exhausted) rep.outcome = ReasoningOutcome::INCONCLUSIVE;
        else {
            rep.outcome = ReasoningOutcome::STALLED;
            rep.steps.push_back({StepKind::STALL, SearchPhase::SOLVE, 0, "", "no rule can prove another word or elimination; the board below is partial.", {}, {}, rep.final_board});
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
