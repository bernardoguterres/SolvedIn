#include "solvedin/crossclimb_board.hpp"
#include "solvedin/crossclimb_solver.hpp"
#include "solvedin/patches_board.hpp"
#include "solvedin/patches_solver.hpp"
#include "solvedin/sudoku_board.hpp"
#include "solvedin/sudoku_solver.hpp"
#include "solvedin/svg_exporter.hpp"
#include "solvedin/tango_board.hpp"
#include "solvedin/tango_solver.hpp"
#include "solvedin/trie.hpp"
#include "solvedin/wend_board.hpp"
#include "solvedin/wend_solver.hpp"
#include "solvedin/zip_board.hpp"
#include "solvedin/zip_solver.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

using namespace solvedin;

void save_test_file(const std::string& filepath, const std::string& content) {
    std::ofstream out(filepath);
    CHECK_ASSERT(out.is_open());
    out << content;
}

void assert_valid_svg_envelope(const std::string& svg, int expected_w, int expected_h) {
    CHECK_ASSERT(!svg.empty());
    CHECK_ASSERT(svg.find("<svg") != std::string::npos);
    CHECK_ASSERT(svg.find("</svg>") != std::string::npos);
    CHECK_ASSERT(svg.find("xmlns=\"http://www.w3.org/2000/svg\"") != std::string::npos);

    std::string w_attr = "width=\"" + std::to_string(expected_w) + "\"";
    std::string h_attr = "height=\"" + std::to_string(expected_h) + "\"";
    CHECK_ASSERT(svg.find(w_attr) != std::string::npos);
    CHECK_ASSERT(svg.find(h_attr) != std::string::npos);

    std::string vb_attr = "viewBox=\"0 0 " + std::to_string(expected_w) + " " + std::to_string(expected_h) + "\"";
    CHECK_ASSERT(svg.find(vb_attr) != std::string::npos);
}

void test_sudoku_svg_export() {
    std::cout << "Testing Sudoku SVG export..." << std::endl;

    auto [success, maybe_board, err] = SudokuBoard::load_from_file("fixtures/sudoku_valid_unique.txt");
    CHECK_ASSERT(success);
    CHECK_ASSERT(maybe_board.has_value());
    const auto& board = *maybe_board;

    auto result = solve_sudoku(board, true);
    CHECK_ASSERT(result.status == SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(!result.solutions.empty());

    const auto& sol = result.solutions[0];

    // Convert to std::vector<int> (36 elements)
    std::vector<int> sol_vec;
    sol_vec.reserve(36);
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            sol_vec.push_back(sol.grid[r][c]);
        }
    }

    // 1. Test free function with std::vector<int>
    int cell_size = 50;
    int expected_dim = 6 * cell_size + 24 * 2; // 348
    std::string svg1 = export_sudoku_svg(board, sol_vec, cell_size);
    save_test_file("/tmp/test_sudoku.svg", svg1);
    assert_valid_svg_envelope(svg1, expected_dim, expected_dim);
    CHECK_ASSERT(svg1.find("class=\"box-border\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"given-digit\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"solved-digit\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"solved-pill\"") != std::string::npos);

    // 2. Test overload with SudokuSolution struct
    std::string svg2 = export_sudoku_svg(board, sol, cell_size);
    CHECK_ASSERT(svg1 == svg2);

    // 3. Test SvgExporter static methods
    std::string svg3 = SvgExporter::export_sudoku_svg(board, sol_vec, cell_size);
    CHECK_ASSERT(svg1 == svg3);

    std::string svg4 = SvgExporter::export_sudoku_svg(board, sol, cell_size);
    CHECK_ASSERT(svg1 == svg4);

    std::cout << "  Sudoku SVG export passed (size: " << svg1.size() << " bytes)." << std::endl;
}

