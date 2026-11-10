#include "solvedin/board.hpp"
#include "solvedin/crossclimb_board.hpp"
#include "solvedin/crossclimb_logic.hpp"
#include "solvedin/crossclimb_solver.hpp"
#include "solvedin/crossclimb_validator.hpp"
#include "solvedin/patches_board.hpp"
#include "solvedin/patches_logic.hpp"
#include "solvedin/patches_solver.hpp"
#include "solvedin/patches_validator.hpp"
#include "solvedin/pinpoint.hpp"
#include "solvedin/pinpoint_logic.hpp"
#include "solvedin/reasoning.hpp"
#include "solvedin/queens_generator.hpp"
#include "solvedin/queens_logic.hpp"
#include "solvedin/queens_solver.hpp"
#include "solvedin/queens_validator.hpp"
#include "solvedin/sudoku_board.hpp"
#include "solvedin/sudoku_generator.hpp"
#include "solvedin/sudoku_logic.hpp"
#include "solvedin/sudoku_solver.hpp"
#include "solvedin/sudoku_validator.hpp"
#include "solvedin/svg_exporter.hpp"
#include "solvedin/tango_board.hpp"
#include "solvedin/tango_generator.hpp"
#include "solvedin/tango_logic.hpp"
#include "solvedin/tango_solver.hpp"
#include "solvedin/tango_validator.hpp"
#include "solvedin/trie.hpp"
#include "solvedin/wend_board.hpp"
#include "solvedin/wend_logic.hpp"
#include "solvedin/wend_solver.hpp"
#include "solvedin/wend_validator.hpp"
#include "solvedin/zip_board.hpp"
#include "solvedin/zip_generator.hpp"
#include "solvedin/zip_logic.hpp"
#include "solvedin/zip_solver.hpp"
#include "solvedin/zip_validator.hpp"

#include <fstream>
#include <functional>
#include <optional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <cctype>

static const char* reasoning_help_text() {
    return "Reasoning Commands (explainable solver):\n"
           "  hint <board-file>     One useful move or elimination from the board as it stands\n"
           "  deduce <board-file>   Sound deductions only; stops with a partial board when stuck\n"
           "  explain <board-file>  Deductions, then bounded guessing with deductions inside each\n"
           "                        branch, then a separate uniqueness check; prints every step\n\n"
           "Reasoning Options:\n"
           "  --max-steps <N>       Print at most N steps (default 200; 0 = all)\n"
           "  --no-unique           explain: stop at the first solution (skip the uniqueness check)\n"
           "  --basic-rules         Use only the simplest rules (Sudoku: singles), for comparison\n"
           "  --max-nodes <num>     Search node budget (explain)\n"
           "  --max-time-ms <num>   Search time budget in ms (explain)\n\n";
}

namespace {

bool write_svg_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << content;
    return true;
}

void print_general_help(std::ostream& os) {
    os << "SolvedIn\n\n"
       << "Usage:\n"
       << "  solvedin <game> <command> [options] [arguments]\n\n"
       << "Games:\n"
       << "  queens     LinkedIn Queens puzzle solver and generator\n"
       << "  sudoku     LinkedIn Mini Sudoku (6x6 with 2x3 boxes) solver and generator\n"
       << "  tango      LinkedIn Tango (Sun & Moon with edge clues) solver and generator\n"
       << "  zip        LinkedIn Zip (Hamiltonian path with checkpoints & walls) solver and generator\n"
       << "  patches    LinkedIn Patches (exact board tiling with clue shapes) solver\n"
       << "  wend       LinkedIn Wend (orthogonal trie-based word cover) solver\n"
       << "  crossclimb LinkedIn Crossclimb (word ladder graph with missing endpoints) solver\n"
       << "  pinpoint   LinkedIn Pinpoint (offline ranked category guesser)\n\n"
       << "Options:\n"
       << "  -h, --help    Show this help message\n";
}

void print_queens_help(std::ostream& os) {
    os << "LinkedIn Queens Puzzle Solver & Generator\n\n"
       << "Usage:\n"
       << "  solvedin queens <command> [options]\n\n"
       << "Commands:\n"
       << "  solve <board-file>    Solve a board from file\n"
       << "  generate              Generate a valid seeded board\n\n"
       << reasoning_help_text()
       << "Solve Options:\n"
       << "  --check-unique        Check whether the puzzle has a unique solution\n"
       << "  --solver <variant>    Select solver variant: baseline (default), mrv, bitset\n"
       << "  --max-nodes <num>     Set search node budget (inconclusive on exhaustion)\n"
       << "  --max-time-ms <num>   Set search time budget in ms (inconclusive on exhaustion)\n"
       << "  --export-svg <file>   Export solved board visualization as SVG\n"
       << "  --stats               Print search metrics (nodes visited, elapsed time)\n\n"
       << "Generate Options:\n"
       << "  --size <N>            Board dimension N (e.g. 8, 32, 64, 128)\n"
       << "  --seed <S>            64-bit random seed for deterministic generation\n"
       << "  --output <file>       Write generated board to file instead of stdout\n\n"
       << "General Options:\n"
       << "  -h, --help            Show this help message\n";
}

void print_sudoku_help(std::ostream& os) {
    os << "LinkedIn Mini Sudoku (6x6) Solver & Generator\n\n"
       << "Usage:\n"
       << "  solvedin sudoku <command> [options]\n\n"
       << "Commands:\n"
       << "  solve <board-file>    Solve a 6x6 mini sudoku puzzle from file\n"
       << "  generate              Generate a valid seeded mini sudoku puzzle\n\n"
       << reasoning_help_text()
       << "Solve Options:\n"
       << "  --check-unique        Check whether the puzzle has a unique solution\n"
       << "  --max-nodes <num>     Set search node budget\n"
       << "  --max-time-ms <num>   Set search time budget in ms\n"
       << "  --export-svg <file>   Export solved puzzle visualization as SVG\n"
       << "  --stats               Print search metrics (nodes visited, elapsed time)\n\n"
       << "Generate Options:\n"
       << "  --seed <S>            64-bit random seed for deterministic generation\n"
       << "  --remove <N>          Number of cells to attempt removing (default: 20)\n"
       << "  --output <file>       Write generated puzzle to file instead of stdout\n\n"
       << "General Options:\n"
       << "  -h, --help            Show this help message\n";
}

void print_tango_help(std::ostream& os) {
    os << "LinkedIn Tango Solver & Generator\n\n"
       << "Usage:\n"
       << "  solvedin tango <command> [options]\n\n"
       << "Commands:\n"
       << "  solve <board-file>    Solve a tango puzzle from file\n"
       << "  generate              Generate a valid seeded tango puzzle\n\n"
       << reasoning_help_text()
       << "Solve Options:\n"
       << "  --check-unique        Check whether the puzzle has a unique solution\n"
       << "  --max-nodes <num>     Set search node budget\n"
       << "  --max-time-ms <num>   Set search time budget in ms\n"
       << "  --export-svg <file>   Export solved puzzle visualization as SVG\n"
       << "  --stats               Print search metrics (nodes visited, elapsed time)\n\n"
       << "Generate Options:\n"
       << "  --seed <S>            64-bit random seed for deterministic generation\n"
       << "  --givens <G>          Target number of given symbols (default: 8)\n"
       << "  --clues <C>           Target number of edge clues (default: 6)\n"
       << "  --output <file>       Write generated puzzle to file instead of stdout\n\n"
       << "General Options:\n"
       << "  -h, --help            Show this help message\n";
}

void print_zip_help(std::ostream& os) {
    os << "LinkedIn Zip Solver & Generator\n\n"
       << "Usage:\n"
       << "  solvedin zip <command> [options]\n\n"
       << "Commands:\n"
       << "  solve <board-file>    Solve a zip path puzzle from file\n"
       << "  generate              Generate a valid seeded zip puzzle\n\n"
       << reasoning_help_text()
       << "Solve Options:\n"
       << "  --check-unique        Check whether the puzzle has a unique solution\n"
       << "  --max-nodes <num>     Set search node budget\n"
       << "  --max-time-ms <num>   Set search time budget in ms\n"
       << "  --export-svg <file>   Export solved puzzle visualization as SVG\n"
       << "  --stats               Print search metrics (nodes visited, elapsed time)\n\n"
       << "Generate Options:\n"
       << "  --rows <R>            Number of board rows\n"
       << "  --cols <C>            Number of board columns\n"
       << "  --checkpoints <K>     Number of checkpoints to place\n"
       << "  --seed <S>            64-bit random seed for deterministic generation\n"
       << "  --output <file>       Write generated puzzle to file instead of stdout\n\n"
       << "General Options:\n"
       << "  -h, --help            Show this help message\n";
}

