#include "solvedin/svg_exporter.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace solvedin {

namespace {

constexpr double PI = 3.14159265358979323846;

// Converts HSL to hex RGB string
std::string hsl_to_hex(double h, double s, double l) {
    double c = (1.0 - std::abs(2.0 * l - 1.0)) * s;
    double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
    double m = l - c / 2.0;

    double r = 0, g = 0, b = 0;
    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }

    int ir = static_cast<int>(std::round((r + m) * 255.0));
    int ig = static_cast<int>(std::round((g + m) * 255.0));
    int ib = static_cast<int>(std::round((b + m) * 255.0));

    std::ostringstream oss;
    oss << "#"
        << std::hex << std::setfill('0') << std::setw(2) << ir
        << std::setfill('0') << std::setw(2) << ig
        << std::setfill('0') << std::setw(2) << ib;
    return oss.str();
}

std::string get_region_color(int reg_id, int total_regions) {
    (void)total_regions;
    // Golden angle distribution produces visually distinct adjacent hues
    double hue = std::fmod(reg_id * 137.50776405003785, 360.0);
    return hsl_to_hex(hue, 0.65, 0.82);
}

std::string xml_escape(const std::string& str) {
    std::ostringstream oss;
    for (char ch : str) {
        switch (ch) {
        case '&':  oss << "&amp;"; break;
        case '<':  oss << "&lt;"; break;
        case '>':  oss << "&gt;"; break;
        case '"':  oss << "&quot;"; break;
        case '\'': oss << "&apos;"; break;
        default:   oss << ch; break;
        }
    }
    return oss.str();
}

} // namespace

// ============================================================================
// Queens SVG Exporter (Original Implementation)
// ============================================================================

std::string SvgExporter::generate_svg_string(const Board& board,
                                             const std::vector<Position>& queens,
                                             int cell_size) {
    const int n = board.size();
    const int padding = 24;
    const int board_width = n * cell_size;
    const int total_width = board_width + padding * 2;
    const int total_height = board_width + padding * 2;

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "viewBox=\"0 0 " << total_width << " " << total_height << "\" "
        << "width=\"" << total_width << "\" height=\"" << total_height << "\">\n"
        << "<style>\n"
        << "  .cell { stroke: none; }\n"
        << "  .grid-fine { stroke: #000000; stroke-opacity: 0.12; stroke-width: 1; }\n"
        << "  .region-border { stroke: #0F172A; stroke-width: 3.0; stroke-linecap: round; }\n"
        << "  .board-outer { stroke: #0F172A; stroke-width: 4.0; fill: none; }\n"
        << "  .crown-badge { fill: #FFFFFF; stroke: #0F172A; stroke-width: 1.5; }\n"
        << "  .crown-fill { fill: #EAB308; stroke: #854D0E; stroke-width: 1.2; stroke-linejoin: round; }\n"
        << "  .crown-jewel { fill: #DC2626; }\n"
        << "</style>\n\n"
        << "<!-- Background -->\n"
        << "<rect width=\"" << total_width << "\" height=\"" << total_height << "\" fill=\"#F8FAFC\"/>\n\n"
        << "<g transform=\"translate(" << padding << ", " << padding << ")\">\n";

    // 1. Draw cell background rectangles colored by region
    svg << "<!-- Region Cells -->\n";
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            int reg = board.get_region(r, c);
            std::string color = get_region_color(reg, n);
            svg << "<rect class=\"cell\" x=\"" << (c * cell_size) << "\" y=\"" << (r * cell_size)
                << "\" width=\"" << cell_size << "\" height=\"" << cell_size
                << "\" fill=\"" << color << "\"/>\n";
        }
    }

    // 2. Draw subtle internal grid lines
    svg << "\n<!-- Fine Grid Lines -->\n";
    for (int r = 0; r <= n; ++r) {
        svg << "<line class=\"grid-fine\" x1=\"0\" y1=\"" << (r * cell_size)
            << "\" x2=\"" << board_width << "\" y2=\"" << (r * cell_size) << "\"/>\n";
    }
    for (int c = 0; c <= n; ++c) {
        svg << "<line class=\"grid-fine\" x1=\"" << (c * cell_size) << "\" y1=\"0\""
            << " x2=\"" << (c * cell_size) << "\" y2=\"" << board_width << "\"/>\n";
    }

    // 3. Draw thick visible region boundary lines
    svg << "\n<!-- Thick Region Boundaries -->\n";
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            int curr_reg = board.get_region(r, c);

            if (r > 0 && board.get_region(r - 1, c) != curr_reg) {
                svg << "<line class=\"region-border\" x1=\"" << (c * cell_size)
                    << "\" y1=\"" << (r * cell_size)
                    << "\" x2=\"" << ((c + 1) * cell_size)
                    << "\" y2=\"" << (r * cell_size) << "\"/>\n";
            }

            if (c > 0 && board.get_region(r, c - 1) != curr_reg) {
                svg << "<line class=\"region-border\" x1=\"" << (c * cell_size)
                    << "\" y1=\"" << (r * cell_size)
                    << "\" x2=\"" << (c * cell_size)
                    << "\" y2=\"" << ((r + 1) * cell_size) << "\"/>\n";
            }
        }
    }

    // 4. Outer border
    svg << "<rect class=\"board-outer\" x=\"0\" y=\"0\" width=\""
        << board_width << "\" height=\"" << board_width << "\"/>\n";

    // 5. Draw Queen markers
    svg << "\n<!-- Queen Markers -->\n";
    for (const auto& q : queens) {
        double cx = q.col * cell_size + cell_size / 2.0;
        double cy = q.row * cell_size + cell_size / 2.0;
        double badge_r = cell_size * 0.40;

        svg << "<circle class=\"crown-badge\" cx=\"" << cx << "\" cy=\"" << cy
            << "\" r=\"" << badge_r << "\"/>\n";

        double cw = cell_size * 0.26;
        double ch = cell_size * 0.22;
        double top_y = cy - ch * 0.7;
        double bot_y = cy + ch * 0.75;
        double mid_valley_y = cy + ch * 0.15;
        double center_peak_y = cy - ch * 0.95;

        svg << "<polygon class=\"crown-fill\" points=\""
            << (cx - cw) << "," << bot_y << " "
            << (cx + cw) << "," << bot_y << " "
            << (cx + cw * 0.9) << "," << top_y << " "
            << (cx + cw * 0.45) << "," << mid_valley_y << " "
            << cx << "," << center_peak_y << " "
            << (cx - cw * 0.45) << "," << mid_valley_y << " "
            << (cx - cw * 0.9) << "," << top_y << "\"/>\n";

        double jr = std::max(1.0, cell_size * 0.04);
        svg << "<circle class=\"crown-jewel\" cx=\"" << (cx - cw * 0.9) << "\" cy=\"" << top_y << "\" r=\"" << jr << "\"/>\n";
        svg << "<circle class=\"crown-jewel\" cx=\"" << cx << "\" cy=\"" << center_peak_y << "\" r=\"" << jr << "\"/>\n";
        svg << "<circle class=\"crown-jewel\" cx=\"" << (cx + cw * 0.9) << "\" cy=\"" << top_y << "\" r=\"" << jr << "\"/>\n";
    }

    svg << "</g>\n</svg>\n";
    return svg.str();
}

