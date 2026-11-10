#include "solvedin/board.hpp"
#include "solvedin/queens_generator.hpp"
#include "solvedin/queens_solver.hpp"
#include "solvedin/queens_validator.hpp"
#include "solvedin/svg_exporter.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <sys/resource.h>

namespace {

double get_peak_rss_mb() {
    struct rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__) && defined(__MACH__)
        // ru_maxrss is in bytes on macOS
        return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
        // ru_maxrss is in kilobytes on Linux
        return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
    }
    return 0.0;
}

struct EvalResult {
    int size{0};
    uint64_t seed{0};
    std::string phase;
    std::string variant;
    std::string status_str;
    size_t nodes{0};
    double elapsed_ms{0.0};
    double peak_rss_mb{0.0};
    bool solved{false};
    bool unique_proven{false};
};

EvalResult run_case(int size, uint64_t seed, bool check_unique,
                    solvedin::SolverVariant variant,
                    size_t max_nodes, double max_time_ms,
                    std::vector<solvedin::Position>& out_solution) {
    auto gen = solvedin::QueensGenerator::generate(size, seed);
    const auto& board = gen.board;

    solvedin::SolveOptions opts;
    opts.max_nodes = max_nodes;
    opts.max_time_ms = max_time_ms;

    auto start = std::chrono::steady_clock::now();
    auto result = solvedin::solve_queens(board, check_unique, variant, opts);
    auto end = std::chrono::steady_clock::now();

    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double peak_rss = get_peak_rss_mb();

    EvalResult er;
    er.size = size;
    er.seed = seed;
    er.phase = check_unique ? "Uniqueness Proof" : "First Solution";
    er.variant = solvedin::to_string(variant);
    er.nodes = result.stats.nodes_visited;
    er.elapsed_ms = elapsed_ms;
    er.peak_rss_mb = peak_rss;

    if (result.status == solvedin::SolveStatus::INCONCLUSIVE) {
        er.status_str = "INCONCLUSIVE (budget exhausted)";
        if (!result.solutions.empty()) {
            er.solved = true;
            out_solution = result.solutions[0].queens;
        }
    } else if (result.status == solvedin::SolveStatus::UNIQUE_SOLUTION) {
        er.status_str = check_unique ? "UNIQUE (proven)" : "FOUND (1st solution)";
        er.solved = true;
        if (check_unique) er.unique_proven = true;
        if (!result.solutions.empty()) {
            out_solution = result.solutions[0].queens;
            auto val = solvedin::validate_queens_solution(board, out_solution);
            if (!val.is_valid) {
                std::cerr << "VALIDATION ERROR: " << val.error_message << "\n";
            }
        }
    } else if (result.status == solvedin::SolveStatus::MULTIPLE_SOLUTIONS) {
        er.status_str = "MULTIPLE_SOLUTIONS";
        er.solved = true;
        if (!result.solutions.empty()) {
            out_solution = result.solutions[0].queens;
            auto val = solvedin::validate_queens_solution(board, out_solution);
            if (!val.is_valid) {
                std::cerr << "VALIDATION ERROR: " << val.error_message << "\n";
            }
        }
    } else {
        er.status_str = "NO_SOLUTION";
    }

    return er;
}

} // namespace