void print_patches_help(std::ostream& os) {
    os << "LinkedIn Patches Solver\n\n"
       << "Usage:\n"
       << "  solvedin patches <command> [options]\n\n"
       << "Commands:\n"
       << "  solve <board-file>    Solve a patches puzzle from file\n\n"
       << reasoning_help_text()
       << "Solve Options:\n"
       << "  --check-unique        Check whether the puzzle has a unique solution\n"
       << "  --max-nodes <num>     Set search node budget\n"
       << "  --max-time-ms <num>   Set search time budget in ms\n"
       << "  --export-svg <file>   Export solved patchwork visualization as SVG\n"
       << "  --stats               Print search metrics (nodes visited, elapsed time)\n\n"
       << "General Options:\n"
       << "  -h, --help            Show this help message\n";
}

void print_wend_help(std::ostream& os) {
    os << "LinkedIn Wend Solver\n\n"
       << "Usage:\n"
       << "  solvedin wend <command> [options]\n\n"
       << "Commands:\n"
       << "  solve <board-file>    Solve a wend word cover puzzle from file\n\n"
       << reasoning_help_text()
       << "Solve Options:\n"
       << "  --dict <file>         User-supplied dictionary file (default: embedded test lexicon)\n"
       << "  --check-unique        Check whether the puzzle has a unique solution\n"
       << "  --max-nodes <num>     Set search node budget\n"
       << "  --max-time-ms <num>   Set search time budget in ms\n"
       << "  --export-svg <file>   Export solved word cover visualization as SVG\n"
       << "  --stats               Print search metrics (nodes visited, elapsed time)\n\n"
       << "General Options:\n"
       << "  -h, --help            Show this help message\n";
}

void print_crossclimb_help(std::ostream& os) {
    os << "LinkedIn Crossclimb Solver\n\n"
       << "Usage:\n"
       << "  solvedin crossclimb <command> [options]\n\n"
       << "Commands:\n"
       << "  solve <board-file>    Solve a crossclimb word ladder puzzle from file\n\n"
       << reasoning_help_text()
       << "Solve Options:\n"
       << "  --dict <file>         User-supplied dictionary file (default: embedded test lexicon)\n"
       << "  --check-unique        Check whether the puzzle has a unique solution (default: true)\n"
       << "  --max-nodes <num>     Set search node budget\n"
       << "  --max-time-ms <num>   Set search time budget in ms\n"
       << "  --export-svg <file>   Export solved ladder visualization as SVG\n"
       << "  --stats               Print search metrics (nodes visited, elapsed time)\n\n"
       << "General Options:\n"
       << "  -h, --help            Show this help message\n";
}

void print_pinpoint_help(std::ostream& os) {
    os << "LinkedIn Pinpoint Offline Ranked Guesser\n\n"
       << "Usage:\n"
       << "  solvedin pinpoint <command> [options]\n\n"
       << "Commands:\n"
       << "  guess [clues-file]    Rank category guesses for supplied clues\n"
       << "  explain <clues-file>  After each clue: leading hypotheses (lexicon, and words before/after every\n"
       << "                        clue with --dict <word list>), their connection to each clue, and a decision\n"
       << "                        to answer, ask for the next clue, or abstain\n\n"
       << "Guess Options:\n"
       << "  --clues <c1,c2,...>   Supply comma-separated clues directly on CLI\n"
       << "  --lexicon <file>      User-supplied category lexicon file\n"
       << "  --threshold <float>   Confidence threshold for non-unknown guess (default: 0.50)\n\n"
       << "General Options:\n"
       << "  -h, --help            Show this help message\n";
}

void print_solution_board(std::ostream& os, const solvedin::Board& board,
                          const solvedin::QueensSolution& solution) {
    const int n = board.size();
    std::vector<std::string> grid(n, std::string(n, '.'));
    for (const auto& q : solution.queens) {
        if (q.row >= 0 && q.row < n && q.col >= 0 && q.col < n) {
            grid[q.row][q.col] = 'Q';
        }
    }

    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            os << grid[r][c] << (c + 1 < n ? " " : "");
        }
        os << "\n";
    }

    os << "Queen positions (row, col):\n";
    for (const auto& q : solution.queens) {
        os << "  (" << q.row << ", " << q.col << ")\n";
    }
}

void print_stats(std::ostream& os, const solvedin::SolveResult& result) {
    os << "\n--- Search Metrics ---\n"
       << "Solver variant: " << solvedin::to_string(result.variant) << "\n"
       << "Nodes visited : " << result.stats.nodes_visited << "\n"
       << std::fixed << std::setprecision(3)
       << "Elapsed time  : " << result.stats.elapsed_ms << " ms\n"
       << "Budget status : " << (result.stats.budget_exhausted ? "EXHAUSTED" : "WITHIN_BUDGET") << "\n";
}

void print_generic_stats(std::ostream& os, const solvedin::SolveStats& stats) {
    os << "\n--- Search Metrics ---\n"
       << "Nodes visited : " << stats.nodes_visited << "\n"
       << std::fixed << std::setprecision(3)
       << "Elapsed time  : " << stats.elapsed_ms << " ms\n"
       << "Budget status : " << (stats.budget_exhausted ? "EXHAUSTED" : "WITHIN_BUDGET") << "\n";
}

// ============================================================================
// Explainable runs shared by every game: hint / deduce / explain
// ============================================================================


using ReasoningRunner = std::function<std::optional<solvedin::ReasoningReport>(
    const std::string& file, const solvedin::ReasoningOptions& opt,
    const std::vector<std::pair<std::string, std::string>>& extra, std::string& error)>;

// Options with a value that a game may accept on top of the shared ones (e.g. --dict for Wend).
int handle_reasoning(int argc, char* argv[], solvedin::ReasoningMode mode, const ReasoningRunner& run,
                     void (*help)(std::ostream&), const std::vector<std::string>& extra_value_options = {}) {
    solvedin::ReasoningOptions opt;
    opt.mode = mode;
    size_t max_steps = 200;
    std::string file;
    std::vector<std::pair<std::string, std::string>> extra;
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        auto value = [&](const std::string& name) -> std::optional<std::string> {
            if (arg == name && i + 1 < argc) return std::string(argv[++i]);
            if (arg.rfind(name + "=", 0) == 0) return arg.substr(name.size() + 1);
            return std::nullopt;
        };
        if (arg == "-h" || arg == "--help") { help(std::cout); return 0; }
        if (arg == "--no-unique") { opt.check_unique = false; continue; }
        if (arg == "--basic-rules") { opt.basic_rules = true; continue; }
        if (auto v = value("--max-steps")) { max_steps = std::stoull(*v); continue; }
        if (auto v = value("--max-nodes")) { opt.max_nodes = std::stoull(*v); continue; }
        if (auto v = value("--max-time-ms")) { opt.max_time_ms = std::stod(*v); continue; }
        bool matched = false;
        for (const auto& name : extra_value_options) {
            if (auto v = value(name)) { extra.push_back({name, *v}); matched = true; break; }
        }
        if (matched) continue;
        if (arg.rfind("-", 0) == 0) { std::cerr << "Error: Unrecognized option '" << arg << "'\n\n"; help(std::cerr); return 1; }
        if (!file.empty()) { std::cerr << "Error: Unexpected argument '" << arg << "'\n\n"; help(std::cerr); return 1; }
        file = arg;
    }
    if (file.empty()) { std::cerr << "Error: Missing board file argument\n\n"; help(std::cerr); return 1; }
    std::string error;
    auto report = run(file, opt, extra, error);
    if (!report) { std::cerr << "Error: " << error << "\n"; return 2; }
    solvedin::print_reasoning_report(std::cout, *report, mode, max_steps);
    return 0;
}

bool reasoning_command(std::string_view cmd, solvedin::ReasoningMode& mode) {
    if (cmd == "hint") { mode = solvedin::ReasoningMode::HINT; return true; }
    if (cmd == "deduce") { mode = solvedin::ReasoningMode::DEDUCE; return true; }
    if (cmd == "explain") { mode = solvedin::ReasoningMode::EXPLAIN; return true; }
    return false;
}

// ============================================================================
// Queens Handlers
// ============================================================================

