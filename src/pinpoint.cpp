#include "solvedin/pinpoint.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace solvedin {

namespace {

std::string trim(std::string_view s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        start++;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        end--;
    }
    return std::string(s.substr(start, end - start));
}

std::string normalize_text(std::string_view text) {
    std::string norm;
    norm.reserve(text.size());
    for (char c : text) {
        if (std::isalnum(static_cast<unsigned char>(c)) || std::isspace(static_cast<unsigned char>(c))) {
            norm.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    return trim(norm);
}

bool term_matches_clue(std::string_view term, std::string_view clue) {
    std::string n_term = normalize_text(term);
    std::string n_clue = normalize_text(clue);

    if (n_term.empty() || n_clue.empty()) return false;

    // Exact match
    if (n_term == n_clue) return true;

    // Word token match
    std::istringstream term_iss(n_term);
    std::string word;
    while (term_iss >> word) {
        if (word == n_clue) return true;
    }

    std::istringstream clue_iss(n_clue);
    while (clue_iss >> word) {
        if (word == n_term) return true;
    }

    return false;
}

} // namespace

void PinpointLexicon::add_category(PinpointCategory cat) {
    categories_.push_back(std::move(cat));
}

PinpointLexicon PinpointLexicon::load_from_stream(std::istream& in) {
    PinpointLexicon lex;
    std::string line;
    std::optional<PinpointCategory> cur_cat;

    auto finish_current = [&]() {
        if (cur_cat.has_value() && !cur_cat->name.empty() && !cur_cat->terms.empty()) {
            lex.add_category(std::move(*cur_cat));
        }
        cur_cat = std::nullopt;
    };

    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        if (t.rfind("CATEGORY:", 0) == 0 || t.rfind("CATEGORY", 0) == 0) {
            finish_current();
            cur_cat = PinpointCategory{};
            size_t colon = t.find(':');
            cur_cat->name = trim((colon != std::string::npos) ? t.substr(colon + 1) : t.substr(8));
            continue;
        }

        if (t.rfind("TERMS:", 0) == 0 || t.rfind("TERMS", 0) == 0) {
            if (!cur_cat.has_value()) {
                cur_cat = PinpointCategory{};
                cur_cat->name = "Untitled";
            }
            size_t colon = t.find(':');
            std::string rem = (colon != std::string::npos) ? t.substr(colon + 1) : t.substr(5);
            std::istringstream iss(rem);
            std::string item;
            while (std::getline(iss, item, ',')) {
                std::string clean = trim(item);
                if (!clean.empty()) {
                    cur_cat->terms.push_back(clean);
                }
            }
            continue;
        }

        if (t.rfind("DESCRIPTION:", 0) == 0 || t.rfind("DESCRIPTION", 0) == 0) {
            if (cur_cat.has_value()) {
                size_t colon = t.find(':');
                cur_cat->description = trim((colon != std::string::npos) ? t.substr(colon + 1) : t.substr(11));
            }
            continue;
        }
    }

    finish_current();
    return lex;
}

PinpointLexicon PinpointLexicon::load_from_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        return get_default_lexicon();
    }
    return load_from_stream(ifs);
}