bool SvgExporter::export_to_file(const Board& board,
                                 const std::vector<Position>& queens,
                                 const std::string& filepath,
                                 int cell_size) {
    std::ofstream out(filepath);
    if (!out.is_open()) {
        return false;
    }
    out << generate_svg_string(board, queens, cell_size);
    return true;
}

// ============================================================================
// 1. Mini Sudoku SVG Exporter
// ============================================================================

std::string export_sudoku_svg(const SudokuBoard& board,
                              const std::vector<int>& solution,
                              int cell_size) {
    constexpr int N = SudokuBoard::SIZE; // 6
    constexpr int BOX_R = SudokuBoard::BOX_ROWS; // 2
    constexpr int BOX_C = SudokuBoard::BOX_COLS; // 3
    const int padding = 24;
    const int board_size = N * cell_size;
    const int total_width = board_size + padding * 2;
    const int total_height = board_size + padding * 2;

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "viewBox=\"0 0 " << total_width << " " << total_height << "\" "
        << "width=\"" << total_width << "\" height=\"" << total_height << "\">\n"
        << "<style>\n"
        << "  text { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }\n"
        << "  .sudoku-bg { fill: #F8FAFC; rx: 12px; }\n"
        << "  .grid-fine { stroke: #CBD5E1; stroke-width: 1; }\n"
        << "  .box-border { stroke: #0F172A; stroke-width: 3.0; stroke-linecap: round; }\n"
        << "  .board-outer { stroke: #0F172A; stroke-width: 3.5; fill: none; }\n"
        << "  .given-digit { fill: #0F172A; font-weight: 700; }\n"
        << "  .solved-pill { fill: #EFF6FF; stroke: #93C5FD; stroke-width: 1.2; }\n"
        << "  .solved-digit { fill: #2563EB; font-weight: 600; }\n"
        << "</style>\n\n"
        << "<rect width=\"" << total_width << "\" height=\"" << total_height << "\" class=\"sudoku-bg\"/>\n"
        << "<g transform=\"translate(" << padding << ", " << padding << ")\">\n";

    // 1. Alternating box background fill
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int bidx = (r / BOX_R) + (c / BOX_C);
            std::string fill = (bidx % 2 == 0) ? "#FFFFFF" : "#F8FAFC";
            svg << "<rect x=\"" << (c * cell_size) << "\" y=\"" << (r * cell_size)
                << "\" width=\"" << cell_size << "\" height=\"" << cell_size
                << "\" fill=\"" << fill << "\"/>\n";
        }
    }

    // 2. Fine cell grid lines
    for (int i = 0; i <= N; ++i) {
        svg << "<line class=\"grid-fine\" x1=\"0\" y1=\"" << (i * cell_size)
            << "\" x2=\"" << board_size << "\" y2=\"" << (i * cell_size) << "\"/>\n";
        svg << "<line class=\"grid-fine\" x1=\"" << (i * cell_size) << "\" y1=\"0\""
            << " x2=\"" << (i * cell_size) << "\" y2=\"" << board_size << "\"/>\n";
    }

    // 3. Thick 2x3 box divider lines
    for (int r = BOX_R; r < N; r += BOX_R) {
        svg << "<line class=\"box-border\" x1=\"0\" y1=\"" << (r * cell_size)
            << "\" x2=\"" << board_size << "\" y2=\"" << (r * cell_size) << "\"/>\n";
    }
    for (int c = BOX_C; c < N; c += BOX_C) {
        svg << "<line class=\"box-border\" x1=\"" << (c * cell_size) << "\" y1=\"0\""
            << " x2=\"" << (c * cell_size) << "\" y2=\"" << board_size << "\"/>\n";
    }

    // 4. Outer border
    svg << "<rect class=\"board-outer\" x=\"0\" y=\"0\" width=\""
        << board_size << "\" height=\"" << board_size << "\"/>\n";

    // 5. Digits: givens in bold dark color, solved in distinct accent style
    const double given_font_size = cell_size * 0.48;
    const double solved_font_size = cell_size * 0.44;

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int val = 0;
            if (solution.size() == static_cast<size_t>(N * N)) {
                val = solution[r * N + c];
            } else {
                val = board.get(r, c);
            }

            if (val <= 0) {
                continue;
            }

            double cx = (c + 0.5) * cell_size;
            double cy = (r + 0.5) * cell_size;
            bool is_given = board.is_given(r, c) || (board.get(r, c) == val && board.get(r, c) != 0);

            if (is_given) {
                svg << "<text class=\"given-digit\" x=\"" << cx << "\" y=\"" << cy
                    << "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                    << "font-size=\"" << given_font_size << "px\">" << val << "</text>\n";
            } else {
                // Solved number with distinct circular pill and distinct color
                double pill_r = cell_size * 0.35;
                svg << "<circle class=\"solved-pill\" cx=\"" << cx << "\" cy=\"" << cy
                    << "\" r=\"" << pill_r << "\"/>\n";
                svg << "<text class=\"solved-digit\" x=\"" << cx << "\" y=\"" << cy
                    << "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                    << "font-size=\"" << solved_font_size << "px\">" << val << "</text>\n";
            }
        }
    }

    svg << "</g>\n</svg>\n";
    return svg.str();
}

std::string export_sudoku_svg(const SudokuBoard& board,
                              const SudokuSolution& solution,
                              int cell_size) {
    std::vector<int> sol_vec;
    sol_vec.reserve(SudokuBoard::SIZE * SudokuBoard::SIZE);
    for (int r = 0; r < SudokuBoard::SIZE; ++r) {
        for (int c = 0; c < SudokuBoard::SIZE; ++c) {
            sol_vec.push_back(solution.grid[r][c]);
        }
    }
    return export_sudoku_svg(board, sol_vec, cell_size);
}

