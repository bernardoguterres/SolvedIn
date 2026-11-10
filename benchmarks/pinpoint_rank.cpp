// Prints the FULL ranked list (confidence, support count) for each clue file.
// Usage: pinpoint-rank <lexicon.txt> <clues1.txt> [clues2.txt ...]
// Reads only the lexicon and clue files it is given; never any gold labels.
#include "solvedin/pinpoint.hpp"
#include <iostream>
using namespace solvedin;
int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "usage: pinpoint-rank <lexicon> <clues>...\n"; return 2; }
    PinpointLexicon lex = PinpointLexicon::load_from_file(argv[1]);
    PinpointGuesser g(std::move(lex), 0.0);  // threshold 0: expose every ranked candidate
    for (int i = 2; i < argc; ++i) {
        auto res = g.guess(PinpointGuesser::load_clues_from_file(argv[i]));
        std::cout << "FILE\t" << argv[i] << "\n";
        for (const auto& r : res.ranked_guesses)
            std::cout << "R\t" << r.category_name << "\t" << r.confidence << "\t" << r.supported_clues.size() << "\n";
    }
}
