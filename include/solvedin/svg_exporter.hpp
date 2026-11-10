#pragma once

#include "solvedin/board.hpp"
#include "solvedin/crossclimb_board.hpp"
#include "solvedin/patches_board.hpp"
#include "solvedin/patches_validator.hpp"
#include "solvedin/sudoku_board.hpp"
#include "solvedin/sudoku_solver.hpp"
#include "solvedin/tango_board.hpp"
#include "solvedin/tango_solver.hpp"
#include "solvedin/wend_board.hpp"
#include "solvedin/zip_board.hpp"
#include "solvedin/zip_solver.hpp"

#include <string>
#include <vector>

namespace solvedin {

/// Mini Sudoku SVG Exporter:
/// 6x6 grid with thick 2x3 box borders, given numbers in distinct bold dark style,
/// and solved numbers in distinct styled accent color and circular pill.
std::string export_sudoku_svg(const SudokuBoard& board,
                              const std::vector<int>& solution,
                              int cell_size = 50);

std::string export_sudoku_svg(const SudokuBoard& board,
                              const SudokuSolution& solution,
                              int cell_size = 50);

/// Tango SVG Exporter:
/// 6x6 grid, rendered with crisp vector Sun and Moon glyphs,
/// and distinct edge clue indicators ('=' and 'x') between cells.
std::string export_tango_svg(const TangoBoard& board,
                             const std::vector<char>& solution,
                             int cell_size = 60);

std::string export_tango_svg(const TangoBoard& board,
                             const TangoSolution& solution,
                             int cell_size = 60);

/// Zip SVG Exporter:
/// Numbered checkpoint circles, wall lines, and continuous directed snake path
/// with directional arrows and step numbers.
std::string export_zip_svg(const ZipBoard& board,
                           const std::vector<Position>& path,
                           int cell_size = 60);

std::string export_zip_svg(const ZipBoard& board,
                           const ZipSolution& solution,
                           int cell_size = 60);

/// Patches SVG Exporter:
/// Distinct colored regions for each patch, patch borders, and clue labels/badges
/// in their respective clue cells.
std::string export_patches_svg(const PatchesBoard& board,
                               const PatchesSolution& solution,
                               int cell_size = 60);

/// Wend SVG Exporter:
/// Letter grid with colored snake paths showing each word cover.
std::string export_wend_svg(const WendBoard& board,
                            const WendSolution& solution,
                            int cell_size = 60);

/// Crossclimb SVG Exporter:
/// Word ladder vertical graphic with highlighted 1-letter transitions.
std::string export_crossclimb_svg(const CrossclimbBoard& board,
                                  const CrossclimbSolution& solution,
                                  int rung_width = 240,
                                  int rung_height = 45);

/// Comprehensive SvgExporter class encapsulating all puzzle SVG generators.
class SvgExporter {
public:
    // Existing Queens SVG methods
    static bool export_to_file(const Board& board,
                               const std::vector<Position>& queens,
                               const std::string& filepath,
                               int cell_size = 28);

    static std::string generate_svg_string(const Board& board,
                                           const std::vector<Position>& queens,
                                           int cell_size = 28);

    // Mini Sudoku
    static std::string export_sudoku_svg(const SudokuBoard& board,
                                         const std::vector<int>& solution,
                                         int cell_size = 50);
    static std::string export_sudoku_svg(const SudokuBoard& board,
                                         const SudokuSolution& solution,
                                         int cell_size = 50);

    // Tango
    static std::string export_tango_svg(const TangoBoard& board,
                                        const std::vector<char>& solution,
                                        int cell_size = 60);
    static std::string export_tango_svg(const TangoBoard& board,
                                        const TangoSolution& solution,
                                        int cell_size = 60);

    // Zip
    static std::string export_zip_svg(const ZipBoard& board,
                                      const std::vector<Position>& path,
                                      int cell_size = 60);
    static std::string export_zip_svg(const ZipBoard& board,
                                      const ZipSolution& solution,
                                      int cell_size = 60);

    // Patches
    static std::string export_patches_svg(const PatchesBoard& board,
                                          const PatchesSolution& solution,
                                          int cell_size = 60);

    // Wend
    static std::string export_wend_svg(const WendBoard& board,
                                       const WendSolution& solution,
                                       int cell_size = 60);

    // Crossclimb
    static std::string export_crossclimb_svg(const CrossclimbBoard& board,
                                             const CrossclimbSolution& solution,
                                             int rung_width = 240,
                                             int rung_height = 45);
};

} // namespace solvedin