// ============================================================================
// 2. Tango SVG Exporter
// ============================================================================

std::string export_tango_svg(const TangoBoard& board,
                             const std::vector<char>& solution,
                             int cell_size) {
    const int n = board.size();
    const int padding = 28;
    const int board_size = n * cell_size;
    const int total_width = board_size + padding * 2;
    const int total_height = board_size + padding * 2;

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "viewBox=\"0 0 " << total_width << " " << total_height << "\" "
        << "width=\"" << total_width << "\" height=\"" << total_height << "\">\n"
        << "<style>\n"
        << "  text { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }\n"
        << "  .tango-bg { fill: #F8FAFC; rx: 12px; }\n"
        << "  .grid-fine { stroke: #E2E8F0; stroke-width: 1.5; }\n"
        << "  .board-outer { stroke: #0F172A; stroke-width: 3.5; fill: none; }\n"
        << "  .badge-same { fill: #0284C7; stroke: #FFFFFF; stroke-width: 2.0; }\n"
        << "  .badge-opp { fill: #EF4444; stroke: #FFFFFF; stroke-width: 2.0; }\n"
        << "  .badge-line { stroke: #FFFFFF; stroke-width: 2.2; stroke-linecap: round; }\n"
        << "  .sun-core { fill: #F59E0B; stroke: #D97706; stroke-width: 1.5; }\n"
        << "  .sun-ray { stroke: #F59E0B; stroke-width: 2.5; stroke-linecap: round; }\n"
        << "  .moon-body { fill: #6366F1; stroke: #4338CA; stroke-width: 1.5; stroke-linejoin: round; }\n"
        << "  .given-indicator { fill: #94A3B8; }\n"
        << "</style>\n\n"
        << "<rect width=\"" << total_width << "\" height=\"" << total_height << "\" class=\"tango-bg\"/>\n"
        << "<g transform=\"translate(" << padding << ", " << padding << ")\">\n";

    // 1. Draw cell tiles
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            svg << "<rect x=\"" << (c * cell_size) << "\" y=\"" << (r * cell_size)
                << "\" width=\"" << cell_size << "\" height=\"" << cell_size
                << "\" fill=\"#FFFFFF\"/>\n";
        }
    }

    // 2. Grid lines
    for (int i = 0; i <= n; ++i) {
        svg << "<line class=\"grid-fine\" x1=\"0\" y1=\"" << (i * cell_size)
            << "\" x2=\"" << board_size << "\" y2=\"" << (i * cell_size) << "\"/>\n";
        svg << "<line class=\"grid-fine\" x1=\"" << (i * cell_size) << "\" y1=\"0\""
            << " x2=\"" << (i * cell_size) << "\" y2=\"" << board_size << "\"/>\n";
    }

    // 3. Outer border
    svg << "<rect class=\"board-outer\" x=\"0\" y=\"0\" width=\""
        << board_size << "\" height=\"" << board_size << "\"/>\n";

    // 4. Render Sun and Moon glyphs
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n; ++c) {
            char symbol = '.';
            if (solution.size() == static_cast<size_t>(n * n)) {
                symbol = solution[r * n + c];
            } else {
                auto s = board.get_symbol(r, c);
                if (s == TangoSymbol::SUN) symbol = 'S';
                else if (s == TangoSymbol::MOON) symbol = 'M';
            }

            if (symbol == '.' || symbol == ' ') {
                continue;
            }

            double cx = (c + 0.5) * cell_size;
            double cy = (r + 0.5) * cell_size;

            if (symbol == 'S') {
                // Crisp Vector Sun
                double r_core = cell_size * 0.18;
                double r_ray_in = cell_size * 0.24;
                double r_ray_out = cell_size * 0.33;

                // 8 radiant rays
                for (int k = 0; k < 8; ++k) {
                    double angle = k * (PI / 4.0);
                    double x1 = cx + std::cos(angle) * r_ray_in;
                    double y1 = cy + std::sin(angle) * r_ray_in;
                    double x2 = cx + std::cos(angle) * r_ray_out;
                    double y2 = cy + std::sin(angle) * r_ray_out;
                    svg << "<line class=\"sun-ray\" x1=\"" << x1 << "\" y1=\"" << y1
                        << "\" x2=\"" << x2 << "\" y2=\"" << y2 << "\"/>\n";
                }
                // Central sun disc
                svg << "<circle class=\"sun-core\" cx=\"" << cx << "\" cy=\"" << cy
                    << "\" r=\"" << r_core << "\"/>\n";
                // Subtle highlight shine
                svg << "<circle cx=\"" << (cx - cell_size * 0.05) << "\" cy=\"" << (cy - cell_size * 0.05)
                    << "\" r=\"" << (cell_size * 0.06) << "\" fill=\"#FEF3C7\" opacity=\"0.75\"/>\n";

            } else if (symbol == 'M') {
                // Crisp Vector Moon (Crescent geometry)
                double R = cell_size * 0.28;
                double shift = 0.20 * R;

                svg << "<path class=\"moon-body\" d=\"M " << (cx - shift) << " " << (cy - R)
                    << " A " << R << " " << R << " 0 0 0 " << (cx - shift) << " " << (cy + R)
                    << " A " << (0.80 * R) << " " << (0.80 * R) << " 0 0 1 " << (cx - shift) << " " << (cy - R)
                    << " Z\"/>\n";
                // Tiny companion star
                svg << "<circle cx=\"" << (cx + cell_size * 0.24) << "\" cy=\"" << (cy - cell_size * 0.16)
                    << "\" r=\"2.2\" fill=\"#818CF8\"/>\n";
            }

            // If given, subtle corner pin indicator
            if (board.is_given(r, c)) {
                svg << "<circle class=\"given-indicator\" cx=\"" << (cx + cell_size * 0.34)
                    << "\" cy=\"" << (cy + cell_size * 0.34) << "\" r=\"2.5\"/>\n";
            }
        }
    }

    // 5. Render distinct edge clues ('=' and 'x')
    const double badge_r = cell_size * 0.16;

    // Horizontal edge clues
    for (int r = 0; r < n; ++r) {
        for (int c = 0; c < n - 1; ++c) {
            EdgeClue clue = board.get_horizontal_clue(r, c);
            if (clue == EdgeClue::NONE) continue;

            double bx = (c + 1) * cell_size;
            double by = (r + 0.5) * cell_size;

            if (clue == EdgeClue::SAME) {
                // '=' clue badge
                svg << "<circle class=\"badge-same\" cx=\"" << bx << "\" cy=\"" << by << "\" r=\"" << badge_r << "\"/>\n"
                    << "<line class=\"badge-line\" x1=\"" << (bx - badge_r * 0.48) << "\" y1=\"" << (by - badge_r * 0.25)
                    << "\" x2=\"" << (bx + badge_r * 0.48) << "\" y2=\"" << (by - badge_r * 0.25) << "\"/>\n"
                    << "<line class=\"badge-line\" x1=\"" << (bx - badge_r * 0.48) << "\" y1=\"" << (by + badge_r * 0.25)
                    << "\" x2=\"" << (bx + badge_r * 0.48) << "\" y2=\"" << (by + badge_r * 0.25) << "\"/>\n";
            } else if (clue == EdgeClue::OPPOSITE) {
                // 'x' clue badge
                svg << "<circle class=\"badge-opp\" cx=\"" << bx << "\" cy=\"" << by << "\" r=\"" << badge_r << "\"/>\n"
                    << "<line class=\"badge-line\" x1=\"" << (bx - badge_r * 0.45) << "\" y1=\"" << (by - badge_r * 0.45)
                    << "\" x2=\"" << (bx + badge_r * 0.45) << "\" y2=\"" << (by + badge_r * 0.45) << "\"/>\n"
                    << "<line class=\"badge-line\" x1=\"" << (bx + badge_r * 0.45) << "\" y1=\"" << (by - badge_r * 0.45)
                    << "\" x2=\"" << (bx - badge_r * 0.45) << "\" y2=\"" << (by + badge_r * 0.45) << "\"/>\n";
            }
        }
    }

    // Vertical edge clues
    for (int r = 0; r < n - 1; ++r) {
        for (int c = 0; c < n; ++c) {
            EdgeClue clue = board.get_vertical_clue(r, c);
            if (clue == EdgeClue::NONE) continue;

            double bx = (c + 0.5) * cell_size;
            double by = (r + 1) * cell_size;

            if (clue == EdgeClue::SAME) {
                // '=' clue badge
                svg << "<circle class=\"badge-same\" cx=\"" << bx << "\" cy=\"" << by << "\" r=\"" << badge_r << "\"/>\n"
                    << "<line class=\"badge-line\" x1=\"" << (bx - badge_r * 0.48) << "\" y1=\"" << (by - badge_r * 0.25)
                    << "\" x2=\"" << (bx + badge_r * 0.48) << "\" y2=\"" << (by - badge_r * 0.25) << "\"/>\n"
                    << "<line class=\"badge-line\" x1=\"" << (bx - badge_r * 0.48) << "\" y1=\"" << (by + badge_r * 0.25)
                    << "\" x2=\"" << (bx + badge_r * 0.48) << "\" y2=\"" << (by + badge_r * 0.25) << "\"/>\n";
            } else if (clue == EdgeClue::OPPOSITE) {
                // 'x' clue badge
                svg << "<circle class=\"badge-opp\" cx=\"" << bx << "\" cy=\"" << by << "\" r=\"" << badge_r << "\"/>\n"
                    << "<line class=\"badge-line\" x1=\"" << (bx - badge_r * 0.45) << "\" y1=\"" << (by - badge_r * 0.45)
                    << "\" x2=\"" << (bx + badge_r * 0.45) << "\" y2=\"" << (by + badge_r * 0.45) << "\"/>\n"
                    << "<line class=\"badge-line\" x1=\"" << (bx + badge_r * 0.45) << "\" y1=\"" << (by - badge_r * 0.45)
                    << "\" x2=\"" << (bx - badge_r * 0.45) << "\" y2=\"" << (by + badge_r * 0.45) << "\"/>\n";
            }
        }
    }

    svg << "</g>\n</svg>\n";
    return svg.str();
}

