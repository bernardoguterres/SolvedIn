#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

struct ExecResult {
    int exit_code{0};
    std::string output;
};

ExecResult run_cmd(const std::string& cmd) {
    std::array<char, 256> buffer{};
    std::string result;
    std::string full_cmd = cmd + " 2>&1";
    FILE* pipe = popen(full_cmd.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    int status = pclose(pipe);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {exit_code, result};
}

} // namespace

int main(int argc, char* argv[]) {
    std::string bin = "./solvedin";
    if (argc > 1) {
        bin = argv[1];
    }

    std::cout << "Running CLI Tests against binary: " << bin << " ...\n";

    // 1. General help
    {
        auto res = run_cmd(bin + " --help");
        assert(res.exit_code == 0);
        assert(res.output.find("SolvedIn") != std::string::npos);
        assert(res.output.find("sudoku") != std::string::npos);
        assert(res.output.find("tango") != std::string::npos);
        assert(res.output.find("zip") != std::string::npos);
        assert(res.output.find("patches") != std::string::npos);
        std::cout << "[PASS] CLI general help\n";
    }

    // 2. Queens help
    {
        auto res = run_cmd(bin + " queens --help");
        assert(res.exit_code == 0);
        assert(res.output.find("LinkedIn Queens Puzzle Solver") != std::string::npos);
        std::cout << "[PASS] CLI queens help\n";
    }

    // 3. No arguments
    {
        auto res = run_cmd(bin);
        assert(res.exit_code == 1);
        assert(res.output.find("Usage:") != std::string::npos);
        std::cout << "[PASS] CLI no args displays help and exits with 1\n";
    }

    // 4. Missing solve argument
    {
        auto res = run_cmd(bin + " queens solve");
        assert(res.exit_code == 1);
        assert(res.output.find("Missing board file argument") != std::string::npos);
        std::cout << "[PASS] CLI missing board file exits with 1\n";
    }

    // 5. Non-existent file
    {
        auto res = run_cmd(bin + " queens solve nonexistent_file_xyz.txt");
        assert(res.exit_code == 2);
        assert(res.output.find("Malformed board file") != std::string::npos);
        std::cout << "[PASS] CLI non-existent file exits with 2\n";
    }

    // 6. Malformed board file
    {
        auto res = run_cmd(bin + " queens solve fixtures/malformed_dimensions.txt");
        assert(res.exit_code == 2);
        assert(res.output.find("Malformed board file") != std::string::npos);
        std::cout << "[PASS] CLI malformed file exits with 2\n";
    }

    // 7. Solve valid unique board (default baseline)
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_8x8_unique.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("Solution found:") != std::string::npos);
        assert(res.output.find("Q") != std::string::npos);
        std::cout << "[PASS] CLI solve unique (baseline)\n";
    }

    // 8. Solve with MRV solver
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_8x8_unique.txt --solver mrv");
        assert(res.exit_code == 0);
        assert(res.output.find("Solution found:") != std::string::npos);
        std::cout << "[PASS] CLI solve unique (mrv)\n";
    }

    // 9. Solve with BitSet solver
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_8x8_unique.txt --solver bitset");
        assert(res.exit_code == 0);
        assert(res.output.find("Solution found:") != std::string::npos);
        std::cout << "[PASS] CLI solve unique (bitset)\n";
    }

    // 10. Solve with stats flag
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_8x8_unique.txt --solver bitset --stats");
        assert(res.exit_code == 0);
        assert(res.output.find("Search Metrics") != std::string::npos);
        assert(res.output.find("Nodes visited") != std::string::npos);
        assert(res.output.find("Elapsed time") != std::string::npos);
        assert(res.output.find("Solver variant: bitset") != std::string::npos);
        std::cout << "[PASS] CLI solve with --stats\n";
    }

    // 11. Check unique on unique board
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_8x8_unique.txt --check-unique");
        assert(res.exit_code == 0);
        assert(res.output.find("Unique solution found:") != std::string::npos);
        std::cout << "[PASS] CLI check-unique on unique board\n";
    }

    // 12. Check unique on multiple solutions board
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_4x4_multiple.txt --check-unique");
        assert(res.exit_code == 0);
        assert(res.output.find("Multiple solutions found (at least 2):") != std::string::npos);
        assert(res.output.find("--- Solution 1 ---") != std::string::npos);
        assert(res.output.find("--- Solution 2 ---") != std::string::npos);
        std::cout << "[PASS] CLI check-unique on multiple solutions board\n";
    }

    // 13. Solve board with no solution
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_no_solution.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("No solution exists.") != std::string::npos);
        std::cout << "[PASS] CLI solve board with no solution\n";
    }

    // 14. Invalid option
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_8x8_unique.txt --unknown-flag");
        assert(res.exit_code == 1);
        assert(res.output.find("Unrecognized option") != std::string::npos);
        std::cout << "[PASS] CLI unrecognized option exits with 1\n";
    }

    // 15. Invalid solver variant
    {
        auto res = run_cmd(bin + " queens solve fixtures/valid_8x8_unique.txt --solver quantum");
        assert(res.exit_code == 1);
        assert(res.output.find("Invalid solver variant") != std::string::npos);
        std::cout << "[PASS] CLI invalid solver variant exits with 1\n";
    }

    // 16. Generate command: stdout output
    {
        auto res = run_cmd(bin + " queens generate --size 6 --seed 42");
        assert(res.exit_code == 0);
        assert(res.output.rfind("6\n", 0) == 0); // Starts with dimension 6
        std::cout << "[PASS] CLI queens generate --size 6 --seed 42\n";
    }

    // 17. Generate command: file output, solve, and SVG export roundtrip
    {
        std::string tmp_board = "cli_tmp_gen_8.txt";
        std::string tmp_svg = "cli_tmp_gen_8.svg";

        auto gen_res = run_cmd(bin + " queens generate --size 8 --seed 12345 --output " + tmp_board);
        assert(gen_res.exit_code == 0);
        assert(std::filesystem::exists(tmp_board));

        auto solve_res = run_cmd(bin + " queens solve " + tmp_board + " --solver bitset --export-svg " + tmp_svg + " --stats");
        assert(solve_res.exit_code == 0);
        assert(solve_res.output.find("Solution found:") != std::string::npos);
        assert(solve_res.output.find("Exported solution visualization to SVG") != std::string::npos);
        assert(std::filesystem::exists(tmp_svg));

        std::filesystem::remove(tmp_board);
        std::filesystem::remove(tmp_svg);
        std::cout << "[PASS] CLI generate, solve, and SVG export roundtrip\n";
    }

    // 18. Budget exhaustion (inconclusive).
    // Uses a shipped fixture, not a seeded generated board: std::shuffle / uniform_int_distribution are
    // implementation-defined, so the same seed yields different boards on libstdc++ and libc++.
    {
        auto solve_res = run_cmd(bin + " queens solve fixtures/valid_8x8_unique.txt --solver bitset --max-nodes 1 --stats");
        assert(solve_res.exit_code == 0);
        assert(solve_res.output.find("Search inconclusive: budget exhausted") != std::string::npos);
        assert(solve_res.output.find("Budget status : EXHAUSTED") != std::string::npos);

        std::cout << "[PASS] CLI solve budget exhaustion reports inconclusive\n";
    }

    // 19. Sudoku help
    {
        auto res = run_cmd(bin + " sudoku --help");
        assert(res.exit_code == 0);
        assert(res.output.find("LinkedIn Mini Sudoku (6x6) Solver") != std::string::npos);
        std::cout << "[PASS] CLI sudoku help\n";
    }

    // 20. Sudoku solve valid unique
    {
        auto res = run_cmd(bin + " sudoku solve fixtures/sudoku_valid_unique.txt --check-unique --stats");
        assert(res.exit_code == 0);
        assert(res.output.find("Unique solution found:") != std::string::npos);
        assert(res.output.find("1 2 3 | 4 5 6") != std::string::npos);
        assert(res.output.find("Nodes visited :") != std::string::npos);
        std::cout << "[PASS] CLI sudoku solve unique with stats\n";
    }

    // 21. Sudoku solve multiple solutions
    {
        auto res = run_cmd(bin + " sudoku solve fixtures/sudoku_valid_multiple.txt --check-unique");
        assert(res.exit_code == 0);
        assert(res.output.find("Multiple solutions found (at least 2):") != std::string::npos);
        assert(res.output.find("--- Solution 1 ---") != std::string::npos);
        assert(res.output.find("--- Solution 2 ---") != std::string::npos);
        std::cout << "[PASS] CLI sudoku solve multiple\n";
    }

    // 22. Sudoku contradictory givens
    {
        auto res = run_cmd(bin + " sudoku solve fixtures/sudoku_contradictory.txt");
        assert(res.exit_code == 2);
        assert(res.output.find("Malformed sudoku board file") != std::string::npos);
        assert(res.output.find("Contradictory givens") != std::string::npos);
        std::cout << "[PASS] CLI sudoku contradictory givens rejected\n";
    }

    // 23. Tango help
    {
        auto res = run_cmd(bin + " tango --help");
        assert(res.exit_code == 0);
        assert(res.output.find("LinkedIn Tango Solver") != std::string::npos);
        std::cout << "[PASS] CLI tango help\n";
    }

    // 24. Tango solve valid unique
    {
        auto res = run_cmd(bin + " tango solve fixtures/tango_valid_unique.txt --check-unique --stats");
        assert(res.exit_code == 0);
        assert(res.output.find("Unique solution found:") != std::string::npos);
        assert(res.output.find("S M S M M S") != std::string::npos);
        assert(res.output.find("Nodes visited :") != std::string::npos);
        std::cout << "[PASS] CLI tango solve unique with stats\n";
    }

    // 25. Tango edge clue dependent: ignoring clue produces multiple solutions
    {
        auto res1 = run_cmd(bin + " tango solve fixtures/tango_edge_clue_dependent.txt --check-unique");
        assert(res1.exit_code == 0);
        assert(res1.output.find("Unique solution found:") != std::string::npos);

        auto res2 = run_cmd(bin + " tango solve fixtures/tango_edge_clue_ignored.txt --check-unique");
        assert(res2.exit_code == 0);
        assert(res2.output.find("Multiple solutions found (at least 2):") != std::string::npos);
        std::cout << "[PASS] CLI tango edge clue dependency verified\n";
    }

    // 26. Tango contradictory clue
    {
        auto res = run_cmd(bin + " tango solve fixtures/tango_contradictory.txt");
        assert(res.exit_code == 2);
        assert(res.output.find("Malformed tango board file") != std::string::npos);
        assert(res.output.find("Contradictory edge clue") != std::string::npos);
        std::cout << "[PASS] CLI tango contradictory clue rejected\n";
    }

    // 27. Zip help
    {
        auto res = run_cmd(bin + " zip --help");
        assert(res.exit_code == 0);
        assert(res.output.find("LinkedIn Zip Solver") != std::string::npos);
        std::cout << "[PASS] CLI zip help\n";
    }

    // 28. Zip solve valid unique
    {
        auto res = run_cmd(bin + " zip solve fixtures/zip_valid_unique.txt --check-unique --stats");
        assert(res.exit_code == 0);
        assert(res.output.find("Unique solution found:") != std::string::npos);
        assert(res.output.find("--- Path Step Grid ---") != std::string::npos);
        assert(res.output.find("Path (16 steps):") != std::string::npos);
        assert(res.output.find("Nodes visited :") != std::string::npos);
        std::cout << "[PASS] CLI zip solve unique with stats\n";
    }

    // 29. Zip solve blocked fixture
    {
        auto res = run_cmd(bin + " zip solve fixtures/zip_walls_blocked.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("No solution exists.") != std::string::npos);
        std::cout << "[PASS] CLI zip solve blocked\n";
    }

    // 30. Zip malformed fixture
    {
        auto res = run_cmd(bin + " zip solve fixtures/zip_malformed.txt");
        assert(res.exit_code == 2);
        assert(res.output.find("Malformed zip board file") != std::string::npos);
        std::cout << "[PASS] CLI zip malformed fixture rejected\n";
    }

    // 31. Patches help
    {
        auto res = run_cmd(bin + " patches --help");
        assert(res.exit_code == 0);
        assert(res.output.find("LinkedIn Patches Solver") != std::string::npos);
        std::cout << "[PASS] CLI patches help\n";
    }

    // 32. Patches solve valid unique
    {
        auto res = run_cmd(bin + " patches solve fixtures/patches_valid_unique.txt --check-unique --stats");
        assert(res.exit_code == 0);
        assert(res.output.find("Unique solution found:") != std::string::npos);
        assert(res.output.find("--- Patch Tiling Grid ---") != std::string::npos);
        assert(res.output.find("Patches (9 total):") != std::string::npos);
        assert(res.output.find("Nodes visited :") != std::string::npos);
        std::cout << "[PASS] CLI patches solve unique with stats\n";
    }

    // 33. Patches solve freeform
    {
        auto res = run_cmd(bin + " patches solve fixtures/patches_freeform.txt --check-unique");
        assert(res.exit_code == 0);
        assert(res.output.find("Unique solution found:") != std::string::npos);
        assert(res.output.find("Patches (4 total):") != std::string::npos);
        std::cout << "[PASS] CLI patches solve freeform\n";
    }

    // 34. Patches contradictory fixture
    {
        auto res = run_cmd(bin + " patches solve fixtures/patches_contradictory.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("No solution exists.") != std::string::npos);
        std::cout << "[PASS] CLI patches contradictory solve no solution\n";
    }

    // 35. Patches malformed fixture
    {
        auto res = run_cmd(bin + " patches solve fixtures/patches_malformed.txt");
        assert(res.exit_code == 2);
        assert(res.output.find("Malformed patches board file") != std::string::npos);
        std::cout << "[PASS] CLI patches malformed fixture rejected\n";
    }


    // 36. Wend help
    {
        auto res = run_cmd(bin + " wend --help");
        assert(res.exit_code == 0);
        assert(res.output.find("LinkedIn Wend Solver") != std::string::npos);
        std::cout << "[PASS] CLI wend help\n";
    }

    // 37. Wend solve valid unique
    {
        auto res = run_cmd(bin + " wend solve fixtures/wend_valid_unique.txt --check-unique --stats");
        assert(res.exit_code == 0);
        assert(res.output.find("Unique solution found:") != std::string::npos);
        assert(res.output.find("FOR") != std::string::npos);
        assert(res.output.find("VAN") != std::string::npos);
        assert(res.output.find("PIG") != std::string::npos);
        assert(res.output.find("Cover Map:") != std::string::npos);
        assert(res.output.find("Nodes visited :") != std::string::npos);
        std::cout << "[PASS] CLI wend solve unique with stats\n";
    }

    // 38. Wend solve with user-supplied dict
    {
        auto res = run_cmd(bin + " wend solve fixtures/wend_valid_unique.txt --dict fixtures/default_lexicon.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("Solution found:") != std::string::npos);
        std::cout << "[PASS] CLI wend solve user-supplied dict\n";
    }

    // 39. Wend solve ambiguous
    {
        auto res = run_cmd(bin + " wend solve fixtures/wend_ambiguous.txt --check-unique");
        assert(res.exit_code == 0);
        assert(res.output.find("Multiple solutions found") != std::string::npos);
        std::cout << "[PASS] CLI wend solve ambiguous\n";
    }

    // 40. Wend solve no solution
    {
        auto res = run_cmd(bin + " wend solve fixtures/wend_no_solution.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("No solution exists.") != std::string::npos);
        std::cout << "[PASS] CLI wend solve no solution\n";
    }

    // 41. Wend malformed rejected
    {
        auto res = run_cmd(bin + " wend solve fixtures/wend_malformed.txt");
        assert(res.exit_code == 2);
        assert(res.output.find("Malformed wend board file") != std::string::npos);
        std::cout << "[PASS] CLI wend malformed rejected\n";
    }

    // 42. Crossclimb help
    {
        auto res = run_cmd(bin + " crossclimb --help");
        assert(res.exit_code == 0);
        assert(res.output.find("LinkedIn Crossclimb Solver") != std::string::npos);
        std::cout << "[PASS] CLI crossclimb help\n";
    }

    // 43. Crossclimb solve valid unique
    {
        auto res = run_cmd(bin + " crossclimb solve fixtures/crossclimb_valid_unique.txt --stats");
        assert(res.exit_code == 0);
        assert(res.output.find("Unique solution found:") != std::string::npos);
        assert(res.output.find("COLD (START)") != std::string::npos);
        assert(res.output.find("WARN (END)") != std::string::npos);
        assert(res.output.find("Nodes visited :") != std::string::npos);
        std::cout << "[PASS] CLI crossclimb solve unique with stats\n";
    }

    // 44. Crossclimb solve ambiguous
    {
        auto res = run_cmd(bin + " crossclimb solve fixtures/crossclimb_ambiguous.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("Multiple solutions found (ambiguous ladder") != std::string::npos);
        std::cout << "[PASS] CLI crossclimb solve ambiguous\n";
    }

    // 45. Crossclimb missing endpoints with clue candidates
    {
        auto res = run_cmd(bin + " crossclimb solve fixtures/crossclimb_missing_endpoints.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("solution found") != std::string::npos || res.output.find("solutions found") != std::string::npos || res.output.find("Solution found") != std::string::npos);
        std::cout << "[PASS] CLI crossclimb solve missing endpoints\n";
    }

    // 46. Crossclimb solve no solution
    {
        auto res = run_cmd(bin + " crossclimb solve fixtures/crossclimb_no_solution.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("No solution exists.") != std::string::npos);
        std::cout << "[PASS] CLI crossclimb solve no solution\n";
    }

    // 47. Crossclimb malformed rejected
    {
        auto res = run_cmd(bin + " crossclimb solve fixtures/crossclimb_malformed.txt");
        assert(res.exit_code == 2);
        assert(res.output.find("Malformed crossclimb board file") != std::string::npos);
        std::cout << "[PASS] CLI crossclimb malformed rejected\n";
    }

    // 48. Pinpoint help
    {
        auto res = run_cmd(bin + " pinpoint --help");
        assert(res.exit_code == 0);
        assert(res.output.find("LinkedIn Pinpoint Offline Ranked Guesser") != std::string::npos);
        std::cout << "[PASS] CLI pinpoint help\n";
    }

    // 49. Pinpoint guess from file
    {
        auto res = run_cmd(bin + " pinpoint guess fixtures/pinpoint_clues_drives.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("Computer Storage Drives") != std::string::npos);
        assert(res.output.find("confidence: 100.0%") != std::string::npos);
        assert(res.output.find("Supported (5):") != std::string::npos);
        std::cout << "[PASS] CLI pinpoint guess from file\n";
    }

    // 50. Pinpoint guess from CLI --clues
    {
        auto res = run_cmd(bin + " pinpoint guess --clues 'PULL, CREW, TURTLE, V, BOAT'");
        assert(res.exit_code == 0);
        assert(res.output.find("Types of Necklines") != std::string::npos);
        assert(res.output.find("confidence: 100.0%") != std::string::npos);
        std::cout << "[PASS] CLI pinpoint guess from CLI --clues\n";
    }

    // 51. Pinpoint guess weak evidence / unknown
    {
        auto res = run_cmd(bin + " pinpoint guess fixtures/pinpoint_clues_weak.txt");
        assert(res.exit_code == 0);
        assert(res.output.find("STATUS: UNKNOWN") != std::string::npos);
        std::cout << "[PASS] CLI pinpoint guess weak evidence / unknown\n";
    }

    // 52. Sudoku generate command
    {
        auto res = run_cmd(bin + " sudoku generate --seed 42 --remove 20");
        assert(res.exit_code == 0);
        assert(res.output.find("6\n") != std::string::npos);
        std::cout << "[PASS] CLI sudoku generate\n";
    }

    // 53. Tango generate command
    {
        auto res = run_cmd(bin + " tango generate --seed 101 --givens 8 --clues 6");
        assert(res.exit_code == 0);
        assert(res.output.find("GRID") != std::string::npos);
        assert(res.output.find("HORIZONTAL") != std::string::npos);
        assert(res.output.find("VERTICAL") != std::string::npos);
        std::cout << "[PASS] CLI tango generate\n";
    }

    // 54. Zip generate command
    {
        auto res = run_cmd(bin + " zip generate --rows 4 --cols 4 --checkpoints 4 --seed 1001");
        assert(res.exit_code == 0);
        assert(res.output.find("GRID") != std::string::npos);
        // Wall placement depends on std::shuffle, which differs between standard libraries, so do not assert
        // which WALLS_* sections appear; assert instead that the generated puzzle is solvable.
        auto gen = run_cmd(bin + " zip generate --rows 4 --cols 4 --checkpoints 4 --seed 1001 --output cli_tmp_zip_gen.txt");
        assert(gen.exit_code == 0);
        auto solved = run_cmd(bin + " zip solve cli_tmp_zip_gen.txt");
        assert(solved.exit_code == 0);
        std::filesystem::remove("cli_tmp_zip_gen.txt");
        std::cout << "[PASS] CLI zip generate\n";
    }

    // 55. Sudoku solve with --export-svg
    {
        auto res = run_cmd(bin + " sudoku solve fixtures/sudoku_valid_unique.txt --export-svg /tmp/cli_sudoku.svg");
        assert(res.exit_code == 0);
        assert(res.output.find("Exported solution visualization to SVG: /tmp/cli_sudoku.svg") != std::string::npos);
        assert(std::filesystem::exists("/tmp/cli_sudoku.svg"));
        std::cout << "[PASS] CLI sudoku solve with --export-svg\n";
    }

    // 56. Tango solve with --export-svg
    {
        auto res = run_cmd(bin + " tango solve fixtures/tango_valid_unique.txt --export-svg /tmp/cli_tango.svg");
        assert(res.exit_code == 0);
        assert(res.output.find("Exported solution visualization to SVG: /tmp/cli_tango.svg") != std::string::npos);
        assert(std::filesystem::exists("/tmp/cli_tango.svg"));
        std::cout << "[PASS] CLI tango solve with --export-svg\n";
    }

    // 57. Zip solve with --export-svg
    {
        auto res = run_cmd(bin + " zip solve fixtures/zip_valid_unique.txt --export-svg /tmp/cli_zip.svg");
        assert(res.exit_code == 0);
        assert(res.output.find("Exported solution visualization to SVG: /tmp/cli_zip.svg") != std::string::npos);
        assert(std::filesystem::exists("/tmp/cli_zip.svg"));
        std::cout << "[PASS] CLI zip solve with --export-svg\n";
    }

    // 58. Patches solve with --export-svg
    {
        auto res = run_cmd(bin + " patches solve fixtures/patches_valid_unique.txt --export-svg /tmp/cli_patches.svg");
        assert(res.exit_code == 0);
        assert(res.output.find("Exported solution visualization to SVG: /tmp/cli_patches.svg") != std::string::npos);
        assert(std::filesystem::exists("/tmp/cli_patches.svg"));
        std::cout << "[PASS] CLI patches solve with --export-svg\n";
    }

    // 59. Wend solve with --export-svg
    {
        auto res = run_cmd(bin + " wend solve fixtures/wend_valid_unique.txt --export-svg /tmp/cli_wend.svg");
        assert(res.exit_code == 0);
        assert(res.output.find("Exported solution visualization to SVG: /tmp/cli_wend.svg") != std::string::npos);
        assert(std::filesystem::exists("/tmp/cli_wend.svg"));
        std::cout << "[PASS] CLI wend solve with --export-svg\n";
    }

    // 60. Crossclimb solve with --export-svg
    {
        auto res = run_cmd(bin + " crossclimb solve fixtures/crossclimb_valid_unique.txt --export-svg /tmp/cli_crossclimb.svg");
        assert(res.exit_code == 0);
        assert(res.output.find("Exported solution visualization to SVG: /tmp/cli_crossclimb.svg") != std::string::npos);
        assert(std::filesystem::exists("/tmp/cli_crossclimb.svg"));
        std::cout << "[PASS] CLI crossclimb solve with --export-svg\n";
    }

    std::cout << "All 60 CLI Tests Passed!" << std::endl;
    return 0;
}
