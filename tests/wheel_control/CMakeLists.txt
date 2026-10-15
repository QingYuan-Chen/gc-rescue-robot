cmake_minimum_required(VERSION 3.28)
project(wheel_control_tests C)

# 直接编译正式控制层，只替换硬件读写与 RTOS 临界区，不进入 STM32 构建。
set(app_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../firmware/stm32/App)
add_executable(wheel_control_tests
    test_wheel_control.c
    ${app_dir}/control/pid.c
    ${app_dir}/control/wheel_pid.c
    ${app_dir}/control/wheel_drive.c
    ${app_dir}/debug/wheel_commands.c
    ${app_dir}/debug/wheel_status.c
)
target_include_directories(wheel_control_tests PRIVATE stubs ${app_dir})
set_target_properties(wheel_control_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
target_compile_options(wheel_control_tests PRIVATE -Wall -Wextra -Werror)
enable_testing()
add_test(NAME wheel_control_contract COMMAND wheel_control_tests)

# 单独核对算法的时间尺度和抗饱和行为，不用硬件模型冒充电机验收。
add_executable(pi_tests test_pi.c ${app_dir}/control/pid.c)
target_include_directories(pi_tests PRIVATE ${app_dir})
set_target_properties(pi_tests PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
target_compile_options(pi_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME pi_contract COMMAND pi_tests)