std::string export_tango_svg(const TangoBoard& board,
                             const TangoSolution& solution,
                             int cell_size) {
    std::vector<char> chars;
    chars.reserve(board.size() * board.size());
    for (const auto& row : solution.grid) {
        for (char ch : row) {
            chars.push_back(ch);
        }
    }
    return export_tango_svg(board, chars, cell_size);
}

// ============================================================================
// 3. Zip SVG Exporter
// ============================================================================

std::string export_zip_svg(const ZipBoard& board,
                           const std::vector<Position>& path,
                           int cell_size) {
    const int rows = board.rows();
    const int cols = board.cols();
    const int padding = 28;
    const int board_width = cols * cell_size;
    const int board_height = rows * cell_size;
    const int total_width = board_width + padding * 2;
    const int total_height = board_height + padding * 2;

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "viewBox=\"0 0 " << total_width << " " << total_height << "\" "
        << "width=\"" << total_width << "\" height=\"" << total_height << "\">\n"
        << "<style>\n"
        << "  text { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }\n"
        << "  .zip-bg { fill: #F8FAFC; rx: 12px; }\n"
        << "  .grid-fine { stroke: #E2E8F0; stroke-width: 1.0; }\n"
        << "  .wall-line { stroke: #DC2626; stroke-width: 4.5; stroke-linecap: round; }\n"
        << "  .board-outer { stroke: #0F172A; stroke-width: 3.5; fill: none; }\n"
        << "  .path-glow { fill: none; stroke: #10B981; stroke-width: " << (cell_size * 0.22)
        << "; stroke-linecap: round; stroke-linejoin: round; opacity: 0.28; }\n"
        << "  .path-main { fill: none; stroke: #059669; stroke-width: " << std::max(3.0, cell_size * 0.08)
        << "; stroke-linecap: round; stroke-linejoin: round; }\n"
        << "  .arrow-head { fill: #047857; }\n"
        << "  .step-badge { fill: #ECFDF5; stroke: #10B981; stroke-width: 1.5; }\n"
        << "  .step-text { fill: #065F46; font-weight: 600; }\n"
        << "  .checkpoint-circle { fill: #4F46E5; stroke: #312E81; stroke-width: 2.5; }\n"
        << "  .checkpoint-text { fill: #FFFFFF; font-weight: 700; }\n"
        << "</style>\n\n"
        << "<rect width=\"" << total_width << "\" height=\"" << total_height << "\" class=\"zip-bg\"/>\n"
        << "<g transform=\"translate(" << padding << ", " << padding << ")\">\n";

    // 1. Grid background cells
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            svg << "<rect x=\"" << (c * cell_size) << "\" y=\"" << (r * cell_size)
                << "\" width=\"" << cell_size << "\" height=\"" << cell_size
                << "\" fill=\"#FFFFFF\"/>\n";
        }
    }

    // 2. Fine grid lines
    for (int r = 0; r <= rows; ++r) {
        svg << "<line class=\"grid-fine\" x1=\"0\" y1=\"" << (r * cell_size)
            << "\" x2=\"" << board_width << "\" y2=\"" << (r * cell_size) << "\"/>\n";
    }
    for (int c = 0; c <= cols; ++c) {
        svg << "<line class=\"grid-fine\" x1=\"" << (c * cell_size) << "\" y1=\"0\""
            << " x2=\"" << (c * cell_size) << "\" y2=\"" << board_height << "\"/>\n";
    }

    // 3. Continuous Snake Path (under checkpoints and walls)
    if (path.size() >= 2) {
        std::ostringstream points_ss;
        for (size_t i = 0; i < path.size(); ++i) {
            double px = (path[i].col + 0.5) * cell_size;
            double py = (path[i].row + 0.5) * cell_size;
            if (i > 0) points_ss << " ";
            points_ss << px << "," << py;
        }

        // Glow polyline
        svg << "<polyline class=\"path-glow\" points=\"" << points_ss.str() << "\"/>\n";
        // Main path line
        svg << "<polyline class=\"path-main\" points=\"" << points_ss.str() << "\"/>\n";

        // Segment Directional Arrows
        const double arrow_len = cell_size * 0.16;
        const double arrow_wid = cell_size * 0.13;

        for (size_t i = 0; i + 1 < path.size(); ++i) {
            const auto& p1 = path[i];
            const auto& p2 = path[i + 1];

            double mx = ((p1.col + p2.col) * 0.5 + 0.5) * cell_size;
            double my = ((p1.row + p2.row) * 0.5 + 0.5) * cell_size;

            double dx = p2.col - p1.col;
            double dy = p2.row - p1.row;
            double dlen = std::hypot(dx, dy);
            if (dlen > 0.001) {
                dx /= dlen;
                dy /= dlen;
            }

            double tip_x = mx + dx * (arrow_len * 0.5);
            double tip_y = my + dy * (arrow_len * 0.5);
            double base_x = mx - dx * (arrow_len * 0.5);
            double base_y = my - dy * (arrow_len * 0.5);

            double w1_x = base_x - dy * (arrow_wid * 0.5);
            double w1_y = base_y + dx * (arrow_wid * 0.5);
            double w2_x = base_x + dy * (arrow_wid * 0.5);
            double w2_y = base_y - dx * (arrow_wid * 0.5);

            svg << "<polygon class=\"arrow-head\" points=\""
                << tip_x << "," << tip_y << " "
                << w1_x << "," << w1_y << " "
                << w2_x << "," << w2_y << "\"/>\n";
        }

        // Step numbers for non-checkpoint cells
        for (size_t i = 0; i < path.size(); ++i) {
            const auto& p = path[i];
            if (board.checkpoint_at(p.row, p.col) == 0) {
                double cx = (p.col + 0.5) * cell_size;
                double cy = (p.row + 0.5) * cell_size;
                double r_step = cell_size * 0.18;
                svg << "<circle class=\"step-badge\" cx=\"" << cx << "\" cy=\"" << cy
                    << "\" r=\"" << r_step << "\"/>\n";
                svg << "<text class=\"step-text\" x=\"" << cx << "\" y=\"" << cy
                    << "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                    << "font-size=\"" << (cell_size * 0.18) << "px\">" << (i + 1) << "</text>\n";
            }
        }
    }

    // 4. Checkpoint Circles and Numbers
    const double cp_r = cell_size * 0.36;
    for (int cp = 1; cp <= board.max_checkpoint(); ++cp) {
        Position pos = board.checkpoint_pos(cp);
        if (pos.row < 0 || pos.col < 0) continue;

        double cx = (pos.col + 0.5) * cell_size;
        double cy = (pos.row + 0.5) * cell_size;

        svg << "<circle class=\"checkpoint-circle\" cx=\"" << cx << "\" cy=\"" << cy
            << "\" r=\"" << cp_r << "\"/>\n";
        svg << "<text class=\"checkpoint-text\" x=\"" << cx << "\" y=\"" << cy
            << "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
            << "font-size=\"" << (cell_size * 0.36) << "px\">" << cp << "</text>\n";
    }

    // 5. Red Wall Lines (Horizontal and Vertical)
    for (int r = 0; r < rows - 1; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board.has_h_wall(r, c)) {
                double y = (r + 1) * cell_size;
                double x1 = c * cell_size;
                double x2 = (c + 1) * cell_size;
                svg << "<line class=\"wall-line\" x1=\"" << x1 << "\" y1=\"" << y
                    << "\" x2=\"" << x2 << "\" y2=\"" << y << "\"/>\n";
            }
        }
    }

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            if (board.has_v_wall(r, c)) {
                double x = (c + 1) * cell_size;
                double y1 = r * cell_size;
                double y2 = (r + 1) * cell_size;
                svg << "<line class=\"wall-line\" x1=\"" << x << "\" y1=\"" << y1
                    << "\" x2=\"" << x << "\" y2=\"" << y2 << "\"/>\n";
            }
        }
    }

    // 6. Outer border
    svg << "<rect class=\"board-outer\" x=\"0\" y=\"0\" width=\""
        << board_width << "\" height=\"" << board_height << "\"/>\n";

    svg << "</g>\n</svg>\n";
    return svg.str();
}

