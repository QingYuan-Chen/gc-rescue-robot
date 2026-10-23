cmake_minimum_required(VERSION 3.28)
project(safe_baseline_tests C)

set(firmware_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/stm32)
add_executable(safe_baseline_tests
    test_safe_baseline.c
    ${firmware_dir}/App/bsp/board_outputs.c
    ${firmware_dir}/App/debug/debug_console.c
)
target_include_directories(safe_baseline_tests PRIVATE
    stubs ${firmware_dir}/Core/Inc ${firmware_dir}/App
)
set_target_properties(safe_baseline_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
target_compile_options(safe_baseline_tests PRIVATE -Wall -Wextra -Werror)
enable_testing()
add_test(NAME safe_baseline_contract COMMAND safe_baseline_tests)
