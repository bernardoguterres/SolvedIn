// Explainable Pinpoint: per-reveal hypotheses, no peeking at later clues, compound connections, abstention.
#include "solvedin/pinpoint.hpp"
#include "solvedin/pinpoint_logic.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_set>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

using namespace solvedin;

int main() {
    auto lex = PinpointLexicon::load_from_file("fixtures/pinpoint_lexicon.txt");
    // 1. lexicon category: answered once two clues agree, and every step is a hypothesis
    auto drives = PinpointGuesser::load_clues_from_file("fixtures/pinpoint_clues_drives.txt");
    auto r = reason_pinpoint(drives, lex, nullptr);
    CHECK_ASSERT(r.outcome == ReasoningOutcome::ANSWERED);
    CHECK_ASSERT(r.final_board.find("Storage") != std::string::npos);
    CHECK_ASSERT(r.steps.size() >= 2);  // never commits on the first clue
    for (const auto& s : r.steps) CHECK_ASSERT(s.kind == StepKind::HYPOTHESIS);
    // the step for reveal k mentions no later clue
    for (size_t k = 0; k < r.steps.size(); ++k)
        for (size_t j = k + 1; j < drives.size(); ++j)
            CHECK_ASSERT(r.steps[k].explanation.find("(" + drives[j] + ")") == std::string::npos);
    std::cout << "[PASS] lexicon hypothesis answered after two clues; no step reads a later clue\n";

    // 2. compound generator: FIRE before LIGHT / HOUSE / FLY, shown with its connections but not answered by default
    std::unordered_set<std::string> words{"FIRE", "LIGHT", "HOUSE", "FLY", "FIRELIGHT", "FIREHOUSE", "FIREFLY", "WORK", "FIREWORK", "BOAT", "HOUSEBOAT"};
    std::vector<std::string> fire{"Light", "House", "Fly"};
    auto c = reason_pinpoint(fire, lex, &words);
    CHECK_ASSERT(c.outcome == ReasoningOutcome::ABSTAINED);
    CHECK_ASSERT(c.steps.back().explanation.find("Words that come after \"Fire\"") != std::string::npos);
    CHECK_ASSERT(c.steps.back().explanation.find("FIRE + FLY = FIREFLY") != std::string::npos);
    PinpointReasoningOptions o;
    o.max_compound_options = 1;
    auto c2 = reason_pinpoint(fire, lex, &words, o);
    CHECK_ASSERT(c2.outcome == ReasoningOutcome::ANSWERED && c2.final_board == "Words that come after \"Fire\"");
    // a plural clue matches its singular compound in a word list without plurals
    auto c3 = reason_pinpoint({"Light", "Works"}, lex, &words, o);
    CHECK_ASSERT(c3.steps.back().explanation.find("FIREWORK(S)") != std::string::npos);
    // a phrase clue is not testable and does not remove the hypothesis
    auto c4 = reason_pinpoint({"Light", "In numbers", "Fly"}, lex, &words, o);
    CHECK_ASSERT(c4.steps.back().explanation.find("phrase, not testable") != std::string::npos);
    std::cout << "[PASS] compound hypotheses show their connection; answered only when the policy allows\n";

    // 3. weak evidence abstains
    auto weak = PinpointGuesser::load_clues_from_file("fixtures/pinpoint_clues_weak.txt");
    CHECK_ASSERT(reason_pinpoint(weak, lex, nullptr).outcome == ReasoningOutcome::ABSTAINED);
    std::cout << "[PASS] weak evidence abstains\n";
    std::cout << "All Pinpoint logic tests passed.\n";
    return 0;
}