std::string export_zip_svg(const ZipBoard& board,
                           const ZipSolution& solution,
                           int cell_size) {
    return export_zip_svg(board, solution.path, cell_size);
}

// ============================================================================
// 4. Patches SVG Exporter
// ============================================================================

std::string export_patches_svg(const PatchesBoard& board,
                               const PatchesSolution& solution,
                               int cell_size) {
    const int rows = board.rows();
    const int cols = board.cols();
    const int padding = 28;
    const int board_width = cols * cell_size;
    const int board_height = rows * cell_size;
    const int total_width = board_width + padding * 2;
    const int total_height = board_height + padding * 2;

    std::vector<std::vector<int>> cell_patch(rows, std::vector<int>(cols, -1));
    for (size_t p = 0; p < solution.patches.size(); ++p) {
        for (const auto& cell : solution.patches[p].cells) {
            if (cell.row >= 0 && cell.row < rows && cell.col >= 0 && cell.col < cols) {
                cell_patch[cell.row][cell.col] = static_cast<int>(p);
            }
        }
    }

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "viewBox=\"0 0 " << total_width << " " << total_height << "\" "
        << "width=\"" << total_width << "\" height=\"" << total_height << "\">\n"
        << "<style>\n"
        << "  text { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }\n"
        << "  .patches-bg { fill: #F8FAFC; rx: 12px; }\n"
        << "  .grid-fine { stroke: #000000; stroke-opacity: 0.10; stroke-width: 1.0; }\n"
        << "  .patch-border { stroke: #0F172A; stroke-width: 3.5; stroke-linecap: round; }\n"
        << "  .board-outer { stroke: #0F172A; stroke-width: 4.0; fill: none; }\n"
        << "  .clue-badge { fill: #FFFFFF; stroke: #0F172A; stroke-width: 2.0; rx: 6px; }\n"
        << "  .clue-text { fill: #0F172A; font-weight: 700; }\n"
        << "</style>\n\n"
        << "<rect width=\"" << total_width << "\" height=\"" << total_height << "\" class=\"patches-bg\"/>\n"
        << "<g transform=\"translate(" << padding << ", " << padding << ")\">\n";

    // 1. Colored Patch Cells
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int pid = cell_patch[r][c];
            std::string color = (pid >= 0) ? get_region_color(pid, static_cast<int>(solution.patches.size())) : "#FFFFFF";
            svg << "<rect x=\"" << (c * cell_size) << "\" y=\"" << (r * cell_size)
                << "\" width=\"" << cell_size << "\" height=\"" << cell_size
                << "\" fill=\"" << color << "\"/>\n";
        }
    }

    // 2. Subtle internal grid lines
    for (int r = 0; r <= rows; ++r) {
        svg << "<line class=\"grid-fine\" x1=\"0\" y1=\"" << (r * cell_size)
            << "\" x2=\"" << board_width << "\" y2=\"" << (r * cell_size) << "\"/>\n";
    }
    for (int c = 0; c <= cols; ++c) {
        svg << "<line class=\"grid-fine\" x1=\"" << (c * cell_size) << "\" y1=\"0\""
            << " x2=\"" << (c * cell_size) << "\" y2=\"" << board_height << "\"/>\n";
    }

    // 3. Thick visible patch borders between cells of different patches
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            int curr_pid = cell_patch[r][c];

            // Horizontal border with row above
            if (r > 0 && cell_patch[r - 1][c] != curr_pid) {
                svg << "<line class=\"patch-border\" x1=\"" << (c * cell_size)
                    << "\" y1=\"" << (r * cell_size)
                    << "\" x2=\"" << ((c + 1) * cell_size)
                    << "\" y2=\"" << (r * cell_size) << "\"/>\n";
            }

            // Vertical border with col to left
            if (c > 0 && cell_patch[r][c - 1] != curr_pid) {
                svg << "<line class=\"patch-border\" x1=\"" << (c * cell_size)
                    << "\" y1=\"" << (r * cell_size)
                    << "\" x2=\"" << (c * cell_size)
                    << "\" y2=\"" << ((r + 1) * cell_size) << "\"/>\n";
            }
        }
    }

    // 4. Outer border
    svg << "<rect class=\"board-outer\" x=\"0\" y=\"0\" width=\""
        << board_width << "\" height=\"" << board_height << "\"/>\n";

    // 5. Clue labels and badges in their clue cells
    for (const auto& clue : board.clues()) {
        double cx = (clue.pos.col + 0.5) * cell_size;
        double cy = (clue.pos.row + 0.5) * cell_size;

        double bw = cell_size * 0.76;
        double bh = cell_size * 0.54;

        svg << "<rect class=\"clue-badge\" x=\"" << (cx - bw * 0.5) << "\" y=\"" << (cy - bh * 0.5)
            << "\" width=\"" << bw << "\" height=\"" << bh << "\"/>\n";

        std::string label = xml_escape(clue.label());
        svg << "<text class=\"clue-text\" x=\"" << cx << "\" y=\"" << cy
            << "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
            << "font-size=\"" << (cell_size * 0.28) << "px\">" << label << "</text>\n";
    }

    svg << "</g>\n</svg>\n";
    return svg.str();
}