int handle_queens_generate(int argc, char* argv[]) {
    int size = 0;
    uint64_t seed = 0;
    bool size_set = false;
    bool seed_set = false;
    std::string output_path;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_queens_help(std::cout);
            return 0;
        } else if (arg == "--size" && i + 1 < argc) {
            size = std::stoi(argv[++i]);
            size_set = true;
        } else if (arg.starts_with("--size=")) {
            size = std::stoi(std::string(arg.substr(7)));
            size_set = true;
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
            seed_set = true;
        } else if (arg.starts_with("--seed=")) {
            seed = std::stoull(std::string(arg.substr(7)));
            seed_set = true;
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg.starts_with("--output=")) {
            output_path = arg.substr(9);
        } else {
            std::cerr << "Error: Unrecognized option '" << arg << "' for generate command\n\n";
            print_queens_help(std::cerr);
            return 1;
        }
    }

    if (!size_set) {
        std::cerr << "Error: Missing required option --size <N>\n";
        return 1;
    }
    if (!seed_set) {
        std::cerr << "Error: Missing required option --seed <S>\n";
        return 1;
    }

    try {
        auto gen = solvedin::QueensGenerator::generate(size, seed);
        const auto& board = gen.board;

        std::ostringstream out;
        out << board.size() << "\n";
        for (int r = 0; r < board.size(); ++r) {
            for (int c = 0; c < board.size(); ++c) {
                out << board.get_region(r, c) << (c + 1 < board.size() ? " " : "");
            }
            out << "\n";
        }

        if (!output_path.empty()) {
            std::ofstream f(output_path);
            if (!f.is_open()) {
                std::cerr << "Error: Failed to write to output file '" << output_path << "'\n";
                return 1;
            }
            f << out.str();
        } else {
            std::cout << out.str();
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error generating board: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

int handle_queens_solve(int argc, char* argv[]) {
    std::string board_filepath;
    bool check_unique = false;
    bool show_stats = false;
    std::string export_svg_path;
    solvedin::SolverVariant solver_variant = solvedin::SolverVariant::BASELINE;
    solvedin::SolveOptions options;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_queens_help(std::cout);
            return 0;
        } else if (arg == "--check-unique") {
            check_unique = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else if (arg == "--export-svg" && i + 1 < argc) {
            export_svg_path = argv[++i];
        } else if (arg.starts_with("--export-svg=")) {
            export_svg_path = arg.substr(13);
        } else if (arg == "--max-nodes" && i + 1 < argc) {
            options.max_nodes = std::stoull(argv[++i]);
        } else if (arg.starts_with("--max-nodes=")) {
            options.max_nodes = std::stoull(std::string(arg.substr(12)));
        } else if (arg == "--max-time-ms" && i + 1 < argc) {
            options.max_time_ms = std::stod(argv[++i]);
        } else if (arg.starts_with("--max-time-ms=")) {
            options.max_time_ms = std::stod(std::string(arg.substr(14)));
        } else if (arg == "--solver" && i + 1 < argc) {
            auto parsed = solvedin::parse_solver_variant(argv[++i]);
            if (!parsed) {
                std::cerr << "Error: Invalid solver variant '" << argv[i]
                          << "'. Options: baseline, mrv, bitset\n\n";
                print_queens_help(std::cerr);
                return 1;
            }
            solver_variant = *parsed;
        } else if (arg.starts_with("--solver=")) {
            auto val = arg.substr(9);
            auto parsed = solvedin::parse_solver_variant(val);
            if (!parsed) {
                std::cerr << "Error: Invalid solver variant '" << val
                          << "'. Options: baseline, mrv, bitset\n\n";
                print_queens_help(std::cerr);
                return 1;
            }
            solver_variant = *parsed;
        } else if (arg.starts_with("-")) {
            std::cerr << "Error: Unrecognized option '" << arg << "'\n\n";
            print_queens_help(std::cerr);
            return 1;
        } else {
            if (board_filepath.empty()) {
                board_filepath = arg;
            } else {
                std::cerr << "Error: Unexpected argument '" << arg << "'\n\n";
                print_queens_help(std::cerr);
                return 1;
            }
        }
    }

    if (board_filepath.empty()) {
        std::cerr << "Error: Missing board file argument\n\n";
        print_queens_help(std::cerr);
        return 1;
    }

    auto [success, maybe_board, error_message] = solvedin::Board::load_from_file(board_filepath);
    if (!success || !maybe_board) {
        std::cerr << "Error: Malformed board file '" << board_filepath << "': " << error_message << "\n";
        return 2;
    }

    const auto& board = *maybe_board;
    auto result = solvedin::solve_queens(board, check_unique, solver_variant, options);

    if (!export_svg_path.empty() && !result.solutions.empty()) {
        if (!solvedin::SvgExporter::export_to_file(board, result.solutions[0].queens, export_svg_path)) {
            std::cerr << "Warning: Failed to export SVG to '" << export_svg_path << "'\n";
        } else {
            std::cout << "Exported solution visualization to SVG: " << export_svg_path << "\n";
        }
    }

    if (!check_unique) {
        if (result.status == solvedin::SolveStatus::INCONCLUSIVE) {
            std::cout << "Search inconclusive: budget exhausted (limit reached).\n";
            if (show_stats) print_stats(std::cout, result);
            return 0;
        }
        if (result.status == solvedin::SolveStatus::NO_SOLUTION || result.solutions.empty()) {
            std::cout << "No solution exists.\n";
            if (show_stats) print_stats(std::cout, result);
            return 0;
        }

        std::cout << "Solution found:\n";
        print_solution_board(std::cout, board, result.solutions[0]);
        if (show_stats) print_stats(std::cout, result);
        return 0;
    } else {
        switch (result.status) {
        case solvedin::SolveStatus::INCONCLUSIVE:
            if (!result.solutions.empty()) {
                std::cout << "First solution found, but uniqueness proof is inconclusive (budget exhausted):\n";
                print_solution_board(std::cout, board, result.solutions[0]);
            } else {
                std::cout << "Search inconclusive: budget exhausted before solution found.\n";
            }
            if (show_stats) print_stats(std::cout, result);
            return 0;
        case solvedin::SolveStatus::NO_SOLUTION:
            std::cout << "No solution exists.\n";
            if (show_stats) print_stats(std::cout, result);
            return 0;
        case solvedin::SolveStatus::UNIQUE_SOLUTION:
            std::cout << "Unique solution found:\n";
            print_solution_board(std::cout, board, result.solutions[0]);
            if (show_stats) print_stats(std::cout, result);
            return 0;
        case solvedin::SolveStatus::MULTIPLE_SOLUTIONS:
            std::cout << "Multiple solutions found (at least 2):\n\n";
            for (size_t i = 0; i < result.solutions.size(); ++i) {
                std::cout << "--- Solution " << (i + 1) << " ---\n";
                print_solution_board(std::cout, board, result.solutions[i]);
                if (i + 1 < result.solutions.size()) {
                    std::cout << "\n";
                }
            }
            if (show_stats) print_stats(std::cout, result);
            return 0;
        }
    }

    return 0;
}

// ============================================================================
// Sudoku Handlers
// ============================================================================

void print_sudoku_grid(std::ostream& os, const std::array<std::array<int, 6>, 6>& grid) {
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            os << grid[r][c];
            if (c + 1 == 3) {
                os << " | ";
            } else if (c + 1 < 6) {
                os << " ";
            }
        }
        os << "\n";
        if (r + 1 < 6 && (r + 1) % 2 == 0) {
            os << "------+-------\n";
        }
    }
}

