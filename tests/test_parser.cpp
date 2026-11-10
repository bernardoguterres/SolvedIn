#include "solvedin/board.hpp"

#include <cassert>
#include <iostream>
#include <string>

void test_valid_board_loading() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_8x8_unique.txt");
    assert(success);
    assert(maybe_board.has_value());
    assert(maybe_board->size() == 8);
    assert(maybe_board->regions().size() == 64);
    assert(maybe_board->distinct_regions().size() == 8);
    assert(err.empty());
    std::cout << "[PASS] test_valid_board_loading\n";
}

void test_malformed_region_count() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/malformed_region_count.txt");
    assert(!success);
    assert(!maybe_board.has_value());
    assert(err.find("requiring exactly 4 distinct regions, but found 3") != std::string::npos);
    std::cout << "[PASS] test_malformed_region_count\n";
}

void test_malformed_dimensions() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/malformed_dimensions.txt");
    assert(!success);
    assert(!maybe_board.has_value());
    assert(err.find("expected 16 region values") != std::string::npos);
    std::cout << "[PASS] test_malformed_dimensions\n";
}

void test_malformed_non_integer() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/malformed_non_integer.txt");
    assert(!success);
    assert(!maybe_board.has_value());
    assert(err.find("non-integer token") != std::string::npos);
    std::cout << "[PASS] test_malformed_non_integer\n";
}

void test_malformed_empty() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/malformed_empty.txt");
    assert(!success);
    assert(!maybe_board.has_value());
    assert(err.find("empty or missing board size N") != std::string::npos);
    std::cout << "[PASS] test_malformed_empty\n";
}

void test_nonexistent_file() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/does_not_exist.txt");
    assert(!success);
    assert(!maybe_board.has_value());
    assert(err.find("Failed to open file") != std::string::npos);
    std::cout << "[PASS] test_nonexistent_file\n";
}

void test_invalid_size_n() {
    auto res_zero = solvedin::Board::parse_from_string("0");
    assert(!res_zero.success);
    assert(res_zero.error_message.find("positive") != std::string::npos);

    auto res_neg = solvedin::Board::parse_from_string("-3");
    assert(!res_neg.success);
    assert(res_neg.error_message.find("positive") != std::string::npos);

    auto res_nan = solvedin::Board::parse_from_string("four");
    assert(!res_nan.success);
    assert(res_nan.error_message.find("not a valid integer") != std::string::npos);

    std::cout << "[PASS] test_invalid_size_n\n";
}

void test_trailing_tokens() {
    std::string content = "2\n1 2\n2 1\nextra_token\n";
    auto res = solvedin::Board::parse_from_string(content);
    assert(!res.success);
    assert(res.error_message.find("extra trailing token") != std::string::npos);
    std::cout << "[PASS] test_trailing_tokens\n";
}

int main() {
    std::cout << "Running Parser Tests...\n";
    test_valid_board_loading();
    test_malformed_region_count();
    test_malformed_dimensions();
    test_malformed_non_integer();
    test_malformed_empty();
    test_nonexistent_file();
    test_invalid_size_n();
    test_trailing_tokens();
    std::cout << "All Parser Tests Passed!\n";
    return 0;
}