// ============================================================================
// 5. Wend SVG Exporter
// ============================================================================

std::string export_wend_svg(const WendBoard& board,
                            const WendSolution& solution,
                            int cell_size) {
    const int rows = board.rows();
    const int cols = board.cols();
    const int padding = 28;
    const int board_width = cols * cell_size;
    const int board_height = rows * cell_size;

    // Palette of vibrant colors for word paths
    const std::vector<std::string> word_colors = {
        "#2563EB", // Blue
        "#059669", // Emerald
        "#D97706", // Amber
        "#7C3AED", // Purple
        "#DB2777", // Pink
        "#0891B2", // Cyan
        "#EA580C", // Orange
        "#4F46E5"  // Indigo
    };

    // Calculate legend height below board
    int legend_h = 0;
    if (!solution.words.empty()) {
        legend_h = 36 + static_cast<int>((solution.words.size() + 2) / 3) * 28;
    }

    const int total_width = board_width + padding * 2;
    const int total_height = board_height + padding * 2 + legend_h;

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "viewBox=\"0 0 " << total_width << " " << total_height << "\" "
        << "width=\"" << total_width << "\" height=\"" << total_height << "\">\n"
        << "<style>\n"
        << "  text { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }\n"
        << "  .wend-bg { fill: #F8FAFC; rx: 12px; }\n"
        << "  .letter-tile { fill: #FFFFFF; stroke: #E2E8F0; stroke-width: 1.5; rx: 8px; }\n"
        << "  .letter-text { fill: #0F172A; font-weight: 700; }\n"
        << "</style>\n\n"
        << "<rect width=\"" << total_width << "\" height=\"" << total_height << "\" class=\"wend-bg\"/>\n"
        << "<g transform=\"translate(" << padding << ", " << padding << ")\">\n";

    // 1. Letter grid tiles
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board.is_void(r, c)) continue;
            double tx = c * cell_size + 4.0;
            double ty = r * cell_size + 4.0;
            double tw = cell_size - 8.0;
            double th = cell_size - 8.0;
            svg << "<rect class=\"letter-tile\" x=\"" << tx << "\" y=\"" << ty
                << "\" width=\"" << tw << "\" height=\"" << th << "\"/>\n";
        }
    }

    // 2. Colored snake paths for each word
    for (size_t wi = 0; wi < solution.words.size(); ++wi) {
        const auto& wp = solution.words[wi];
        std::string color = word_colors[wi % word_colors.size()];

        if (wp.path.size() >= 2) {
            std::ostringstream points_ss;
            for (size_t i = 0; i < wp.path.size(); ++i) {
                double px = (wp.path[i].col + 0.5) * cell_size;
                double py = (wp.path[i].row + 0.5) * cell_size;
                if (i > 0) points_ss << " ";
                points_ss << px << "," << py;
            }

            // Broad ribbon highlight behind letters
            svg << "<polyline points=\"" << points_ss.str() << "\" fill=\"none\" "
                << "stroke=\"" << color << "\" stroke-width=\"" << (cell_size * 0.28)
                << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\" opacity=\"0.28\"/>\n";

            // Center path line
            svg << "<polyline points=\"" << points_ss.str() << "\" fill=\"none\" "
                << "stroke=\"" << color << "\" stroke-width=\"3.0\" "
                << "stroke-linecap=\"round\" stroke-linejoin=\"round\" opacity=\"0.9\"/>\n";

            // Directional chevrons along path segments
            const double arrow_len = cell_size * 0.14;
            const double arrow_wid = cell_size * 0.12;

            for (size_t i = 0; i + 1 < wp.path.size(); ++i) {
                const auto& p1 = wp.path[i];
                const auto& p2 = wp.path[i + 1];

                double mx = ((p1.col + p2.col) * 0.5 + 0.5) * cell_size;
                double my = ((p1.row + p2.row) * 0.5 + 0.5) * cell_size;

                double dx = p2.col - p1.col;
                double dy = p2.row - p1.row;
                double dlen = std::hypot(dx, dy);
                if (dlen > 0.001) {
                    dx /= dlen;
                    dy /= dlen;
                }

                double tip_x = mx + dx * (arrow_len * 0.5);
                double tip_y = my + dy * (arrow_len * 0.5);
                double base_x = mx - dx * (arrow_len * 0.5);
                double base_y = my - dy * (arrow_len * 0.5);

                double w1_x = base_x - dy * (arrow_wid * 0.5);
                double w1_y = base_y + dx * (arrow_wid * 0.5);
                double w2_x = base_x + dy * (arrow_wid * 0.5);
                double w2_y = base_y - dx * (arrow_wid * 0.5);

                svg << "<polygon points=\""
                    << tip_x << "," << tip_y << " "
                    << w1_x << "," << w1_y << " "
                    << w2_x << "," << w2_y << "\" fill=\"" << color << "\"/>\n";
            }

            // Start marker circle
            double scx = (wp.path[0].col + 0.5) * cell_size;
            double scy = (wp.path[0].row + 0.5) * cell_size;
            svg << "<circle cx=\"" << scx << "\" cy=\"" << scy << "\" r=\"" << (cell_size * 0.12)
                << "\" fill=\"" << color << "\" stroke=\"#FFFFFF\" stroke-width=\"1.5\"/>\n";
        }
    }

    // 3. Crisp letters rendered on top of paths
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (board.is_void(r, c)) continue;
            char ch = board.letter_at(r, c);
            double cx = (c + 0.5) * cell_size;
            double cy = (r + 0.5) * cell_size;

            svg << "<text class=\"letter-text\" x=\"" << cx << "\" y=\"" << cy
                << "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                << "font-size=\"" << (cell_size * 0.44) << "px\">" << ch << "</text>\n";
        }
    }

    // 4. Word legend below board
    if (!solution.words.empty()) {
        double leg_y = board_height + 20.0;
        svg << "<!-- Words Legend -->\n"
            << "<g transform=\"translate(0, " << leg_y << ")\">\n";

        double cur_x = 0;
        double cur_y = 0;
        const double col_w = (board_width) / 3.0;

        for (size_t wi = 0; wi < solution.words.size(); ++wi) {
            const auto& wp = solution.words[wi];
            std::string color = word_colors[wi % word_colors.size()];

            svg << "<circle cx=\"" << (cur_x + 8.0) << "\" cy=\"" << (cur_y + 8.0)
                << "\" r=\"5\" fill=\"" << color << "\"/>\n";
            svg << "<text x=\"" << (cur_x + 18.0) << "\" y=\"" << (cur_y + 8.0)
                << "\" dominant-baseline=\"central\" font-size=\"12px\" font-weight=\"600\" fill=\"#1E293B\">"
                << xml_escape(wp.word) << " (" << wp.length() << ")</text>\n";

            cur_x += col_w;
            if ((wi + 1) % 3 == 0) {
                cur_x = 0;
                cur_y += 24.0;
            }
        }
        svg << "</g>\n";
    }

    svg << "</g>\n</svg>\n";
    return svg.str();
}

