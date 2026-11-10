cmake_minimum_required(VERSION 3.28)
project(wheel_io_tests C)
set(firmware_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/stm32)
add_executable(wheel_io_tests
    test_wheel_io.c
    ${firmware_dir}/Core/Src/tim.c
    ${firmware_dir}/Core/Src/stm32h7xx_hal_timebase_tim.c
    ${firmware_dir}/App/bsp/encoder.c
    ${firmware_dir}/App/bsp/motor.c
    ${firmware_dir}/App/runtime/wheel_io.c
    ${firmware_dir}/App/debug/motor_commands.c
)
target_include_directories(wheel_io_tests PRIVATE
    stubs ../safe_baseline/stubs ${firmware_dir}/Core/Inc ${firmware_dir}/App
)
set_target_properties(wheel_io_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
target_compile_options(wheel_io_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
enable_testing()
foreach(case startup configuration encoder motor timeout stop fault commands)
    add_test(NAME wheel_${case} COMMAND wheel_io_tests ${case})
endforeach()
