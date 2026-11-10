#include "solvedin/pinpoint_logic.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <map>

namespace solvedin {

namespace {

std::string letters_upper(const std::string& s) {
    std::string o;
    for (char c : s) if (std::isalpha(static_cast<unsigned char>(c))) o += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return o;
}

std::string title(const std::string& up) {
    std::string o = up;
    for (size_t i = 1; i < o.size(); ++i) o[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(o[i])));
    return o;
}

struct Affix {
    std::string x;
    bool before{true};                   // X comes before each clue (X + clue)
    std::vector<std::string> compounds;  // per revealed clue
    size_t breadth{0};                   // how many dictionary compounds X forms in that position (lower = more specific)
    [[nodiscard]] std::string category() const {
        return before ? "Words that come after \"" + title(x) + "\"" : "Words that come before \"" + title(x) + "\"";
    }
};

// Affixes X such that X+clue (or clue+X) is a dictionary word for every clue, with X itself a word of 3+ letters.
std::vector<Affix> affixes(const std::vector<std::string>& clues, const std::unordered_set<std::string>& words) {
    std::map<std::pair<std::string, bool>, std::vector<std::string>> acc;
    bool first = true;
    for (size_t k = 0; k < clues.size(); ++k) {
        const std::string full = letters_upper(clues[k]);
        // a phrase clue ("in numbers") cannot be a closed compound: it is reported as not testable and does not filter
        if (clues[k].find_first_of(" -") != std::string::npos || full.size() < 2) {
            for (auto& [key, v] : acc) v.push_back("(" + clues[k] + ": phrase, not testable)");
            continue;
        }
        std::map<std::pair<std::string, bool>, std::string> here;
        // word lists such as web2 carry no plurals, so a plural clue also matches in its singular form (WORKS -> FIREWORK)
        std::vector<std::string> forms{full};
        if (full.size() > 3 && full.back() == 'S') forms.push_back(full.substr(0, full.size() - 1));
        for (const auto& c : forms) {
            for (const auto& w : words) {
                if (w.size() < c.size() + 3) continue;
                if (w.compare(w.size() - c.size(), c.size(), c) == 0) {
                    std::string x = w.substr(0, w.size() - c.size());
                    if (words.count(x)) here.emplace(std::make_pair(x, true), c == full ? w : w + "(S)");
                }
                if (w.compare(0, c.size(), c) == 0) {
                    std::string x = w.substr(c.size());
                    if (words.count(x)) here.emplace(std::make_pair(x, false), c == full ? w : w + "(S)");
                }
            }
        }
        if (first) {
            first = false;
            for (auto& [key, w] : here) {
                std::vector<std::string> v;
                for (size_t j = 0; j < k; ++j) v.push_back("(" + clues[j] + ": phrase, not testable)");
                v.push_back(w);
                acc[key] = v;
            }
        } else {
            for (auto it = acc.begin(); it != acc.end();) {
                auto h = here.find(it->first);
                if (h == here.end()) it = acc.erase(it); else { it->second.push_back(h->second); ++it; }
            }
        }
        if (!first && acc.empty()) return {};
    }
    if (first) return {};  // no testable clue yet
    std::vector<Affix> out;
    for (auto& [key, comps] : acc) out.push_back({key.first, key.second, comps, 0});
    if (out.size() <= 20) {  // breadth only for the short list that is shown or ranked
        for (auto& a : out)
            for (const auto& w : words) {
                if (w.size() < a.x.size() + 3) continue;
                if (a.before && w.compare(0, a.x.size(), a.x) == 0 && words.count(w.substr(a.x.size()))) ++a.breadth;
                if (!a.before && w.compare(w.size() - a.x.size(), a.x.size(), a.x) == 0 && words.count(w.substr(0, w.size() - a.x.size()))) ++a.breadth;
            }
    }
    std::sort(out.begin(), out.end(), [](const Affix& p, const Affix& q) { return p.breadth != q.breadth ? p.breadth < q.breadth : p.x < q.x; });
    return out;
}

} // namespace