// ============================================================================
// 6. Crossclimb SVG Exporter
// ============================================================================

std::string export_crossclimb_svg(const CrossclimbBoard& board,
                                  const CrossclimbSolution& solution,
                                  int rung_width,
                                  int rung_height) {
    std::vector<std::string> words = solution.ladder;
    if (words.empty()) {
        if (board.has_start_word()) words.push_back(*board.start_word());
        for (const auto& w : board.rung_words()) words.push_back(w);
        if (board.has_end_word()) words.push_back(*board.end_word());
    }

    const int num_rungs = static_cast<int>(words.size());
    const int word_len = (board.word_length() > 0) ? board.word_length()
                        : (!words.empty() ? static_cast<int>(words[0].size()) : 4);

    const int rung_gap = 18;
    const int padding_x = 40;
    const int padding_y = 30;
    const int total_width = rung_width + padding_x * 2;
    const int ladder_h = std::max(1, num_rungs) * rung_height + std::max(0, num_rungs - 1) * rung_gap;
    const int total_height = ladder_h + padding_y * 2;

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "viewBox=\"0 0 " << total_width << " " << total_height << "\" "
        << "width=\"" << total_width << "\" height=\"" << total_height << "\">\n"
        << "<style>\n"
        << "  text { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }\n"
        << "  .crossclimb-bg { fill: #F8FAFC; rx: 16px; }\n"
        << "  .ladder-rail { stroke: #94A3B8; stroke-width: 8; stroke-linecap: round; }\n"
        << "  .rung-card { rx: 10px; stroke-width: 2.0; }\n"
        << "  .letter-box { rx: 6px; }\n"
        << "  .transition-line { stroke: #EAB308; stroke-width: 2.5; stroke-linecap: round; }\n"
        << "  .transition-arrow { fill: #EAB308; }\n"
        << "</style>\n\n"
        << "<rect width=\"" << total_width << "\" height=\"" << total_height << "\" class=\"crossclimb-bg\"/>\n";

    // 1. Ladder vertical side rails
    double rail_left = padding_x - 12.0;
    double rail_right = padding_x + rung_width + 12.0;
    double rail_y1 = padding_y - 10.0;
    double rail_y2 = padding_y + ladder_h + 10.0;

    svg << "<line class=\"ladder-rail\" x1=\"" << rail_left << "\" y1=\"" << rail_y1
        << "\" x2=\"" << rail_left << "\" y2=\"" << rail_y2 << "\"/>\n"
        << "<line class=\"ladder-rail\" x1=\"" << rail_right << "\" y1=\"" << rail_y1
        << "\" x2=\"" << rail_right << "\" y2=\"" << rail_y2 << "\"/>\n";

    // 2. Rungs and Letter Transitions
    const double box_margin = 10.0;
    const double avail_w = rung_width - box_margin * 2.0;
    const double cell_w = (word_len > 0) ? (avail_w / word_len) : avail_w;
    const double box_w = std::max(12.0, cell_w - 4.0);
    const double box_h = std::max(12.0, rung_height - 12.0);

    for (int i = 0; i < num_rungs; ++i) {
        double ry = padding_y + i * (rung_height + rung_gap);

        // Rung card container styling: start is green tinted, end is blue tinted
        std::string card_fill = "#FFFFFF";
        std::string card_stroke = "#CBD5E1";
        if (i == 0) {
            card_fill = "#F0FDF4";
            card_stroke = "#10B981";
        } else if (i == num_rungs - 1) {
            card_fill = "#EFF6FF";
            card_stroke = "#3B82F6";
        }

        svg << "<rect class=\"rung-card\" x=\"" << padding_x << "\" y=\"" << ry
            << "\" width=\"" << rung_width << "\" height=\"" << rung_height
            << "\" fill=\"" << card_fill << "\" stroke=\"" << card_stroke << "\"/>\n";

        const std::string& curr_word = words[i];

        // Highlight 1-letter transition indicator between rungs
        int diff_col = -1;
        if (i > 0 && words[i - 1].size() == curr_word.size()) {
            for (int j = 0; j < word_len && j < static_cast<int>(curr_word.size()); ++j) {
                if (curr_word[j] != words[i - 1][j]) {
                    diff_col = j;
                    break;
                }
            }
        }

        if (diff_col >= 0) {
            double ax = padding_x + box_margin + diff_col * cell_w + (cell_w - 4.0) * 0.5 + 2.0;
            double top_y = ry - rung_gap + 2.0;
            double bot_y = ry - 2.0;

            svg << "<line class=\"transition-line\" x1=\"" << ax << "\" y1=\"" << top_y
                << "\" x2=\"" << ax << "\" y2=\"" << bot_y << "\"/>\n";
            svg << "<polygon class=\"transition-arrow\" points=\""
                << (ax - 3.5) << "," << (bot_y - 4.0) << " "
                << (ax + 3.5) << "," << (bot_y - 4.0) << " "
                << ax << "," << bot_y << "\"/>\n";
        }

        // Draw letter boxes
        for (int j = 0; j < word_len && j < static_cast<int>(curr_word.size()); ++j) {
            double bx = padding_x + box_margin + j * cell_w + 2.0;
            double by = ry + 6.0;

            bool is_diff = (j == diff_col);

            std::string bg_fill = is_diff ? "#FEF08A" : "#F8FAFC";
            std::string border_stroke = is_diff ? "#EAB308" : "#E2E8F0";
            double stroke_w = is_diff ? 1.8 : 1.0;
            std::string text_fill = is_diff ? "#854D0E" : "#334155";
            std::string font_weight = is_diff ? "800" : "600";

            svg << "<rect class=\"letter-box\" x=\"" << bx << "\" y=\"" << by
                << "\" width=\"" << box_w << "\" height=\"" << box_h
                << "\" fill=\"" << bg_fill << "\" stroke=\"" << border_stroke
                << "\" stroke-width=\"" << stroke_w << "\"/>\n";

            double lcx = bx + box_w * 0.5;
            double lcy = by + box_h * 0.5;
            svg << "<text x=\"" << lcx << "\" y=\"" << lcy
                << "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                << "font-size=\"" << (box_h * 0.62) << "px\" font-weight=\"" << font_weight
                << "\" fill=\"" << text_fill << "\">" << curr_word[j] << "</text>\n";
        }
    }

    svg << "</svg>\n";
    return svg.str();
}

