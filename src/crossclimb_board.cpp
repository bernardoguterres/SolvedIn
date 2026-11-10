#include "solvedin/crossclimb_board.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

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

std::string normalize_word(std::string_view word) {
    std::string norm;
    norm.reserve(word.size());
    for (char c : word) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            norm.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    return norm;
}

} // namespace

CrossclimbBoard::CrossclimbBoard() : word_length_(0) {}

CrossclimbBoard::CrossclimbBoard(int word_length,
                                 std::optional<std::string> start_word,
                                 std::vector<std::string> start_candidates,
                                 std::vector<std::string> rung_words,
                                 std::optional<std::string> end_word,
                                 std::vector<std::string> end_candidates)
    : word_length_(word_length),
      start_word_(std::move(start_word)),
      start_candidates_(std::move(start_candidates)),
      rung_words_(std::move(rung_words)),
      end_word_(std::move(end_word)),
      end_candidates_(std::move(end_candidates)) {
    if (word_length_ <= 0) {
        throw std::invalid_argument("Word length must be positive");
    }
    if (rung_words_.empty()) {
        throw std::invalid_argument("Crossclimb puzzle must contain at least one rung word");
    }

    std::unordered_set<std::string> seen_rungs;
    for (const auto& rw : rung_words_) {
        if (static_cast<int>(rw.size()) != word_length_) {
            throw std::invalid_argument("Rung word '" + rw + "' does not match word length " + std::to_string(word_length_));
        }
        if (seen_rungs.contains(rw)) {
            throw std::invalid_argument("Duplicate rung word '" + rw + "'");
        }
        seen_rungs.insert(rw);
    }

    if (start_word_.has_value()) {
        if (static_cast<int>(start_word_->size()) != word_length_) {
            throw std::invalid_argument("Start word '" + *start_word_ + "' does not match word length " + std::to_string(word_length_));
        }
    }
    for (const auto& sc : start_candidates_) {
        if (static_cast<int>(sc.size()) != word_length_) {
            throw std::invalid_argument("Start candidate '" + sc + "' does not match word length " + std::to_string(word_length_));
        }
    }

    if (end_word_.has_value()) {
        if (static_cast<int>(end_word_->size()) != word_length_) {
            throw std::invalid_argument("End word '" + *end_word_ + "' does not match word length " + std::to_string(word_length_));
        }
    }
    for (const auto& ec : end_candidates_) {
        if (static_cast<int>(ec.size()) != word_length_) {
            throw std::invalid_argument("End candidate '" + ec + "' does not match word length " + std::to_string(word_length_));
        }
    }
}

std::string CrossclimbBoard::to_string() const {
    std::ostringstream oss;
    oss << "WORD_LENGTH: " << word_length_ << "\n";
    if (start_word_.has_value()) {
        oss << "START: " << *start_word_ << "\n";
    } else {
        oss << "START: ?\n";
    }
    if (!start_candidates_.empty()) {
        oss << "START_CANDIDATES:";
        for (const auto& sc : start_candidates_) {
            oss << " " << sc;
        }
        oss << "\n";
    }
    oss << "RUNGS:\n";
    for (const auto& rw : rung_words_) {
        oss << rw << "\n";
    }
    if (end_word_.has_value()) {
        oss << "END: " << *end_word_ << "\n";
    } else {
        oss << "END: ?\n";
    }
    if (!end_candidates_.empty()) {
        oss << "END_CANDIDATES:";
        for (const auto& ec : end_candidates_) {
            oss << " " << ec;
        }
        oss << "\n";
    }
    return oss.str();
}

std::string CrossclimbSolution::to_string() const {
    std::ostringstream oss;
    oss << "Crossclimb Ladder (" << ladder.size() << " rungs):\n";
    for (size_t i = 0; i < ladder.size(); ++i) {
        oss << "  [" << i << "] " << ladder[i];
        if (i == 0) {
            oss << " (START)";
        } else if (i + 1 == ladder.size()) {
            oss << " (END)";
        }
        oss << "\n";
    }
    return oss.str();
}

CrossclimbParseResult CrossclimbBoard::load_from_file(const std::string& filepath) {
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        return CrossclimbParseResult{false, std::nullopt, "Could not open file: " + filepath};
    }
    return parse(ifs);
}

CrossclimbParseResult CrossclimbBoard::parse_from_string(const std::string& content) {
    std::istringstream iss(content);
    return parse(iss);
}