int main() {
    std::cout << "========================================================================\n";
    std::cout << "                     GIANT QUEENS EVALUATION SUITE                      \n";
    std::cout << "========================================================================\n\n";

    const size_t max_nodes = 200000;
    const double max_time_ms = 3000.0;

    std::cout << "Budget limits: max_nodes = " << max_nodes
              << ", max_time = " << std::fixed << std::setprecision(1) << max_time_ms << " ms\n\n";

    // 1. Smaller boards that force backtracking and Giant boards
    struct TestCase {
        int size;
        uint64_t seed;
        std::string desc;
    };

    std::vector<TestCase> test_cases = {
        // Backtracking-heavy smaller boards
        {8, 42, "Backtracking (8x8)"},
        {12, 101, "Backtracking (12x12)"},
        {16, 2024, "Backtracking (16x16)"},
        {20, 777, "Backtracking (20x20)"},
        {24, 999, "Backtracking (24x24)"},

        // Giant boards
        {32, 1, "Giant 32x32 (seed 1)"},
        {32, 7, "Giant 32x32 (seed 7)"},
        {32, 42, "Giant 32x32 (seed 42)"},
        {64, 1, "Giant 64x64 (seed 1)"},
        {64, 7, "Giant 64x64 (seed 7)"},
        {64, 42, "Giant 64x64 (seed 42)"},
        {128, 1, "Giant 128x128 (seed 1)"},
        {128, 7, "Giant 128x128 (seed 7)"},
        {128, 42, "Giant 128x128 (seed 42)"}
    };

    std::cout << "### Benchmark Results\n\n";
    std::cout << "| Size | Seed | Description | Phase | Variant | Status | Nodes Visited | Time (ms) | Peak RSS (MB) |\n";
    std::cout << "|:---:|:---:|:---|:---|:---:|:---|:---:|:---:|:---:|\n";

    int largest_solved_size = 0;
    uint64_t largest_solved_seed = 0;
    solvedin::Board largest_solved_board(1, {0});
    std::vector<solvedin::Position> largest_solved_queens;
    std::string largest_uniqueness_status;
    double largest_solved_time = 0.0;
    size_t largest_solved_nodes = 0;
    double largest_solved_rss = 0.0;
    std::string largest_solved_variant;

    for (const auto& tc : test_cases) {
        // Phase 1: First solution
        std::vector<solvedin::Position> sol1;
        auto res1 = run_case(tc.size, tc.seed, false, solvedin::SolverVariant::BITSET,
                             max_nodes, max_time_ms, sol1);

        std::cout << "| " << res1.size << "x" << res1.size
                  << " | " << res1.seed
                  << " | " << tc.desc
                  << " | " << res1.phase
                  << " | `" << res1.variant << "`"
                  << " | " << res1.status_str
                  << " | " << res1.nodes
                  << " | " << std::fixed << std::setprecision(2) << res1.elapsed_ms
                  << " | " << std::fixed << std::setprecision(2) << res1.peak_rss_mb
                  << " |\n";

        // Phase 2: Uniqueness proof
        std::vector<solvedin::Position> sol2;
        auto res2 = run_case(tc.size, tc.seed, true, solvedin::SolverVariant::BITSET,
                             max_nodes, max_time_ms, sol2);

        std::cout << "| " << res2.size << "x" << res2.size
                  << " | " << res2.seed
                  << " | " << tc.desc
                  << " | " << res2.phase
                  << " | `" << res2.variant << "`"
                  << " | " << res2.status_str
                  << " | " << res2.nodes
                  << " | " << std::fixed << std::setprecision(2) << res2.elapsed_ms
                  << " | " << std::fixed << std::setprecision(2) << res2.peak_rss_mb
                  << " |\n";

        if (res1.solved && tc.size >= largest_solved_size) {
            largest_solved_size = tc.size;
            largest_solved_seed = tc.seed;
            auto gen = solvedin::QueensGenerator::generate(tc.size, tc.seed);
            largest_solved_board = std::move(gen.board);
            largest_solved_queens = sol1;
            largest_uniqueness_status = res2.status_str;
            largest_solved_time = res1.elapsed_ms;
            largest_solved_nodes = res1.nodes;
            largest_solved_rss = res1.peak_rss_mb;
            largest_solved_variant = res1.variant;
        }
    }

    // Variant Comparison on Backtracking Board (20x20, seed 777)
    std::cout << "\n### Variant Comparison on Backtracking Board (20x20, Seed 777)\n\n";
    std::cout << "| Variant | Phase | Status | Nodes Visited | Time (ms) | Speedup vs Base |\n";
    std::cout << "|:---|:---|:---|:---:|:---:|:---:|\n";

    std::vector<solvedin::Position> dummy_sol;
    auto b_res = run_case(20, 777, false, solvedin::SolverVariant::BASELINE, max_nodes, max_time_ms, dummy_sol);
    auto m_res = run_case(20, 777, false, solvedin::SolverVariant::MRV, max_nodes, max_time_ms, dummy_sol);
    auto bit_res = run_case(20, 777, false, solvedin::SolverVariant::BITSET, max_nodes, max_time_ms, dummy_sol);

    double base_time = b_res.elapsed_ms > 0.0 ? b_res.elapsed_ms : 1.0;
    auto print_v_row = [&](const EvalResult& er) {
        double sp = base_time / (er.elapsed_ms > 0.0 ? er.elapsed_ms : 0.001);
        std::cout << "| `" << er.variant << "` | " << er.phase << " | " << er.status_str
                  << " | " << er.nodes << " | " << std::fixed << std::setprecision(2) << er.elapsed_ms
                  << " | " << std::fixed << std::setprecision(1) << sp << "x |\n";
    };
    print_v_row(b_res);
    print_v_row(m_res);
    print_v_row(bit_res);

    std::cout << "\n========================================================================\n";
    std::cout << "                         BENCHMARK SUMMARY                              \n";
    std::cout << "========================================================================\n";
    std::cout << "Largest Board Solved : " << largest_solved_size << "x" << largest_solved_size << "\n";
    std::cout << "Seed                 : " << largest_solved_seed << "\n";
    std::cout << "Solver Variant       : " << largest_solved_variant << "\n";
    std::cout << "First Solution Time  : " << std::fixed << std::setprecision(2) << largest_solved_time << " ms\n";
    std::cout << "Nodes Visited        : " << largest_solved_nodes << "\n";
    std::cout << "Peak Memory          : " << std::fixed << std::setprecision(2) << largest_solved_rss << " MB\n";
    std::cout << "Uniqueness Status    : " << largest_uniqueness_status << "\n";

    std::string unique_proven_str;
    if (largest_uniqueness_status == "UNIQUE (proven)") {
        unique_proven_str = "YES (Single unique solution)";
    } else if (largest_uniqueness_status == "MULTIPLE_SOLUTIONS") {
        unique_proven_str = "NO (Multiple valid solutions exist - uniqueness disproven)";
    } else {
        unique_proven_str = "INCONCLUSIVE (Search budget exhausted before proving uniqueness)";
    }
    std::cout << "Uniqueness Proven    : " << unique_proven_str << "\n";

    if (largest_solved_size > 0 && !largest_solved_queens.empty()) {
        std::string svg_filename = "giant_queens_" + std::to_string(largest_solved_size) + "x" +
                                   std::to_string(largest_solved_size) + ".svg";
        int cell_px = (largest_solved_size <= 32) ? 24 : 16;
        if (solvedin::SvgExporter::export_to_file(largest_solved_board, largest_solved_queens, svg_filename, cell_px)) {
            std::cout << "Exported SVG visualization: " << svg_filename << "\n";
        }
    }

    std::cout << "\n========================================================================\n";
    std::cout << "                      REPRODUCTION COMMANDS                             \n";
    std::cout << "========================================================================\n";
    std::cout << "# 1. Generate the largest solved board (" << largest_solved_size << "x" << largest_solved_size << ", seed " << largest_solved_seed << "):\n";
    std::cout << "./build_release/solvedin queens generate --size " << largest_solved_size
              << " --seed " << largest_solved_seed << " --output board_" << largest_solved_size << "x"
              << largest_solved_size << ".txt\n\n";
    std::cout << "# 2. Solve and export SVG visualization:\n";
    std::cout << "./build_release/solvedin queens solve board_" << largest_solved_size << "x"
              << largest_solved_size << ".txt --solver bitset --export-svg giant_queens_" << largest_solved_size
              << "x" << largest_solved_size << ".svg --stats\n\n";
    std::cout << "# 3. Run full Giant Queens demo:\n";
    std::cout << "./build_release/giant-queens\n";

    return 0;
}