// ============================================================================
// SvgExporter Class Forwarding Methods
// ============================================================================

std::string SvgExporter::export_sudoku_svg(const SudokuBoard& board,
                                           const std::vector<int>& solution,
                                           int cell_size) {
    return solvedin::export_sudoku_svg(board, solution, cell_size);
}

std::string SvgExporter::export_sudoku_svg(const SudokuBoard& board,
                                           const SudokuSolution& solution,
                                           int cell_size) {
    return solvedin::export_sudoku_svg(board, solution, cell_size);
}

std::string SvgExporter::export_tango_svg(const TangoBoard& board,
                                         const std::vector<char>& solution,
                                         int cell_size) {
    return solvedin::export_tango_svg(board, solution, cell_size);
}

std::string SvgExporter::export_tango_svg(const TangoBoard& board,
                                         const TangoSolution& solution,
                                         int cell_size) {
    return solvedin::export_tango_svg(board, solution, cell_size);
}

std::string SvgExporter::export_zip_svg(const ZipBoard& board,
                                       const std::vector<Position>& path,
                                       int cell_size) {
    return solvedin::export_zip_svg(board, path, cell_size);
}

std::string SvgExporter::export_zip_svg(const ZipBoard& board,
                                       const ZipSolution& solution,
                                       int cell_size) {
    return solvedin::export_zip_svg(board, solution, cell_size);
}

std::string SvgExporter::export_patches_svg(const PatchesBoard& board,
                                           const PatchesSolution& solution,
                                           int cell_size) {
    return solvedin::export_patches_svg(board, solution, cell_size);
}

std::string SvgExporter::export_wend_svg(const WendBoard& board,
                                        const WendSolution& solution,
                                        int cell_size) {
    return solvedin::export_wend_svg(board, solution, cell_size);
}

std::string SvgExporter::export_crossclimb_svg(const CrossclimbBoard& board,
                                              const CrossclimbSolution& solution,
                                              int rung_width,
                                              int rung_height) {
    return solvedin::export_crossclimb_svg(board, solution, rung_width, rung_height);
}

} // namespace solvedin