CrossclimbParseResult CrossclimbBoard::parse(std::istream& in) {
    std::string line;
    int word_len = 0;
    bool found_len = false;

    std::optional<std::string> start_word;
    std::vector<std::string> start_cands;
    std::vector<std::string> rungs;
    std::optional<std::string> end_word;
    std::vector<std::string> end_cands;

    enum class Section { NONE, RUNGS, START_CANDS, END_CANDS };
    Section sec = Section::NONE;

    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }

        if (t.rfind("WORD_LENGTH:", 0) == 0 || t.rfind("WORD_LENGTH", 0) == 0 ||
            t.rfind("LENGTH:", 0) == 0 || t.rfind("LENGTH", 0) == 0) {
            size_t colon = t.find(':');
            std::string val = (colon != std::string::npos) ? t.substr(colon + 1) : t.substr(6);
            std::istringstream iss(val);
            if (iss >> word_len) {
                found_len = true;
            }
            sec = Section::NONE;
            continue;
        }

        if (!found_len) {
            // Check if line is simply an integer word length
            std::istringstream iss(t);
            int tmp = 0;
            if (iss >> tmp && tmp > 0) {
                word_len = tmp;
                found_len = true;
                sec = Section::NONE;
                continue;
            }
        }

        if (t.rfind("START_CANDIDATES:", 0) == 0 || t.rfind("START_CANDIDATES", 0) == 0) {
            sec = Section::START_CANDS;
            size_t colon = t.find(':');
            std::string rem = (colon != std::string::npos) ? t.substr(colon + 1) : "";
            std::istringstream iss(rem);
            std::string w;
            while (iss >> w) {
                std::string nw = normalize_word(w);
                if (!nw.empty()) start_cands.push_back(nw);
            }
            continue;
        }

        if (t.rfind("START:", 0) == 0 || t.rfind("START", 0) == 0) {
            sec = Section::NONE;
            size_t colon = t.find(':');
            std::string rem = trim((colon != std::string::npos) ? t.substr(colon + 1) : t.substr(5));
            if (rem == "?" || rem.empty()) {
                start_word = std::nullopt;
            } else {
                std::istringstream iss(rem);
                std::string w;
                if (iss >> w) {
                    if (w == "?") {
                        start_word = std::nullopt;
                    } else {
                        start_word = normalize_word(w);
                    }
                }
            }
            continue;
        }

        if (t == "RUNGS:" || t == "RUNGS") {
            sec = Section::RUNGS;
            continue;
        }

        if (t.rfind("END_CANDIDATES:", 0) == 0 || t.rfind("END_CANDIDATES", 0) == 0) {
            sec = Section::END_CANDS;
            size_t colon = t.find(':');
            std::string rem = (colon != std::string::npos) ? t.substr(colon + 1) : "";
            std::istringstream iss(rem);
            std::string w;
            while (iss >> w) {
                std::string nw = normalize_word(w);
                if (!nw.empty()) end_cands.push_back(nw);
            }
            continue;
        }

        if (t.rfind("END:", 0) == 0 || t.rfind("END", 0) == 0) {
            sec = Section::NONE;
            size_t colon = t.find(':');
            std::string rem = trim((colon != std::string::npos) ? t.substr(colon + 1) : t.substr(3));
            if (rem == "?" || rem.empty()) {
                end_word = std::nullopt;
            } else {
                std::istringstream iss(rem);
                std::string w;
                if (iss >> w) {
                    if (w == "?") {
                        end_word = std::nullopt;
                    } else {
                        end_word = normalize_word(w);
                    }
                }
            }
            continue;
        }

        // Processing tokens based on current section
        if (sec == Section::RUNGS) {
            std::istringstream iss(t);
            std::string w;
            while (iss >> w) {
                std::string nw = normalize_word(w);
                if (!nw.empty()) {
                    rungs.push_back(nw);
                }
            }
            continue;
        }

        if (sec == Section::START_CANDS) {
            std::istringstream iss(t);
            std::string w;
            while (iss >> w) {
                std::string nw = normalize_word(w);
                if (!nw.empty()) {
                    start_cands.push_back(nw);
                }
            }
            continue;
        }

        if (sec == Section::END_CANDS) {
            std::istringstream iss(t);
            std::string w;
            while (iss >> w) {
                std::string nw = normalize_word(w);
                if (!nw.empty()) {
                    end_cands.push_back(nw);
                }
            }
            continue;
        }

        // If no explicit section, treat words as rungs
        std::istringstream iss(t);
        std::string w;
        while (iss >> w) {
            std::string nw = normalize_word(w);
            if (!nw.empty()) {
                rungs.push_back(nw);
            }
        }
    }

    if (!found_len || word_len <= 0) {
        if (!rungs.empty()) {
            word_len = static_cast<int>(rungs[0].size());
            found_len = true;
        } else {
            return CrossclimbParseResult{false, std::nullopt, "Missing or invalid word length"};
        }
    }

    if (rungs.empty()) {
        return CrossclimbParseResult{false, std::nullopt, "No rung words found in puzzle"};
    }

    // Validate lengths
    for (const auto& rw : rungs) {
        if (static_cast<int>(rw.size()) != word_len) {
            return CrossclimbParseResult{
                false, std::nullopt,
                "Rung word '" + rw + "' has length " + std::to_string(rw.size()) +
                ", expected " + std::to_string(word_len)
            };
        }
    }
    if (start_word.has_value() && static_cast<int>(start_word->size()) != word_len) {
        return CrossclimbParseResult{
            false, std::nullopt,
            "Start word '" + *start_word + "' has length " + std::to_string(start_word->size()) +
            ", expected " + std::to_string(word_len)
        };
    }
    for (const auto& sc : start_cands) {
        if (static_cast<int>(sc.size()) != word_len) {
            return CrossclimbParseResult{
                false, std::nullopt,
                "Start candidate '" + sc + "' has length " + std::to_string(sc.size()) +
                ", expected " + std::to_string(word_len)
            };
        }
    }
    if (end_word.has_value() && static_cast<int>(end_word->size()) != word_len) {
        return CrossclimbParseResult{
            false, std::nullopt,
            "End word '" + *end_word + "' has length " + std::to_string(end_word->size()) +
            ", expected " + std::to_string(word_len)
        };
    }
    for (const auto& ec : end_cands) {
        if (static_cast<int>(ec.size()) != word_len) {
            return CrossclimbParseResult{
                false, std::nullopt,
                "End candidate '" + ec + "' has length " + std::to_string(ec.size()) +
                ", expected " + std::to_string(word_len)
            };
        }
    }

    try {
        CrossclimbBoard board(word_len, std::move(start_word), std::move(start_cands),
                              std::move(rungs), std::move(end_word), std::move(end_cands));
        return CrossclimbParseResult{true, std::move(board), ""};
    } catch (const std::exception& ex) {
        return CrossclimbParseResult{false, std::nullopt, ex.what()};
    }
}

} // namespace solvedin
