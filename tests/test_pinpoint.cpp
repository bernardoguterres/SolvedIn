#include "solvedin/pinpoint.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace solvedin;

void test_pinpoint_lexicon_load() {
    std::cout << "Testing Pinpoint lexicon loader..." << std::endl;

    auto lex = PinpointLexicon::load_from_file("fixtures/pinpoint_lexicon.txt");
    assert(!lex.empty());
    assert(lex.size() >= 10);

    [[maybe_unused]] bool found_drives = false;
    for (const auto& cat : lex.categories()) {
        if (cat.name == "Computer Storage Drives") {
            found_drives = true;
            assert(!cat.terms.empty());
            assert(!cat.description.empty());
        }
    }
    assert(found_drives); (void)found_drives;
}

void test_pinpoint_in_lexicon_ranking() {
    std::cout << "Testing Pinpoint in-lexicon ranking quality..." << std::endl;

    auto lex = PinpointLexicon::load_from_file("fixtures/pinpoint_lexicon.txt");
    PinpointGuesser guesser(lex, 0.50);

    // Case 1: 5 clues for Computer Storage Drives
    auto clues_drives = PinpointGuesser::load_clues_from_file("fixtures/pinpoint_clues_drives.txt");
    assert(clues_drives.size() == 5);

    auto res1 = guesser.guess(clues_drives);
    assert(!res1.is_unknown);
    assert(res1.status == SolveStatus::UNIQUE_SOLUTION);
    assert(!res1.ranked_guesses.empty());
    assert(res1.ranked_guesses[0].category_name == "Computer Storage Drives");
    assert(res1.ranked_guesses[0].confidence == 1.0);
    assert(res1.ranked_guesses[0].supported_clues.size() == 5);
    assert(res1.ranked_guesses[0].unsupported_clues.empty());

    // Case 2: 4 clues for Necklines
    std::vector<std::string> clues_neck = {"CREW", "TURTLE", "V", "BOAT"};
    auto res2 = guesser.guess(clues_neck);
    assert(!res2.is_unknown);
    assert(res2.ranked_guesses[0].category_name == "Types of Necklines");
    assert(res2.ranked_guesses[0].confidence == 1.0);

    // Case 3: Partial support (3 out of 4)
    std::vector<std::string> clues_partial = {"LATTE", "MOCHA", "ESPRESSO", "CROISSANT"};
    auto res3 = guesser.guess(clues_partial);
    assert(!res3.is_unknown);
    assert(res3.ranked_guesses[0].category_name == "Coffee Drinks");
    assert(res3.ranked_guesses[0].supported_clues.size() == 3);
    assert(res3.ranked_guesses[0].unsupported_clues.size() == 1);
    assert(res3.ranked_guesses[0].unsupported_clues[0] == "CROISSANT");
}

void test_pinpoint_ambiguity() {
    std::cout << "Testing Pinpoint ambiguous / tied clues..." << std::endl;

    auto lex = PinpointLexicon::load_from_file("fixtures/pinpoint_lexicon.txt");
    PinpointGuesser guesser(lex, 0.50);

    // "DIAMOND" matches both Card Suits and Precious Gemstones
    std::vector<std::string> ambiguous_clues = {"DIAMOND"};
    auto res = guesser.guess(ambiguous_clues);
    assert(!res.is_unknown);
    assert(res.status == SolveStatus::MULTIPLE_SOLUTIONS);
    assert(res.ranked_guesses.size() >= 2);
    assert(res.ranked_guesses[0].confidence == res.ranked_guesses[1].confidence);
}

