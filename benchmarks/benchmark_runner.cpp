#include "solvedin/board.hpp"
#include "solvedin/queens_solver.hpp"
#include "solvedin/queens_validator.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

struct BenchmarkCase {
    std::string name;
    std::string path;
    bool check_unique{false};
};

struct VariantTiming {
    std::string variant_name;
    size_t nodes{0};
    double median_us{0.0};
    double ns_per_node{0.0};
};

VariantTiming benchmark_variant(const solvedin::Board& board,
                                bool check_unique,
                                solvedin::SolverVariant variant,
                                const std::string& name,
                                int warmup_reps = 3,
                                int bench_reps = 20) {
    // Warmup
    for (int i = 0; i < warmup_reps; ++i) {
        auto res = solvedin::solve_queens(board, check_unique, variant);
        (void)res;
    }

    std::vector<double> timings_us;
    timings_us.reserve(bench_reps);
    size_t nodes = 0;

    for (int i = 0; i < bench_reps; ++i) {
        auto start = std::chrono::steady_clock::now();
        auto res = solvedin::solve_queens(board, check_unique, variant);
        auto end = std::chrono::steady_clock::now();

        double elapsed_us = std::chrono::duration<double, std::micro>(end - start).count();
        timings_us.push_back(elapsed_us);
        nodes = res.stats.nodes_visited;

        // Independent validation on first iteration
        if (i == 0) {
            for (const auto& sol : res.solutions) {
                auto val = solvedin::validate_queens_solution(board, sol.queens);
                if (!val.is_valid) {
                    std::cerr << "VALIDATION FAILURE in " << name << " variant "
                              << solvedin::to_string(variant) << ": " << val.error_message << "\n";
                }
            }
        }
    }

    std::sort(timings_us.begin(), timings_us.end());
    double median_us = timings_us[timings_us.size() / 2];
    double ns_per_node = (nodes > 0) ? (median_us * 1000.0 / static_cast<double>(nodes)) : 0.0;

    return {std::string(solvedin::to_string(variant)), nodes, median_us, ns_per_node};
}

int main(int argc, char* argv[]) {
    std::vector<BenchmarkCase> cases = {
        {"4x4 Multiple (check-unique)", "fixtures/valid_4x4_multiple.txt", true},
        {"4x4 No Solution (check-unique)", "fixtures/valid_no_solution.txt", true},
        {"6x6 Long Diagonal (solve)", "fixtures/valid_long_diagonal.txt", false},
        {"6x6 Long Diagonal (check-unique)", "fixtures/valid_long_diagonal.txt", true},
        {"8x8 Unique (solve)", "fixtures/valid_8x8_unique.txt", false},
        {"8x8 Unique (check-unique)", "fixtures/valid_8x8_unique.txt", true}
    };

    if (argc > 1) {
        cases.clear();
        for (int i = 1; i < argc; ++i) {
            cases.push_back({argv[i], argv[i], false});
            cases.push_back({std::string(argv[i]) + " (unique)", argv[i], true});
        }
    }

    std::cout << "# LinkedIn Queens Solver Benchmark Results (Release Build)\n\n";
    std::cout << "| Benchmark Case | Variant | Nodes | Median Time (µs) | Cost/Node (ns/node) | Node Reduction vs Base | Speedup vs Base |\n";
    std::cout << "|:---|:---|:---:|:---:|:---:|:---:|:---:|\n";

    for (const auto& bc : cases) {
        auto [success, maybe_board, err] = solvedin::Board::load_from_file(bc.path);
        if (!success || !maybe_board) {
            std::cerr << "Could not load " << bc.path << ": " << err << "\n";
            continue;
        }

        const auto& board = *maybe_board;
        auto t_base = benchmark_variant(board, bc.check_unique, solvedin::SolverVariant::BASELINE, bc.name);
        auto t_mrv  = benchmark_variant(board, bc.check_unique, solvedin::SolverVariant::MRV, bc.name);
        auto t_bit  = benchmark_variant(board, bc.check_unique, solvedin::SolverVariant::BITSET, bc.name);

        auto print_row = [&](const VariantTiming& vt, double node_red, double speedup) {
            std::cout << "| " << bc.name << " | `" << vt.variant_name << "` | "
                      << vt.nodes << " | "
                      << std::fixed << std::setprecision(1) << vt.median_us << " | "
                      << std::fixed << std::setprecision(1) << vt.ns_per_node << " | "
                      << std::fixed << std::setprecision(2) << node_red << "x | "
                      << std::fixed << std::setprecision(2) << speedup << "x |\n";
        };

        double base_time = t_base.median_us;
        double base_nodes = static_cast<double>(t_base.nodes);

        print_row(t_base, 1.0, 1.0);
        print_row(t_mrv, (t_mrv.nodes > 0) ? (base_nodes / t_mrv.nodes) : 1.0,
                         (t_mrv.median_us > 0.0) ? (base_time / t_mrv.median_us) : 1.0);
        print_row(t_bit, (t_bit.nodes > 0) ? (base_nodes / t_bit.nodes) : 1.0,
                         (t_bit.median_us > 0.0) ? (base_time / t_bit.median_us) : 1.0);
    }

    return 0;
}