PinpointLexicon PinpointLexicon::get_default_lexicon() {
    PinpointLexicon lex;

    lex.add_category(PinpointCategory{
        "Computer Storage Drives",
        "Types of computer storage media and drives",
        {"FLOPPY", "HARD", "COMPACT", "FLASH", "SOLID STATE", "ZIP", "THUMB", "USB", "OPTICAL", "CLOUD"}
    });

    lex.add_category(PinpointCategory{
        "Types of Necklines",
        "Styles and cuts of shirt and sweater necklines",
        {"PULL", "CREW", "TURTLE", "V", "BOAT", "COWL", "SCOOP", "HALTER", "SWEETHEART", "POLO"}
    });

    lex.add_category(PinpointCategory{
        "Citrus Fruits",
        "Edible fruits of the genus Citrus",
        {"LEMON", "LIME", "ORANGE", "GRAPEFRUIT", "TANGERINE", "CLEMENTINE", "CITRON", "POMELO"}
    });

    lex.add_category(PinpointCategory{
        "String Instruments",
        "Musical instruments that produce sound from vibrating strings",
        {"GUITAR", "VIOLIN", "CELLO", "HARP", "BANJO", "UKULELE", "PIANO", "LUTE", "BASS", "MANDOLIN"}
    });

    lex.add_category(PinpointCategory{
        "Card Suits",
        "Suits in a standard 52-card French-suited deck",
        {"HEARTS", "HEART", "DIAMONDS", "DIAMOND", "CLUBS", "CLUB", "SPADES", "SPADE"}
    });

    lex.add_category(PinpointCategory{
        "Precious Gemstones",
        "Highly prized minerals and organic gemstones",
        {"DIAMOND", "RUBY", "SAPPHIRE", "EMERALD", "OPAL", "TOPAZ", "AMETHYST", "PEARL", "JADE"}
    });

    lex.add_category(PinpointCategory{
        "Chess Pieces",
        "Pieces used in the game of chess",
        {"KING", "QUEEN", "ROOK", "BISHOP", "KNIGHT", "PAWN"}
    });

    lex.add_category(PinpointCategory{
        "Planetary Bodies",
        "Major planets and dwarf planets of the Solar System",
        {"MERCURY", "VENUS", "EARTH", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE", "PLUTO"}
    });

    lex.add_category(PinpointCategory{
        "Coffee Drinks",
        "Popular espresso and brewed coffee preparations",
        {"ESPRESSO", "LATTE", "CAPPUCCINO", "AMERICANO", "MOCHA", "MACCHIATO", "FLAT WHITE"}
    });

    lex.add_category(PinpointCategory{
        "Winter Olympic Sports",
        "Sports contested at the Olympic Winter Games",
        {"SKIING", "SNOWBOARDING", "CURLING", "BIATHLON", "BOBSLED", "LUGE", "ICE HOCKEY", "FIGURE SKATING"}
    });

    return lex;
}

PinpointGuesser::PinpointGuesser(PinpointLexicon lexicon, double confidence_threshold)
    : lexicon_(std::move(lexicon)), confidence_threshold_(confidence_threshold) {}

std::vector<std::string> PinpointGuesser::load_clues_from_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return {};

    std::vector<std::string> clues;
    std::string line;
    bool in_clues = false;

    while (std::getline(ifs, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        if (t == "CLUES:" || t == "CLUES") {
            in_clues = true;
            continue;
        }

        if (in_clues || t.find(',') != std::string::npos) {
            std::istringstream iss(t);
            std::string item;
            while (std::getline(iss, item, ',')) {
                std::string clean = trim(item);
                if (!clean.empty() && clean != "CLUES:") {
                    clues.push_back(clean);
                }
            }
        } else {
            clues.push_back(t);
        }
    }
    return clues;
}

PinpointResult PinpointGuesser::guess(const std::vector<std::string>& clues) const {
    PinpointResult result;
    result.input_clues = clues;

    if (clues.empty() || lexicon_.empty()) {
        result.status = SolveStatus::NO_SOLUTION;
        result.is_unknown = true;
        return result;
    }

    std::vector<PinpointGuess> all_guesses;
    all_guesses.reserve(lexicon_.categories().size());

    for (const auto& cat : lexicon_.categories()) {
        PinpointGuess g;
        g.category_name = cat.name;

        for (const auto& clue : clues) {
            bool matched = false;
            for (const auto& term : cat.terms) {
                if (term_matches_clue(term, clue)) {
                    matched = true;
                    break;
                }
            }
            if (matched) {
                g.supported_clues.push_back(clue);
            } else {
                g.unsupported_clues.push_back(clue);
            }
        }

        if (!g.supported_clues.empty()) {
            g.confidence = static_cast<double>(g.supported_clues.size()) / static_cast<double>(clues.size());
            all_guesses.push_back(std::move(g));
        }
    }

    std::sort(all_guesses.begin(), all_guesses.end(), [](const PinpointGuess& a, const PinpointGuess& b) {
        if (std::abs(a.confidence - b.confidence) > 1e-6) {
            return a.confidence > b.confidence;
        }
        if (a.supported_clues.size() != b.supported_clues.size()) {
            return a.supported_clues.size() > b.supported_clues.size();
        }
        return a.category_name < b.category_name;
    });

    result.ranked_guesses = std::move(all_guesses);

    if (result.ranked_guesses.empty() || result.ranked_guesses.front().confidence < confidence_threshold_) {
        result.is_unknown = true;
        result.status = SolveStatus::NO_SOLUTION; // Weak evidence -> unknown
    } else {
        result.is_unknown = false;
        if (result.ranked_guesses.size() >= 2 &&
            std::abs(result.ranked_guesses[0].confidence - result.ranked_guesses[1].confidence) < 1e-6 &&
            result.ranked_guesses[0].supported_clues.size() == result.ranked_guesses[1].supported_clues.size()) {
            result.status = SolveStatus::MULTIPLE_SOLUTIONS; // Ambiguous tie
        } else {
            result.status = SolveStatus::UNIQUE_SOLUTION;
        }
    }

    return result;
}

std::string PinpointResult::to_string() const {
    std::ostringstream oss;
    oss << "Pinpoint Guesser Result (" << input_clues.size() << " clues provided):\n";
    oss << "Clues: [";
    for (size_t i = 0; i < input_clues.size(); ++i) {
        oss << input_clues[i] << (i + 1 < input_clues.size() ? ", " : "");
    }
    oss << "]\n\n";

    if (is_unknown) {
        oss << "STATUS: UNKNOWN (evidence is too weak to make a confident guess)\n";
        if (!ranked_guesses.empty()) {
            oss << "Weak candidates:\n";
            for (size_t i = 0; i < std::min<size_t>(ranked_guesses.size(), 3); ++i) {
                const auto& g = ranked_guesses[i];
                oss << "  #" << (i + 1) << " " << g.category_name
                    << " (confidence: " << std::fixed << std::setprecision(1) << (g.confidence * 100.0) << "%)\n"
                    << "     Supported: ";
                for (const auto& sc : g.supported_clues) oss << sc << " ";
                oss << "\n";
            }
        }
        return oss.str();
    }

    oss << "STATUS: " << (status == SolveStatus::UNIQUE_SOLUTION ? "CONFIDENT GUESS" : "AMBIGUOUS / TIED GUESS") << "\n";
    for (size_t i = 0; i < std::min<size_t>(ranked_guesses.size(), 5); ++i) {
        const auto& g = ranked_guesses[i];
        oss << "  #" << (i + 1) << " " << g.category_name
            << " (confidence: " << std::fixed << std::setprecision(1) << (g.confidence * 100.0) << "%)\n"
            << "     Supported (" << g.supported_clues.size() << "): ";
        for (const auto& sc : g.supported_clues) oss << sc << " ";
        oss << "\n";
        if (!g.unsupported_clues.empty()) {
            oss << "     Unsupported (" << g.unsupported_clues.size() << "): ";
            for (const auto& uc : g.unsupported_clues) oss << uc << " ";
            oss << "\n";
        }
    }

    return oss.str();
}

} // namespace solvedin