void test_pinpoint_weak_evidence_and_out_of_lexicon() {
    std::cout << "Testing Pinpoint weak evidence and out-of-lexicon categories..." << std::endl;

    auto lex = PinpointLexicon::load_from_file("fixtures/pinpoint_lexicon.txt");
    PinpointGuesser guesser(lex, 0.50);

    // Test with completely unknown clues
    auto weak_clues = PinpointGuesser::load_clues_from_file("fixtures/pinpoint_clues_weak.txt");
    auto res_weak = guesser.guess(weak_clues);
    assert(res_weak.is_unknown);
    assert(res_weak.status == SolveStatus::NO_SOLUTION);

    // Out-of-lexicon category: Things associated with Pirates
    std::vector<std::string> pirate_clues = {"PEG LEG", "EYE PATCH", "PARROT", "TREASURE MAP"};
    auto res_pirate = guesser.guess(pirate_clues);
    assert(res_pirate.is_unknown);
    assert(res_pirate.status == SolveStatus::NO_SOLUTION);

    // Out-of-lexicon category: Multi-domain polysemy ("Things that have keys")
    std::vector<std::string> keys_clues = {"LOCK", "MAP", "KEYBOARD", "VAULT"};
    auto res_keys = guesser.guess(keys_clues);
    assert(res_keys.is_unknown);
    assert(res_keys.status == SolveStatus::NO_SOLUTION);

    // Print evaluation of miss modes
    std::cout << "\n=== Pinpoint Held-Out Evaluation Report ===" << std::endl;
    std::cout << "1. In-Lexicon Accuracy: 100% on test benchmarks (Storage Drives, Necklines, Coffee Drinks)." << std::endl;
    std::cout << "2. Clue Attribution: Accurately identifies supported vs unsupported tokens (e.g. CROISSANT)." << std::endl;
    std::cout << "3. Weak Evidence Detection: Successfully abstains (status = UNKNOWN) when confidence < 50%." << std::endl;
    std::cout << "4. Documented Offline Miss Modes:" << std::endl;
    std::cout << "   - Semantic Novelty: Concepts omitted from the editable lexicon (e.g. Pirate gear) cannot be guessed." << std::endl;
    std::cout << "   - Polysemous / Figurative Links: Multi-concept puns ('Things that have keys') lack cross-cutting taxonomy." << std::endl;
    std::cout << "   - Syntactic Wordplay: Anagrams or spelling-based categories ('Words ending in -IGHT') are not captured by semantic terms." << std::endl;
    std::cout << "===========================================\n" << std::endl;
}

void test_pinpoint_progressive_clues() {
    std::cout << "Testing Pinpoint progressive clue resolution (original shipped lexicon)..." << std::endl;

    auto lex = PinpointLexicon::load_from_file("fixtures/pinpoint_lexicon.txt");
    assert(!lex.empty());
    PinpointGuesser guesser(lex, 0.50);

    // "Citrus Fruits": five clues, all present in the shipped lexicon category.
    std::vector<std::string> five = {"LEMON", "LIME", "GRAPEFRUIT", "TANGERINE", "POMELO"};
    auto res5 = guesser.guess(five);
    assert(!res5.is_unknown);
    assert(!res5.ranked_guesses.empty());
    assert(res5.ranked_guesses[0].category_name == "Citrus Fruits");
    assert(res5.ranked_guesses[0].supported_clues.size() == 5);

    // Every prefix of the clue list that stays inside the category keeps Citrus Fruits ranked first.
    for (size_t k = 3; k <= five.size(); ++k) {
        std::vector<std::string> prefix(five.begin(), five.begin() + k);
        auto r = guesser.guess(prefix);
        assert(!r.is_unknown);
        assert(r.ranked_guesses[0].category_name == "Citrus Fruits");
    }

    // A clue set whose category is NOT in the lexicon must abstain, not guess.
    std::vector<std::string> outside = {"SPACESHIP", "QUANTUM", "SUBMARINE", "TELEPORTATION"};
    auto rout = guesser.guess(outside);
    assert(rout.is_unknown);
}

int main() {
    test_pinpoint_lexicon_load();
    test_pinpoint_in_lexicon_ranking();
    test_pinpoint_ambiguity();
    test_pinpoint_weak_evidence_and_out_of_lexicon();
    test_pinpoint_progressive_clues();

    std::cout << "All Pinpoint tests passed successfully!" << std::endl;
    return 0;
}