void test_tango_svg_export() {
    std::cout << "Testing Tango SVG export..." << std::endl;

    auto [success, maybe_board, err] = TangoBoard::load_from_file("fixtures/tango_valid_unique.txt");
    CHECK_ASSERT(success);
    CHECK_ASSERT(maybe_board.has_value());
    const auto& board = *maybe_board;

    auto result = solve_tango(board, true);
    CHECK_ASSERT(result.status == SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(!result.solutions.empty());

    const auto& sol = result.solutions[0];
    std::vector<char> sol_chars;
    for (const auto& row : sol.grid) {
        for (char ch : row) {
            sol_chars.push_back(ch);
        }
    }

    int cell_size = 60;
    int expected_dim = 6 * cell_size + 28 * 2; // 416
    std::string svg1 = export_tango_svg(board, sol_chars, cell_size);
    save_test_file("/tmp/test_tango.svg", svg1);
    assert_valid_svg_envelope(svg1, expected_dim, expected_dim);

    // Check Sun and Moon vector glyph elements
    CHECK_ASSERT(svg1.find("class=\"sun-core\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"sun-ray\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"moon-body\"") != std::string::npos);

    // Check edge clue indicators
    CHECK_ASSERT(svg1.find("class=\"badge-same\"") != std::string::npos);

    // Check overloads
    std::string svg2 = export_tango_svg(board, sol, cell_size);
    CHECK_ASSERT(svg1 == svg2);

    std::string svg3 = SvgExporter::export_tango_svg(board, sol_chars, cell_size);
    CHECK_ASSERT(svg1 == svg3);

    std::string svg4 = SvgExporter::export_tango_svg(board, sol, cell_size);
    CHECK_ASSERT(svg1 == svg4);

    std::cout << "  Tango SVG export passed (size: " << svg1.size() << " bytes)." << std::endl;
}

void test_zip_svg_export() {
    std::cout << "Testing Zip SVG export..." << std::endl;

    auto res = ZipBoard::load_from_file("fixtures/zip_valid_unique.txt");
    CHECK_ASSERT(res.success);
    CHECK_ASSERT(res.board.has_value());
    const auto& board = *res.board;

    auto solve_res = solve_zip(board, true);
    CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(!solve_res.solutions.empty());

    const auto& sol = solve_res.solutions[0];
    int cell_size = 60;
    int expected_w = board.cols() * cell_size + 28 * 2;
    int expected_h = board.rows() * cell_size + 28 * 2;

    std::string svg1 = export_zip_svg(board, sol.path, cell_size);
    save_test_file("/tmp/test_zip.svg", svg1);
    assert_valid_svg_envelope(svg1, expected_w, expected_h);

    // Check Checkpoints, Wall lines, Snake path and arrows
    CHECK_ASSERT(svg1.find("class=\"checkpoint-circle\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"checkpoint-text\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"wall-line\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"path-glow\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"path-main\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"arrow-head\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"step-badge\"") != std::string::npos);

    // Check overloads
    std::string svg2 = export_zip_svg(board, sol, cell_size);
    CHECK_ASSERT(svg1 == svg2);

    std::string svg3 = SvgExporter::export_zip_svg(board, sol.path, cell_size);
    CHECK_ASSERT(svg1 == svg3);

    std::string svg4 = SvgExporter::export_zip_svg(board, sol, cell_size);
    CHECK_ASSERT(svg1 == svg4);

    std::cout << "  Zip SVG export passed (size: " << svg1.size() << " bytes)." << std::endl;
}

void test_patches_svg_export() {
    std::cout << "Testing Patches SVG export..." << std::endl;

    auto res = PatchesBoard::load_from_file("fixtures/patches_valid_unique.txt");
    CHECK_ASSERT(res.success);
    CHECK_ASSERT(res.board.has_value());
    const auto& board = *res.board;

    auto solve_res = solve_patches(board, true);
    CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(!solve_res.solutions.empty());

    const auto& sol = solve_res.solutions[0];
    int cell_size = 60;
    int expected_w = board.cols() * cell_size + 28 * 2;
    int expected_h = board.rows() * cell_size + 28 * 2;

    std::string svg1 = export_patches_svg(board, sol, cell_size);
    save_test_file("/tmp/test_patches.svg", svg1);
    assert_valid_svg_envelope(svg1, expected_w, expected_h);

    // Check patch regions, borders, and clue badges
    CHECK_ASSERT(svg1.find("class=\"patch-border\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"clue-badge\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"clue-text\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("4S") != std::string::npos);

    // Check SvgExporter static method
    std::string svg2 = SvgExporter::export_patches_svg(board, sol, cell_size);
    CHECK_ASSERT(svg1 == svg2);

    std::cout << "  Patches SVG export passed (size: " << svg1.size() << " bytes)." << std::endl;
}

void test_wend_svg_export() {
    std::cout << "Testing Wend SVG export..." << std::endl;

    auto res = WendBoard::load_from_file("fixtures/wend_valid_unique.txt");
    CHECK_ASSERT(res.success);
    CHECK_ASSERT(res.board.has_value());
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    WendSolver solver(board, lexicon);
    auto solve_res = solver.solve(true);
    CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(!solve_res.solutions.empty());

    const auto& sol = solve_res.solutions[0];
    int cell_size = 60;

    std::string svg1 = export_wend_svg(board, sol, cell_size);
    save_test_file("/tmp/test_wend.svg", svg1);
    CHECK_ASSERT(!svg1.empty());
    CHECK_ASSERT(svg1.find("<svg") != std::string::npos);
    CHECK_ASSERT(svg1.find("</svg>") != std::string::npos);

    // Check letters, polyline paths, and legend
    CHECK_ASSERT(svg1.find("class=\"letter-tile\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"letter-text\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("<polyline points=\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("Words Legend") != std::string::npos);

    // Check SvgExporter static method
    std::string svg2 = SvgExporter::export_wend_svg(board, sol, cell_size);
    CHECK_ASSERT(svg1 == svg2);

    std::cout << "  Wend SVG export passed (size: " << svg1.size() << " bytes)." << std::endl;
}

void test_crossclimb_svg_export() {
    std::cout << "Testing Crossclimb SVG export..." << std::endl;

    auto res = CrossclimbBoard::load_from_file("fixtures/crossclimb_valid_unique.txt");
    CHECK_ASSERT(res.success);
    CHECK_ASSERT(res.board.has_value());
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    CrossclimbSolver solver(board, lexicon);
    auto solve_res = solver.solve();
    CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(!solve_res.solutions.empty());

    const auto& sol = solve_res.solutions[0];
    int rung_w = 240;
    int rung_h = 45;

    std::string svg1 = export_crossclimb_svg(board, sol, rung_w, rung_h);
    save_test_file("/tmp/test_crossclimb.svg", svg1);
    CHECK_ASSERT(!svg1.empty());
    CHECK_ASSERT(svg1.find("<svg") != std::string::npos);
    CHECK_ASSERT(svg1.find("</svg>") != std::string::npos);

    // Check ladder side rails, rungs, letter boxes, and transition line/arrow
    CHECK_ASSERT(svg1.find("class=\"ladder-rail\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"rung-card\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"letter-box\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"transition-line\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("class=\"transition-arrow\"") != std::string::npos);
    CHECK_ASSERT(svg1.find("#FEF08A") != std::string::npos); // highlight yellow fill

    // Check SvgExporter static method
    std::string svg2 = SvgExporter::export_crossclimb_svg(board, sol, rung_w, rung_h);
    CHECK_ASSERT(svg1 == svg2);

    std::cout << "  Crossclimb SVG export passed (size: " << svg1.size() << " bytes)." << std::endl;
}

void test_queens_svg_export() {
    std::cout << "Testing Queens SVG export..." << std::endl;

    auto parse_res = Board::parse_from_string("3\n0 0 1\n0 1 1\n2 2 2\n");
    CHECK_ASSERT(parse_res.success);
    CHECK_ASSERT(parse_res.board.has_value());
    const auto& board = *parse_res.board;

    std::vector<Position> queens = {{0, 0}, {1, 2}, {2, 1}};
    std::string svg = SvgExporter::generate_svg_string(board, queens, 30);
    save_test_file("/tmp/test_queens.svg", svg);

    assert_valid_svg_envelope(svg, 3 * 30 + 24 * 2, 3 * 30 + 24 * 2);
    CHECK_ASSERT(svg.find("class=\"crown-fill\"") != std::string::npos);
    CHECK_ASSERT(svg.find("class=\"crown-jewel\"") != std::string::npos);

    std::cout << "  Queens SVG export passed." << std::endl;
}

int main() {
    std::cout << "=== Running SVG Exporter Tests ===" << std::endl;

    test_sudoku_svg_export();
    test_tango_svg_export();
    test_zip_svg_export();
    test_patches_svg_export();
    test_wend_svg_export();
    test_crossclimb_svg_export();
    test_queens_svg_export();

    std::cout << "=== All SVG Exporter Tests Passed Successfully! ===" << std::endl;
    return 0;
}
