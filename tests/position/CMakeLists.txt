cmake_minimum_required(VERSION 3.20)
project(position_check C)
enable_testing()
add_executable(position_check test_position.c
    ../../firmware/stm32/App/runtime/wheel_io.c
    ../../firmware/stm32/App/control/wheel_speed.c
    ../../firmware/stm32/App/debug/position_commands.c)
target_include_directories(position_check PRIVATE stubs ../../firmware/stm32/App)
target_compile_features(position_check PRIVATE c_std_11)
target_compile_options(position_check PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(position_check PRIVATE m)
add_test(NAME position_check COMMAND position_check)