ReasoningReport reason_pinpoint(const std::vector<std::string>& clues, const PinpointLexicon& lexicon,
                                const std::unordered_set<std::string>* words, const PinpointReasoningOptions& opt) {
    ReasoningReport rep;
    const auto t0 = std::chrono::steady_clock::now();
    rep.notes.push_back("every suggestion below is a hypothesis ranked on the revealed clues only; later clues are not read until they are revealed.");
    rep.notes.push_back(std::string("generators: category lexicon (") + std::to_string(lexicon.size()) + " categories)" +
                        (words ? ", and closed-compound words before/after every clue (" + std::to_string(words->size()) + "-word list)" : ", compound generator off (pass --dict <word list>)") + ".");
    PinpointGuesser guesser(lexicon, opt.lexicon_threshold);
    std::string decision_text;
    bool answered = false;
    for (size_t k = 1; k <= clues.size(); ++k) {
        std::vector<std::string> seen(clues.begin(), clues.begin() + static_cast<long>(k));
        auto g = guesser.guess(seen);
        std::string text = "after clue " + std::to_string(k) + " (" + clues[k - 1] + "):";
        bool lex_ok = false;
        std::string lex_cat;
        if (!g.ranked_guesses.empty()) {
            const auto& top = g.ranked_guesses[0];
            lex_ok = !g.is_unknown && top.unsupported_clues.empty();
            lex_cat = top.category_name;
            text += "\n      lexicon: \"" + top.category_name + "\" (confidence " + std::to_string(static_cast<int>(top.confidence * 100 + 0.5)) +
                    "%, lists " + std::to_string(top.supported_clues.size()) + " of " + std::to_string(k) + " clues" +
                    (top.unsupported_clues.empty() ? "" : "; not listed: " + [&] { std::string s; for (auto& u : top.unsupported_clues) s += (s.empty() ? "" : ", ") + u; return s; }()) + ")";
        } else {
            text += "\n      lexicon: no category lists these clues";
        }
        std::vector<Affix> A;
        if (words) {
            A = affixes(seen, *words);
            rep.stats.work["compound_hypotheses_checked"] += A.size();
            if (A.empty()) text += "\n      compound: no single word goes before or after every clue";
            for (size_t i = 0; i < A.size() && i < 3; ++i) {
                std::string conn;
                for (size_t j = 0; j < A[i].compounds.size(); ++j)
                    conn += (j ? ", " : "") + (A[i].compounds[j].front() == '(' ? A[i].compounds[j]
                            : (A[i].before ? A[i].x + " + " + letters_upper(seen[j]) : letters_upper(seen[j]) + " + " + A[i].x) + " = " + A[i].compounds[j]);
                text += "\n      compound: " + A[i].category() + "  [" + conn + "]" + (A[i].breadth ? "  (\"" + title(A[i].x) + "\" forms " + std::to_string(A[i].breadth) + " compounds in that position)" : "");
            }
            if (A.size() > 3) text += "\n      compound: ... " + std::to_string(A.size() - 3) + " more affixes also fit";
        }
        const bool comp_ok = static_cast<int>(k) >= opt.min_clues_to_answer && !A.empty() && A.size() <= opt.max_compound_options;
        if (static_cast<int>(k) < opt.min_clues_to_answer) decision_text = "too early to commit; ask for the next clue.";
        else if (lex_ok) { decision_text = "answer \"" + lex_cat + "\" (lexicon category listing every revealed clue)."; answered = true; rep.final_board = lex_cat; }
        else if (comp_ok) { decision_text = "answer " + A[0].category() + " (the only affix that fits every revealed clue)."; answered = true; rep.final_board = A[0].category(); }
        else {
            decision_text = !A.empty() && opt.max_compound_options == 0
                ? "compound hypotheses are shown but not answered (word lists miss open compounds); ask for the next clue."
                : A.size() > opt.max_compound_options ? "several hypotheses fit; ask for the next clue." : "evidence too weak; ask for the next clue.";
            answered = false;
        }
        text += "\n      decision: " + decision_text;
        rep.steps.push_back({StepKind::HYPOTHESIS, SearchPhase::SOLVE, 0, "reveal-" + std::to_string(k), text, {}, {}, ""});
        if (answered) break;
    }
    rep.outcome = answered ? ReasoningOutcome::ANSWERED : ReasoningOutcome::ABSTAINED;
    if (!answered) rep.final_board = "(abstain: no hypothesis explains the revealed clues well enough)";
    rep.stats.solution_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return rep;
}

} // namespace solvedin
