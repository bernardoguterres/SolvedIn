cmake_minimum_required(VERSION 3.20)
project(SolvedIn VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer options
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Enable compiler warnings
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core static library
add_library(solvedin_lib STATIC
    src/board.cpp
    src/crossclimb_board.cpp
    src/crossclimb_solver.cpp
    src/crossclimb_validator.cpp
    src/patches_board.cpp
    src/patches_solver.cpp
    src/patches_validator.cpp
    src/pinpoint.cpp
    src/queens_generator.cpp
    src/queens_solver.cpp
    src/queens_validator.cpp
    src/sudoku_board.cpp
    src/sudoku_generator.cpp
    src/sudoku_solver.cpp
    src/sudoku_validator.cpp
    src/svg_exporter.cpp
    src/reasoning.cpp
    src/pinpoint_logic.cpp
    src/crossclimb_logic.cpp
    src/wend_logic.cpp
    src/patches_logic.cpp
    src/zip_logic.cpp
    src/sudoku_logic.cpp
    src/queens_logic.cpp
    src/tango_board.cpp
    src/tango_logic.cpp
    src/tango_generator.cpp
    src/tango_solver.cpp
    src/tango_validator.cpp
    src/trie.cpp
    src/wend_board.cpp
    src/wend_solver.cpp
    src/wend_validator.cpp
    src/zip_board.cpp
    src/zip_generator.cpp
    src/zip_solver.cpp
    src/zip_validator.cpp
)

target_include_directories(solvedin_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Main CLI executable
add_executable(solvedin
    src/main.cpp
)

target_link_libraries(solvedin PRIVATE
    solvedin_lib
)

# Benchmark runner executable
add_executable(benchmark-queens
    benchmarks/benchmark_runner.cpp
)

target_link_libraries(benchmark-queens PRIVATE
    solvedin_lib
)

# Giant Queens demo executable
add_executable(giant-queens
    benchmarks/giant_queens.cpp
)

target_link_libraries(giant-queens PRIVATE
    solvedin_lib
)

# Multi-tier corpus benchmark executable
add_executable(corpus-benchmark
    benchmarks/corpus_benchmark.cpp
)

target_link_libraries(corpus-benchmark PRIVATE
    solvedin_lib
)

# Testing with CTest
enable_testing()

function(add_puzzle_test TEST_NAME TEST_SRC)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE solvedin_lib)
    # Tests use assert(); keep it active even in Release (-DNDEBUG would turn every check into a no-op).
    if(NOT MSVC)
        target_compile_options(${TEST_NAME} PRIVATE -UNDEBUG)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

add_puzzle_test(test_parser tests/test_parser.cpp)
add_puzzle_test(test_validator tests/test_validator.cpp)
add_puzzle_test(test_solver tests/test_solver.cpp)
add_puzzle_test(test_bitset tests/test_bitset.cpp)
add_puzzle_test(test_variants tests/test_variants.cpp)
add_puzzle_test(test_generator tests/test_generator.cpp)
add_puzzle_test(test_sudoku tests/test_sudoku.cpp)
add_puzzle_test(test_tango tests/test_tango.cpp)
add_puzzle_test(test_tango_logic tests/test_tango_logic.cpp)
add_puzzle_test(test_queens_logic tests/test_queens_logic.cpp)
add_puzzle_test(test_sudoku_logic tests/test_sudoku_logic.cpp)
add_puzzle_test(test_zip_logic tests/test_zip_logic.cpp)
add_puzzle_test(test_patches_logic tests/test_patches_logic.cpp)
add_puzzle_test(test_wend_logic tests/test_wend_logic.cpp)
add_puzzle_test(test_crossclimb_logic tests/test_crossclimb_logic.cpp)
add_puzzle_test(test_pinpoint_logic tests/test_pinpoint_logic.cpp)
add_puzzle_test(test_zip tests/test_zip.cpp)
add_puzzle_test(test_patches tests/test_patches.cpp)
add_puzzle_test(test_wend tests/test_wend.cpp)
add_puzzle_test(test_crossclimb tests/test_crossclimb.cpp)
add_puzzle_test(test_pinpoint tests/test_pinpoint.cpp)
add_puzzle_test(test_svg_export tests/test_svg_export.cpp)
add_puzzle_test(test_puzzle_generators tests/test_puzzle_generators.cpp)

# CLI integration test
add_executable(test_cli tests/test_cli.cpp)
target_link_libraries(test_cli PRIVATE solvedin_lib)
if(NOT MSVC)
    target_compile_options(test_cli PRIVATE -UNDEBUG)
endif()
add_test(NAME test_cli COMMAND test_cli "$<TARGET_FILE:solvedin>")
set_tests_properties(test_cli PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Python Brute-Force Oracle test
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME test_python_oracle COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/oracle.py" "$<TARGET_FILE:solvedin>")
set_tests_properties(test_python_oracle PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Pinpoint holdout ranking helper (prints full ranked list; no gold access)
add_executable(pinpoint-rank benchmarks/pinpoint_rank.cpp)
target_link_libraries(pinpoint-rank PRIVATE solvedin_lib)