int handle_sudoku_generate(int argc, char* argv[]) {
    uint64_t seed = 0;
    bool seed_set = false;
    int remove_count = 20;
    std::string output_path;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_sudoku_help(std::cout);
            return 0;
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
            seed_set = true;
        } else if (arg.starts_with("--seed=")) {
            seed = std::stoull(std::string(arg.substr(7)));
            seed_set = true;
        } else if (arg == "--remove" && i + 1 < argc) {
            remove_count = std::stoi(argv[++i]);
        } else if (arg.starts_with("--remove=")) {
            remove_count = std::stoi(std::string(arg.substr(9)));
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg.starts_with("--output=")) {
            output_path = arg.substr(9);
        } else {
            std::cerr << "Error: Unrecognized option '" << arg << "' for sudoku generate\n\n";
            print_sudoku_help(std::cerr);
            return 1;
        }
    }

    if (!seed_set) {
        std::cerr << "Error: Missing required option --seed <S>\n";
        return 1;
    }

    try {
        auto board = solvedin::SudokuGenerator::generate(seed, remove_count);
        std::string formatted = solvedin::SudokuGenerator::format_puzzle(board);

        if (!output_path.empty()) {
            std::ofstream f(output_path);
            if (!f.is_open()) {
                std::cerr << "Error: Failed to write to output file '" << output_path << "'\n";
                return 1;
            }
            f << formatted;
        } else {
            std::cout << formatted;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error generating sudoku puzzle: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

int handle_sudoku_solve(int argc, char* argv[]) {
    std::string board_filepath;
    bool check_unique = false;
    bool show_stats = false;
    std::string export_svg_path;
    solvedin::SolveOptions options;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_sudoku_help(std::cout);
            return 0;
        } else if (arg == "--check-unique") {
            check_unique = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else if (arg == "--export-svg" && i + 1 < argc) {
            export_svg_path = argv[++i];
        } else if (arg.starts_with("--export-svg=")) {
            export_svg_path = arg.substr(13);
        } else if (arg == "--max-nodes" && i + 1 < argc) {
            options.max_nodes = std::stoull(argv[++i]);
        } else if (arg.starts_with("--max-nodes=")) {
            options.max_nodes = std::stoull(std::string(arg.substr(12)));
        } else if (arg == "--max-time-ms" && i + 1 < argc) {
            options.max_time_ms = std::stod(argv[++i]);
        } else if (arg.starts_with("--max-time-ms=")) {
            options.max_time_ms = std::stod(std::string(arg.substr(14)));
        } else if (arg.starts_with("-")) {
            std::cerr << "Error: Unrecognized option '" << arg << "'\n\n";
            print_sudoku_help(std::cerr);
            return 1;
        } else {
            if (board_filepath.empty()) {
                board_filepath = arg;
            } else {
                std::cerr << "Error: Unexpected argument '" << arg << "'\n\n";
                print_sudoku_help(std::cerr);
                return 1;
            }
        }
    }

    if (board_filepath.empty()) {
        std::cerr << "Error: Missing board file argument\n\n";
        print_sudoku_help(std::cerr);
        return 1;
    }

    auto [success, maybe_board, error_message] = solvedin::SudokuBoard::load_from_file(board_filepath);
    if (!success || !maybe_board) {
        std::cerr << "Error: Malformed sudoku board file '" << board_filepath << "': " << error_message << "\n";
        return 2;
    }

    const auto& board = *maybe_board;
    auto result = solvedin::solve_sudoku(board, check_unique, options);

    if (!export_svg_path.empty() && !result.solutions.empty()) {
        if (!write_svg_file(export_svg_path, solvedin::export_sudoku_svg(board, result.solutions[0]))) {
            std::cerr << "Warning: Failed to export SVG to '" << export_svg_path << "'\n";
        } else {
            std::cout << "Exported solution visualization to SVG: " << export_svg_path << "\n";
        }
    }

    if (!check_unique) {
        if (result.status == solvedin::SolveStatus::INCONCLUSIVE) {
            std::cout << "Search inconclusive: budget exhausted.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }
        if (result.status == solvedin::SolveStatus::NO_SOLUTION || result.solutions.empty()) {
            std::cout << "No solution exists.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }

        std::cout << "Solution found:\n";
        print_sudoku_grid(std::cout, result.solutions[0].grid);
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    } else {
        switch (result.status) {
        case solvedin::SolveStatus::INCONCLUSIVE:
            std::cout << "Search inconclusive: budget exhausted.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::NO_SOLUTION:
            std::cout << "No solution exists.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::UNIQUE_SOLUTION:
            std::cout << "Unique solution found:\n";
            print_sudoku_grid(std::cout, result.solutions[0].grid);
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::MULTIPLE_SOLUTIONS:
            std::cout << "Multiple solutions found (at least 2):\n\n";
            for (size_t i = 0; i < result.solutions.size(); ++i) {
                std::cout << "--- Solution " << (i + 1) << " ---\n";
                print_sudoku_grid(std::cout, result.solutions[i].grid);
                if (i + 1 < result.solutions.size()) std::cout << "\n";
            }
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }
    }
    return 0;
}

// ============================================================================
// Tango Handlers
// ============================================================================

void print_tango_grid(std::ostream& os, const std::vector<std::string>& grid) {
    for (const auto& row : grid) {
        for (size_t c = 0; c < row.size(); ++c) {
            os << row[c] << (c + 1 < row.size() ? " " : "");
        }
        os << "\n";
    }
}

int handle_tango_generate(int argc, char* argv[]) {
    uint64_t seed = 0;
    bool seed_set = false;
    int given_count = 8;
    int edge_clue_count = 6;
    std::string output_path;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_tango_help(std::cout);
            return 0;
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
            seed_set = true;
        } else if (arg.starts_with("--seed=")) {
            seed = std::stoull(std::string(arg.substr(7)));
            seed_set = true;
        } else if (arg == "--givens" && i + 1 < argc) {
            given_count = std::stoi(argv[++i]);
        } else if (arg.starts_with("--givens=")) {
            given_count = std::stoi(std::string(arg.substr(9)));
        } else if (arg == "--clues" && i + 1 < argc) {
            edge_clue_count = std::stoi(argv[++i]);
        } else if (arg.starts_with("--clues=")) {
            edge_clue_count = std::stoi(std::string(arg.substr(8)));
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg.starts_with("--output=")) {
            output_path = arg.substr(9);
        } else {
            std::cerr << "Error: Unrecognized option '" << arg << "' for tango generate\n\n";
            print_tango_help(std::cerr);
            return 1;
        }
    }

    if (!seed_set) {
        std::cerr << "Error: Missing required option --seed <S>\n";
        return 1;
    }

    try {
        auto board = solvedin::TangoGenerator::generate(seed, given_count, edge_clue_count);
        std::string formatted = board.to_string();

        if (!output_path.empty()) {
            std::ofstream f(output_path);
            if (!f.is_open()) {
                std::cerr << "Error: Failed to write to output file '" << output_path << "'\n";
                return 1;
            }
            f << formatted;
        } else {
            std::cout << formatted;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error generating tango puzzle: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

int handle_tango_solve(int argc, char* argv[]) {
    std::string board_filepath;
    bool check_unique = false;
    bool show_stats = false;
    std::string export_svg_path;
    solvedin::SolveOptions options;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_tango_help(std::cout);
            return 0;
        } else if (arg == "--check-unique") {
            check_unique = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else if (arg == "--export-svg" && i + 1 < argc) {
            export_svg_path = argv[++i];
        } else if (arg.starts_with("--export-svg=")) {
            export_svg_path = arg.substr(13);
        } else if (arg == "--max-nodes" && i + 1 < argc) {
            options.max_nodes = std::stoull(argv[++i]);
        } else if (arg.starts_with("--max-nodes=")) {
            options.max_nodes = std::stoull(std::string(arg.substr(12)));
        } else if (arg == "--max-time-ms" && i + 1 < argc) {
            options.max_time_ms = std::stod(argv[++i]);
        } else if (arg.starts_with("--max-time-ms=")) {
            options.max_time_ms = std::stod(std::string(arg.substr(14)));
        } else if (arg.starts_with("-")) {
            std::cerr << "Error: Unrecognized option '" << arg << "'\n\n";
            print_tango_help(std::cerr);
            return 1;
        } else {
            if (board_filepath.empty()) {
                board_filepath = arg;
            } else {
                std::cerr << "Error: Unexpected argument '" << arg << "'\n\n";
                print_tango_help(std::cerr);
                return 1;
            }
        }
    }

    if (board_filepath.empty()) {
        std::cerr << "Error: Missing board file argument\n\n";
        print_tango_help(std::cerr);
        return 1;
    }

    auto [success, maybe_board, error_message] = solvedin::TangoBoard::load_from_file(board_filepath);
    if (!success || !maybe_board) {
        std::cerr << "Error: Malformed tango board file '" << board_filepath << "': " << error_message << "\n";
        return 2;
    }

    const auto& board = *maybe_board;
    auto result = solvedin::solve_tango(board, check_unique, options);

    if (!export_svg_path.empty() && !result.solutions.empty()) {
        if (!write_svg_file(export_svg_path, solvedin::export_tango_svg(board, result.solutions[0]))) {
            std::cerr << "Warning: Failed to export SVG to '" << export_svg_path << "'\n";
        } else {
            std::cout << "Exported solution visualization to SVG: " << export_svg_path << "\n";
        }
    }

    if (!check_unique) {
        if (result.status == solvedin::SolveStatus::INCONCLUSIVE) {
            std::cout << "Search inconclusive: budget exhausted.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }
        if (result.status == solvedin::SolveStatus::NO_SOLUTION || result.solutions.empty()) {
            std::cout << "No solution exists.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }

        std::cout << "Solution found:\n";
        print_tango_grid(std::cout, result.solutions[0].grid);
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    } else {
        switch (result.status) {
        case solvedin::SolveStatus::INCONCLUSIVE:
            std::cout << "Search inconclusive: budget exhausted.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::NO_SOLUTION:
            std::cout << "No solution exists.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::UNIQUE_SOLUTION:
            std::cout << "Unique solution found:\n";
            print_tango_grid(std::cout, result.solutions[0].grid);
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::MULTIPLE_SOLUTIONS:
            std::cout << "Multiple solutions found (at least 2):\n\n";
            for (size_t i = 0; i < result.solutions.size(); ++i) {
                std::cout << "--- Solution " << (i + 1) << " ---\n";
                print_tango_grid(std::cout, result.solutions[i].grid);
                if (i + 1 < result.solutions.size()) std::cout << "\n";
            }
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }
    }
    return 0;
}

// ============================================================================
// Zip Handlers
// ============================================================================

void print_zip_solution(std::ostream& os, const solvedin::ZipBoard& board,
                        const solvedin::ZipSolution& solution) {
    os << "--- Path Step Grid ---\n";
    os << solution.to_string(board.rows(), board.cols());
    os << "Path (" << solution.path.size() << " steps):\n  ";
    for (size_t i = 0; i < solution.path.size(); ++i) {
        os << "(" << solution.path[i].row << ", " << solution.path[i].col << ")";
        if (i + 1 < solution.path.size()) {
            os << " -> ";
            if ((i + 1) % 6 == 0) os << "\n  ";
        }
    }
    os << "\n";
}

int handle_zip_generate(int argc, char* argv[]) {
    int rows = 0;
    int cols = 0;
    int checkpoints = 0;
    uint64_t seed = 0;
    bool rows_set = false;
    bool cols_set = false;
    bool checkpoints_set = false;
    bool seed_set = false;
    std::string output_path;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_zip_help(std::cout);
            return 0;
        } else if (arg == "--rows" && i + 1 < argc) {
            rows = std::stoi(argv[++i]);
            rows_set = true;
        } else if (arg.starts_with("--rows=")) {
            rows = std::stoi(std::string(arg.substr(7)));
            rows_set = true;
        } else if (arg == "--cols" && i + 1 < argc) {
            cols = std::stoi(argv[++i]);
            cols_set = true;
        } else if (arg.starts_with("--cols=")) {
            cols = std::stoi(std::string(arg.substr(7)));
            cols_set = true;
        } else if (arg == "--checkpoints" && i + 1 < argc) {
            checkpoints = std::stoi(argv[++i]);
            checkpoints_set = true;
        } else if (arg.starts_with("--checkpoints=")) {
            checkpoints = std::stoi(std::string(arg.substr(14)));
            checkpoints_set = true;
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = std::stoull(argv[++i]);
            seed_set = true;
        } else if (arg.starts_with("--seed=")) {
            seed = std::stoull(std::string(arg.substr(7)));
            seed_set = true;
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg.starts_with("--output=")) {
            output_path = arg.substr(9);
        } else {
            std::cerr << "Error: Unrecognized option '" << arg << "' for zip generate\n\n";
            print_zip_help(std::cerr);
            return 1;
        }
    }

    if (!rows_set || !cols_set || !checkpoints_set || !seed_set) {
        std::cerr << "Error: Missing required options. Please provide --rows <R>, --cols <C>, --checkpoints <K>, --seed <S>\n";
        return 1;
    }

    try {
        auto board = solvedin::ZipGenerator::generate(rows, cols, checkpoints, seed);
        std::string formatted = board.to_string();

        if (!output_path.empty()) {
            std::ofstream f(output_path);
            if (!f.is_open()) {
                std::cerr << "Error: Failed to write to output file '" << output_path << "'\n";
                return 1;
            }
            f << formatted;
        } else {
            std::cout << formatted;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error generating zip puzzle: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}

int handle_zip_solve(int argc, char* argv[]) {
    std::string board_filepath;
    bool check_unique = false;
    bool show_stats = false;
    std::string export_svg_path;
    solvedin::SolveOptions options;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_zip_help(std::cout);
            return 0;
        } else if (arg == "--check-unique") {
            check_unique = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else if (arg == "--export-svg" && i + 1 < argc) {
            export_svg_path = argv[++i];
        } else if (arg.starts_with("--export-svg=")) {
            export_svg_path = arg.substr(13);
        } else if (arg == "--max-nodes" && i + 1 < argc) {
            options.max_nodes = std::stoull(argv[++i]);
        } else if (arg.starts_with("--max-nodes=")) {
            options.max_nodes = std::stoull(std::string(arg.substr(12)));
        } else if (arg == "--max-time-ms" && i + 1 < argc) {
            options.max_time_ms = std::stod(argv[++i]);
        } else if (arg.starts_with("--max-time-ms=")) {
            options.max_time_ms = std::stod(std::string(arg.substr(14)));
        } else if (arg.starts_with("-")) {
            std::cerr << "Error: Unrecognized option '" << arg << "'\n\n";
            print_zip_help(std::cerr);
            return 1;
        } else {
            if (board_filepath.empty()) {
                board_filepath = arg;
            } else {
                std::cerr << "Error: Unexpected argument '" << arg << "'\n\n";
                print_zip_help(std::cerr);
                return 1;
            }
        }
    }

    if (board_filepath.empty()) {
        std::cerr << "Error: Missing board file argument\n\n";
        print_zip_help(std::cerr);
        return 1;
    }

    auto [success, maybe_board, error_message] = solvedin::ZipBoard::load_from_file(board_filepath);
    if (!success || !maybe_board) {
        std::cerr << "Error: Malformed zip board file '" << board_filepath << "': " << error_message << "\n";
        return 2;
    }

    const auto& board = *maybe_board;
    auto result = solvedin::solve_zip(board, check_unique, options);

    if (!export_svg_path.empty() && !result.solutions.empty()) {
        if (!write_svg_file(export_svg_path, solvedin::export_zip_svg(board, result.solutions[0]))) {
            std::cerr << "Warning: Failed to export SVG to '" << export_svg_path << "'\n";
        } else {
            std::cout << "Exported solution visualization to SVG: " << export_svg_path << "\n";
        }
    }

    if (!check_unique) {
        if (result.status == solvedin::SolveStatus::INCONCLUSIVE) {
            std::cout << "Search inconclusive: budget exhausted.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }
        if (result.status == solvedin::SolveStatus::NO_SOLUTION || result.solutions.empty()) {
            std::cout << "No solution exists.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }

        std::cout << "Solution found:\n";
        print_zip_solution(std::cout, board, result.solutions[0]);
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    } else {
        switch (result.status) {
        case solvedin::SolveStatus::INCONCLUSIVE:
            std::cout << "Search inconclusive: budget exhausted.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::NO_SOLUTION:
            std::cout << "No solution exists.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::UNIQUE_SOLUTION:
            std::cout << "Unique solution found:\n";
            print_zip_solution(std::cout, board, result.solutions[0]);
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::MULTIPLE_SOLUTIONS:
            std::cout << "Multiple solutions found (at least 2):\n\n";
            for (size_t i = 0; i < result.solutions.size(); ++i) {
                std::cout << "--- Solution " << (i + 1) << " ---\\n";
                print_zip_solution(std::cout, board, result.solutions[i]);
                if (i + 1 < result.solutions.size()) std::cout << "\n";
            }
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }
    }
    return 0;
}

// ============================================================================
// Patches Handlers
// ============================================================================

void print_patches_solution(std::ostream& os, const solvedin::PatchesBoard& board,
                            const solvedin::PatchesSolution& solution) {
    os << "--- Patch Tiling Grid ---\n";
    os << solution.to_string(board.rows(), board.cols());
    os << "Patches (" << solution.patches.size() << " total):\n";
    static const std::string symbols =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@#$%&*";
    for (size_t i = 0; i < solution.patches.size(); ++i) {
        char ch = symbols[i % symbols.size()];
        const auto& p = solution.patches[i];
        auto cl = board.clue_at(p.clue_pos.row, p.clue_pos.col);
        std::string label = cl.has_value() ? cl->label() : "?";
        os << "  Patch " << ch << " [clue '" << label << "' at (" << p.clue_pos.row << ", "
           << p.clue_pos.col << "), size " << p.cells.size() << "]: ";
        for (size_t ci = 0; ci < p.cells.size(); ++ci) {
            os << "(" << p.cells[ci].row << "," << p.cells[ci].col << ")"
               << (ci + 1 < p.cells.size() ? " " : "");
        }
        os << "\n";
    }
}

int handle_patches_solve(int argc, char* argv[]) {
    std::string board_filepath;
    bool check_unique = false;
    bool show_stats = false;
    std::string export_svg_path;
    solvedin::SolveOptions options;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_patches_help(std::cout);
            return 0;
        } else if (arg == "--check-unique") {
            check_unique = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else if (arg == "--export-svg" && i + 1 < argc) {
            export_svg_path = argv[++i];
        } else if (arg.starts_with("--export-svg=")) {
            export_svg_path = arg.substr(13);
        } else if (arg == "--max-nodes" && i + 1 < argc) {
            options.max_nodes = std::stoull(argv[++i]);
        } else if (arg.starts_with("--max-nodes=")) {
            options.max_nodes = std::stoull(std::string(arg.substr(12)));
        } else if (arg == "--max-time-ms" && i + 1 < argc) {
            options.max_time_ms = std::stod(argv[++i]);
        } else if (arg.starts_with("--max-time-ms=")) {
            options.max_time_ms = std::stod(std::string(arg.substr(14)));
        } else if (arg.starts_with("-")) {
            std::cerr << "Error: Unrecognized option '" << arg << "'\n\n";
            print_patches_help(std::cerr);
            return 1;
        } else {
            if (board_filepath.empty()) {
                board_filepath = arg;
            } else {
                std::cerr << "Error: Unexpected argument '" << arg << "'\n\n";
                print_patches_help(std::cerr);
                return 1;
            }
        }
    }

    if (board_filepath.empty()) {
        std::cerr << "Error: Missing board file argument\n\n";
        print_patches_help(std::cerr);
        return 1;
    }

    auto [success, maybe_board, error_message] = solvedin::PatchesBoard::load_from_file(board_filepath);
    if (!success || !maybe_board) {
        std::cerr << "Error: Malformed patches board file '" << board_filepath << "': " << error_message << "\n";
        return 2;
    }

    const auto& board = *maybe_board;
    auto result = solvedin::solve_patches(board, check_unique, options);

    if (!export_svg_path.empty() && !result.solutions.empty()) {
        if (!write_svg_file(export_svg_path, solvedin::export_patches_svg(board, result.solutions[0]))) {
            std::cerr << "Warning: Failed to export SVG to '" << export_svg_path << "'\n";
        } else {
            std::cout << "Exported solution visualization to SVG: " << export_svg_path << "\n";
        }
    }

    if (!check_unique) {
        if (result.status == solvedin::SolveStatus::INCONCLUSIVE) {
            std::cout << "Search inconclusive: budget exhausted.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }
        if (result.status == solvedin::SolveStatus::NO_SOLUTION || result.solutions.empty()) {
            std::cout << "No solution exists.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }

        std::cout << "Solution found:\n";
        print_patches_solution(std::cout, board, result.solutions[0]);
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    } else {
        switch (result.status) {
        case solvedin::SolveStatus::INCONCLUSIVE:
            std::cout << "Search inconclusive: budget exhausted.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::NO_SOLUTION:
            std::cout << "No solution exists.\n";
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::UNIQUE_SOLUTION:
            std::cout << "Unique solution found:\n";
            print_patches_solution(std::cout, board, result.solutions[0]);
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        case solvedin::SolveStatus::MULTIPLE_SOLUTIONS:
            std::cout << "Multiple solutions found (at least 2):\n\n";
            for (size_t i = 0; i < result.solutions.size(); ++i) {
                std::cout << "--- Solution " << (i + 1) << " ---\n";
                print_patches_solution(std::cout, board, result.solutions[i]);
                if (i + 1 < result.solutions.size()) std::cout << "\n";
            }
            if (show_stats) print_generic_stats(std::cout, result.stats);
            return 0;
        }
    }
    return 0;
}

// ============================================================================
// Wend Handlers
// ============================================================================

int handle_wend_solve(int argc, char* argv[]) {
    std::string board_filepath;
    std::string dict_filepath;
    std::string export_svg_path;
    bool check_unique = false;
    bool show_stats = false;
    solvedin::SolveOptions options;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_wend_help(std::cout);
            return 0;
        } else if (arg == "--check-unique") {
            check_unique = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else if (arg == "--export-svg" && i + 1 < argc) {
            export_svg_path = argv[++i];
        } else if (arg.starts_with("--export-svg=")) {
            export_svg_path = arg.substr(13);
        } else if (arg == "--dict") {
            if (i + 1 < argc) {
                dict_filepath = argv[++i];
            } else {
                std::cerr << "Error: --dict requires a filepath argument\n";
                return 1;
            }
        } else if (arg == "--max-nodes") {
            if (i + 1 < argc) {
                try {
                    options.max_nodes = std::stoull(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid number for --max-nodes\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --max-nodes requires an argument\n";
                return 1;
            }
        } else if (arg == "--max-time-ms") {
            if (i + 1 < argc) {
                try {
                    options.max_time_ms = std::stod(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid number for --max-time-ms\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --max-time-ms requires an argument\n";
                return 1;
            }
        } else if (!arg.starts_with("-") && board_filepath.empty()) {
            board_filepath = std::string(arg);
        } else {
            std::cerr << "Error: Unknown or unexpected option '" << arg << "'\n\n";
            print_wend_help(std::cerr);
            return 1;
        }
    }

    if (board_filepath.empty()) {
        std::cerr << "Error: No board file specified\n\n";
        print_wend_help(std::cerr);
        return 1;
    }

    auto [success, maybe_board, error_message] = solvedin::WendBoard::load_from_file(board_filepath);
    if (!success || !maybe_board) {
        std::cerr << "Error: Malformed wend board file '" << board_filepath << "': " << error_message << "\n";
        return 2;
    }

    const auto& board = *maybe_board;
    solvedin::Trie lexicon = dict_filepath.empty()
        ? solvedin::Trie::get_default_lexicon()
        : solvedin::Trie::load_from_file(dict_filepath);

    solvedin::WendSolver solver(board, lexicon, options);
    auto result = solver.solve(check_unique);

    if (!export_svg_path.empty() && !result.solutions.empty()) {
        if (!write_svg_file(export_svg_path, solvedin::export_wend_svg(board, result.solutions[0]))) {
            std::cerr << "Warning: Failed to export SVG to '" << export_svg_path << "'\n";
        } else {
            std::cout << "Exported solution visualization to SVG: " << export_svg_path << "\n";
        }
    }

    if (result.status == solvedin::SolveStatus::INCONCLUSIVE) {
        std::cout << "Search inconclusive: budget exhausted.\n";
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    }

    if (result.status == solvedin::SolveStatus::NO_SOLUTION || result.solutions.empty()) {
        std::cout << "No solution exists.\n";
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    }

    if (!check_unique) {
        std::cout << "Solution found:\n";
        std::cout << result.solutions[0].to_string(board.rows(), board.cols());
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    } else {
        if (result.status == solvedin::SolveStatus::UNIQUE_SOLUTION) {
            std::cout << "Unique solution found:\n";
            std::cout << result.solutions[0].to_string(board.rows(), board.cols());
        } else {
            std::cout << "Multiple solutions found (at least " << result.solutions.size() << "):\n\n";
            for (size_t i = 0; i < result.solutions.size(); ++i) {
                std::cout << "--- Solution " << (i + 1) << " ---\n";
                std::cout << result.solutions[i].to_string(board.rows(), board.cols());
                if (i + 1 < result.solutions.size()) std::cout << "\n";
            }
        }
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    }
}

// ============================================================================
// Crossclimb Handlers
// ============================================================================

int handle_crossclimb_solve(int argc, char* argv[]) {
    std::string board_filepath;
    std::string dict_filepath;
    std::string export_svg_path;
    bool check_unique = true;
    bool show_stats = false;
    solvedin::SolveOptions options;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_crossclimb_help(std::cout);
            return 0;
        } else if (arg == "--check-unique") {
            check_unique = true;
        } else if (arg == "--stats") {
            show_stats = true;
        } else if (arg == "--export-svg" && i + 1 < argc) {
            export_svg_path = argv[++i];
        } else if (arg.starts_with("--export-svg=")) {
            export_svg_path = arg.substr(13);
        } else if (arg == "--dict") {
            if (i + 1 < argc) {
                dict_filepath = argv[++i];
            } else {
                std::cerr << "Error: --dict requires a filepath argument\n";
                return 1;
            }
        } else if (arg == "--max-nodes") {
            if (i + 1 < argc) {
                try {
                    options.max_nodes = std::stoull(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid number for --max-nodes\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --max-nodes requires an argument\n";
                return 1;
            }
        } else if (arg == "--max-time-ms") {
            if (i + 1 < argc) {
                try {
                    options.max_time_ms = std::stod(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid number for --max-time-ms\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --max-time-ms requires an argument\n";
                return 1;
            }
        } else if (!arg.starts_with("-") && board_filepath.empty()) {
            board_filepath = std::string(arg);
        } else {
            std::cerr << "Error: Unknown or unexpected option '" << arg << "'\n\n";
            print_crossclimb_help(std::cerr);
            return 1;
        }
    }

    if (board_filepath.empty()) {
        std::cerr << "Error: No board file specified\n\n";
        print_crossclimb_help(std::cerr);
        return 1;
    }

    auto [success, maybe_board, error_message] = solvedin::CrossclimbBoard::load_from_file(board_filepath);
    if (!success || !maybe_board) {
        std::cerr << "Error: Malformed crossclimb board file '" << board_filepath << "': " << error_message << "\n";
        return 2;
    }

    const auto& board = *maybe_board;
    solvedin::Trie lexicon = dict_filepath.empty()
        ? solvedin::Trie::get_default_lexicon()
        : solvedin::Trie::load_from_file(dict_filepath);

    solvedin::CrossclimbSolver solver(board, lexicon, options);
    auto result = solver.solve(check_unique);

    if (!export_svg_path.empty() && !result.solutions.empty()) {
        if (!write_svg_file(export_svg_path, solvedin::export_crossclimb_svg(board, result.solutions[0]))) {
            std::cerr << "Warning: Failed to export SVG to '" << export_svg_path << "'\n";
        } else {
            std::cout << "Exported solution visualization to SVG: " << export_svg_path << "\n";
        }
    }

    if (result.status == solvedin::SolveStatus::INCONCLUSIVE) {
        std::cout << "Search inconclusive: budget exhausted.\n";
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    }

    if (result.status == solvedin::SolveStatus::NO_SOLUTION || result.solutions.empty()) {
        std::cout << "No solution exists.\n";
        if (show_stats) print_generic_stats(std::cout, result.stats);
        return 0;
    }

    if (result.status == solvedin::SolveStatus::UNIQUE_SOLUTION) {
        std::cout << "Unique solution found:\n";
        std::cout << result.solutions[0].to_string();
    } else {
        std::cout << "Multiple solutions found (ambiguous ladder, at least " << result.solutions.size() << "):\n\n";
        for (size_t i = 0; i < result.solutions.size(); ++i) {
            std::cout << "--- Solution " << (i + 1) << " ---\n";
            std::cout << result.solutions[i].to_string();
            if (i + 1 < result.solutions.size()) std::cout << "\n";
        }
    }
    if (show_stats) print_generic_stats(std::cout, result.stats);
    return 0;
}

// ============================================================================
// Pinpoint Handlers
// ============================================================================

int handle_pinpoint_guess(int argc, char* argv[]) {
    std::string clues_filepath;
    std::string lexicon_filepath;
    std::string clues_arg;
    double threshold = 0.50;

    for (int i = 3; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_pinpoint_help(std::cout);
            return 0;
        } else if (arg == "--lexicon") {
            if (i + 1 < argc) {
                lexicon_filepath = argv[++i];
            } else {
                std::cerr << "Error: --lexicon requires a filepath argument\n";
                return 1;
            }
        } else if (arg == "--threshold") {
            if (i + 1 < argc) {
                try {
                    threshold = std::stod(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid number for --threshold\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --threshold requires an argument\n";
                return 1;
            }
        } else if (arg == "--clues") {
            if (i + 1 < argc) {
                clues_arg = argv[++i];
            } else {
                std::cerr << "Error: --clues requires an argument\n";
                return 1;
            }
        } else if (!arg.starts_with("-") && clues_filepath.empty()) {
            clues_filepath = std::string(arg);
        } else {
            std::cerr << "Error: Unknown or unexpected option '" << arg << "'\n\n";
            print_pinpoint_help(std::cerr);
            return 1;
        }
    }

    std::vector<std::string> clues;
    if (!clues_arg.empty()) {
        std::istringstream iss(clues_arg);
        std::string c;
        while (std::getline(iss, c, ',')) {
            size_t start = 0;
            while (start < c.size() && std::isspace(static_cast<unsigned char>(c[start]))) start++;
            size_t end = c.size();
            while (end > start && std::isspace(static_cast<unsigned char>(c[end - 1]))) end--;
            if (end > start) {
                clues.push_back(c.substr(start, end - start));
            }
        }
    } else if (!clues_filepath.empty()) {
        clues = solvedin::PinpointGuesser::load_clues_from_file(clues_filepath);
        if (clues.empty()) {
            std::cerr << "Error: Malformed or empty clues file '" << clues_filepath << "'\n";
            return 2;
        }
    } else {
        std::cerr << "Error: No clues specified (use clues file or --clues)\n\n";
        print_pinpoint_help(std::cerr);
        return 1;
    }

    solvedin::PinpointLexicon lexicon = lexicon_filepath.empty()
        ? solvedin::PinpointLexicon::load_from_file("fixtures/pinpoint_lexicon.txt")
        : solvedin::PinpointLexicon::load_from_file(lexicon_filepath);

    solvedin::PinpointGuesser guesser(std::move(lexicon), threshold);
    auto result = guesser.guess(clues);
    std::cout << result.to_string();
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_general_help(std::cout);
        return 1;
    }

    const std::string_view first_arg = argv[1];
    if (first_arg == "-h" || first_arg == "--help") {
        print_general_help(std::cout);
        return 0;
    }

    if (first_arg == "queens") {
        if (argc < 3) {
            print_queens_help(std::cout);
            return 1;
        }
        const std::string_view second_arg = argv[2];
        if (second_arg == "-h" || second_arg == "--help") {
            print_queens_help(std::cout);
            return 0;
        }
        solvedin::ReasoningMode rmode;
        if (reasoning_command(second_arg, rmode)) {
            return handle_reasoning(argc, argv, rmode, [](const std::string& file, const solvedin::ReasoningOptions& opt,
                                                         const auto&, std::string& err) -> std::optional<solvedin::ReasoningReport> {
                auto parsed = solvedin::Board::load_from_file(file);
                if (!parsed.success || !parsed.board) { err = "Malformed queens board file '" + file + "': " + parsed.error_message; return std::nullopt; }
                return solvedin::reason_queens(*parsed.board, opt);
            }, print_queens_help);
        }
        if (second_arg == "solve") {
            return handle_queens_solve(argc, argv);
        } else if (second_arg == "generate") {
            return handle_queens_generate(argc, argv);
        } else {
            std::cerr << "Error: Unknown queens command '" << second_arg << "'. Supported commands: solve, generate, hint, deduce, explain\n\n";
            print_queens_help(std::cerr);
            return 1;
        }
    } else if (first_arg == "sudoku") {
        if (argc < 3) {
            print_sudoku_help(std::cout);
            return 1;
        }
        const std::string_view second_arg = argv[2];
        if (second_arg == "-h" || second_arg == "--help") {
            print_sudoku_help(std::cout);
            return 0;
        }
        solvedin::ReasoningMode rmode;
        if (reasoning_command(second_arg, rmode)) {
            return handle_reasoning(argc, argv, rmode, [](const std::string& file, const solvedin::ReasoningOptions& opt,
                                                         const auto&, std::string& err) -> std::optional<solvedin::ReasoningReport> {
                auto parsed = solvedin::SudokuBoard::load_from_file(file);
                if (!parsed.success || !parsed.board) { err = "Malformed sudoku board file '" + file + "': " + parsed.error_message; return std::nullopt; }
                return solvedin::reason_sudoku(*parsed.board, opt);
            }, print_sudoku_help);
        }
        if (second_arg == "solve") {
            return handle_sudoku_solve(argc, argv);
        } else if (second_arg == "generate") {
            return handle_sudoku_generate(argc, argv);
        } else {
            std::cerr << "Error: Unknown sudoku command '" << second_arg << "'. Supported commands: solve, generate, hint, deduce, explain\n\n";
            print_sudoku_help(std::cerr);
            return 1;
        }
    } else if (first_arg == "tango") {
        if (argc < 3) {
            print_tango_help(std::cout);
            return 1;
        }
        const std::string_view second_arg = argv[2];
        if (second_arg == "-h" || second_arg == "--help") {
            print_tango_help(std::cout);
            return 0;
        }
        solvedin::ReasoningMode rmode;
        if (reasoning_command(second_arg, rmode)) {
            return handle_reasoning(argc, argv, rmode, [](const std::string& file, const solvedin::ReasoningOptions& opt,
                                                         const auto&, std::string& err) -> std::optional<solvedin::ReasoningReport> {
                auto parsed = solvedin::TangoBoard::load_from_file(file);
                if (!parsed.success || !parsed.board) { err = "Malformed tango board file '" + file + "': " + parsed.error_message; return std::nullopt; }
                return solvedin::reason_tango(*parsed.board, opt);
            }, print_tango_help);
        }
        if (second_arg == "solve") {
            return handle_tango_solve(argc, argv);
        } else if (second_arg == "generate") {
            return handle_tango_generate(argc, argv);
        } else {
            std::cerr << "Error: Unknown tango command '" << second_arg << "'. Supported commands: solve, generate, hint, deduce, explain\n\n";
            print_tango_help(std::cerr);
            return 1;
        }
    } else if (first_arg == "zip") {
        if (argc < 3) {
            print_zip_help(std::cout);
            return 1;
        }
        const std::string_view second_arg = argv[2];
        if (second_arg == "-h" || second_arg == "--help") {
            print_zip_help(std::cout);
            return 0;
        }
        solvedin::ReasoningMode rmode;
        if (reasoning_command(second_arg, rmode)) {
            return handle_reasoning(argc, argv, rmode, [](const std::string& file, const solvedin::ReasoningOptions& opt,
                                                         const auto&, std::string& err) -> std::optional<solvedin::ReasoningReport> {
                auto parsed = solvedin::ZipBoard::load_from_file(file);
                if (!parsed.success || !parsed.board) { err = "Malformed zip board file '" + file + "': " + parsed.error_message; return std::nullopt; }
                return solvedin::reason_zip(*parsed.board, opt);
            }, print_zip_help);
        }
        if (second_arg == "solve") {
            return handle_zip_solve(argc, argv);
        } else if (second_arg == "generate") {
            return handle_zip_generate(argc, argv);
        } else {
            std::cerr << "Error: Unknown zip command '" << second_arg << "'. Supported commands: solve, generate, hint, deduce, explain\n\n";
            print_zip_help(std::cerr);
            return 1;
        }
    } else if (first_arg == "patches") {
        if (argc < 3) {
            print_patches_help(std::cout);
            return 1;
        }
        const std::string_view second_arg = argv[2];
        if (second_arg == "-h" || second_arg == "--help") {
            print_patches_help(std::cout);
            return 0;
        }
        solvedin::ReasoningMode rmode;
        if (reasoning_command(second_arg, rmode)) {
            return handle_reasoning(argc, argv, rmode, [](const std::string& file, const solvedin::ReasoningOptions& opt,
                                                         const auto&, std::string& err) -> std::optional<solvedin::ReasoningReport> {
                auto parsed = solvedin::PatchesBoard::load_from_file(file);
                if (!parsed.success || !parsed.board) { err = "Malformed patches board file '" + file + "': " + parsed.error_message; return std::nullopt; }
                return solvedin::reason_patches(*parsed.board, opt);
            }, print_patches_help);
        }
        if (second_arg == "solve") {
            return handle_patches_solve(argc, argv);
        } else {
            std::cerr << "Error: Unknown patches command '" << second_arg << "'. Supported commands: solve, hint, deduce, explain\n\n";
            print_patches_help(std::cerr);
            return 1;
        }
    } else if (first_arg == "wend") {
        if (argc < 3) {
            print_wend_help(std::cout);
            return 1;
        }
        const std::string_view second_arg = argv[2];
        if (second_arg == "-h" || second_arg == "--help") {
            print_wend_help(std::cout);
            return 0;
        }
        solvedin::ReasoningMode rmode;
        if (reasoning_command(second_arg, rmode)) {
            return handle_reasoning(argc, argv, rmode, [](const std::string& file, const solvedin::ReasoningOptions& opt,
                                                         const auto& extra, std::string& err) -> std::optional<solvedin::ReasoningReport> {
                auto parsed = solvedin::WendBoard::load_from_file(file);
                if (!parsed.success || !parsed.board) { err = "Malformed wend board file '" + file + "': " + parsed.error_message; return std::nullopt; }
                std::string dict;
                for (const auto& [k, v] : extra) if (k == "--dict") dict = v;
                solvedin::Trie lex = dict.empty() ? solvedin::Trie::get_default_lexicon() : solvedin::Trie::load_from_file(dict);
                return solvedin::reason_wend(*parsed.board, lex, dict.empty() ? "(embedded test lexicon)" : dict, opt);
            }, print_wend_help, {"--dict"});
        }
        if (second_arg == "solve") {
            return handle_wend_solve(argc, argv);
        } else {
            std::cerr << "Error: Unknown wend command '" << second_arg << "'. Supported commands: solve, hint, deduce, explain\n\n";
            print_wend_help(std::cerr);
            return 1;
        }
    } else if (first_arg == "crossclimb") {
        if (argc < 3) {
            print_crossclimb_help(std::cout);
            return 1;
        }
        const std::string_view second_arg = argv[2];
        if (second_arg == "-h" || second_arg == "--help") {
            print_crossclimb_help(std::cout);
            return 0;
        }
        solvedin::ReasoningMode rmode;
        if (reasoning_command(second_arg, rmode)) {
            return handle_reasoning(argc, argv, rmode, [](const std::string& file, const solvedin::ReasoningOptions& opt,
                                                         const auto& extra, std::string& err) -> std::optional<solvedin::ReasoningReport> {
                auto parsed = solvedin::CrossclimbBoard::load_from_file(file);
                if (!parsed.success || !parsed.board) { err = "Malformed crossclimb file '" + file + "': " + parsed.error_message; return std::nullopt; }
                std::string dict;
                for (const auto& [k, v] : extra) if (k == "--dict") dict = v;
                solvedin::Trie lex = dict.empty() ? solvedin::Trie::get_default_lexicon() : solvedin::Trie::load_from_file(dict);
                return solvedin::reason_crossclimb(*parsed.board, lex, opt);
            }, print_crossclimb_help, {"--dict"});
        }
        if (second_arg == "solve") {
            return handle_crossclimb_solve(argc, argv);
        } else {
            std::cerr << "Error: Unknown crossclimb command '" << second_arg << "'. Supported commands: solve, hint, deduce, explain\n\n";
            print_crossclimb_help(std::cerr);
            return 1;
        }
    } else if (first_arg == "pinpoint") {
        if (argc < 3) {
            print_pinpoint_help(std::cout);
            return 1;
        }
        const std::string_view second_arg = argv[2];
        if (second_arg == "-h" || second_arg == "--help") {
            print_pinpoint_help(std::cout);
            return 0;
        }
        if (second_arg == "explain") {
            return handle_reasoning(argc, argv, solvedin::ReasoningMode::EXPLAIN, [](const std::string& file, const solvedin::ReasoningOptions&,
                                                         const auto& extra, std::string& err) -> std::optional<solvedin::ReasoningReport> {
                std::string lexf, dictf;
                double threshold = 0.50;
                for (const auto& [k, v] : extra) { if (k == "--lexicon") lexf = v; if (k == "--dict") dictf = v; if (k == "--threshold") threshold = std::stod(v); }
                auto clues = solvedin::PinpointGuesser::load_clues_from_file(file);
                if (clues.empty()) { err = "No clues in '" + file + "'"; return std::nullopt; }
                auto lex = lexf.empty() ? solvedin::PinpointLexicon::get_default_lexicon() : solvedin::PinpointLexicon::load_from_file(lexf);
                std::unordered_set<std::string> words;
                if (!dictf.empty()) {
                    std::ifstream in(dictf);
                    if (!in) { err = "Cannot read word list '" + dictf + "'"; return std::nullopt; }
                    std::string w;
                    while (std::getline(in, w)) {
                        std::string up;
                        for (char c : w) if (std::isalpha(static_cast<unsigned char>(c))) up += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                        if (up.size() >= 3) words.insert(up);
                    }
                }
                solvedin::PinpointReasoningOptions po;
                po.lexicon_threshold = threshold;
                return solvedin::reason_pinpoint(clues, lex, dictf.empty() ? nullptr : &words, po);
            }, print_pinpoint_help, {"--lexicon", "--dict", "--threshold"});
        }
        if (second_arg == "guess") {
            return handle_pinpoint_guess(argc, argv);
        } else {
            std::cerr << "Error: Unknown pinpoint command '" << second_arg << "'. Supported commands: guess, explain\n\n";
            print_pinpoint_help(std::cerr);
            return 1;
        }
    } else {
        std::cerr << "Error: Unknown game '" << first_arg << "'. Supported games: queens, sudoku, tango, zip, patches, wend, crossclimb, pinpoint\n\n";
        print_general_help(std::cerr);
        return 1;
    }
}
