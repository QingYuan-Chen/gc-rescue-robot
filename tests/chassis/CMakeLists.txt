cmake_minimum_required(VERSION 3.20)
project(chassis_check C)
enable_testing()

set(app_dir ../../firmware/stm32/App)

add_executable(heading_check
    test_heading.c
    ${app_dir}/runtime/heading.c)
target_include_directories(heading_check PRIVATE stubs ${app_dir})
target_compile_features(heading_check PRIVATE c_std_11)
target_compile_options(heading_check PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(heading_check PRIVATE m)
add_test(NAME heading_check COMMAND heading_check)

add_executable(kinematics_check
    test_kinematics.c
    ${app_dir}/control/chassis_kinematics.c)
target_include_directories(kinematics_check PRIVATE stubs ${app_dir})
target_compile_features(kinematics_check PRIVATE c_std_11)
target_compile_options(kinematics_check PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(kinematics_check PRIVATE m)
add_test(NAME kinematics_check COMMAND kinematics_check)

add_executable(chassis_check
    test_chassis.c
    ${app_dir}/runtime/chassis.c
    ${app_dir}/control/chassis_kinematics.c
    ${app_dir}/control/heading_control.c)
target_include_directories(chassis_check PRIVATE stubs ${app_dir})
target_compile_features(chassis_check PRIVATE c_std_11)
target_compile_options(chassis_check PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(chassis_check PRIVATE m)
add_test(NAME chassis_check COMMAND chassis_check)

add_executable(heading_control_check
    test_heading_control.c
    ${app_dir}/control/heading_control.c)
target_include_directories(heading_control_check PRIVATE stubs ${app_dir})
target_compile_features(heading_control_check PRIVATE c_std_11)
target_compile_options(heading_control_check PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(heading_control_check PRIVATE m)
add_test(NAME heading_control_check COMMAND heading_control_check)

add_executable(wheel_io_chassis_check
    test_wheel_io_chassis.c
    ${app_dir}/runtime/wheel_io.c
    ${app_dir}/control/wheel_speed.c)
target_include_directories(wheel_io_chassis_check PRIVATE stubs ${app_dir})
target_compile_features(wheel_io_chassis_check PRIVATE c_std_11)
target_compile_options(wheel_io_chassis_check PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(wheel_io_chassis_check PRIVATE m)
add_test(NAME wheel_io_chassis_check COMMAND wheel_io_chassis_check)

add_executable(chassis_commands_check
    test_chassis_commands.c
    ${app_dir}/debug/chassis_commands.c
    ${app_dir}/runtime/chassis.c
    ${app_dir}/control/chassis_kinematics.c
    ${app_dir}/control/heading_control.c)
target_include_directories(chassis_commands_check PRIVATE stubs ${app_dir})
target_compile_features(chassis_commands_check PRIVATE c_std_11)
target_compile_options(chassis_commands_check PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(chassis_commands_check PRIVATE m)
add_test(NAME chassis_commands_check COMMAND chassis_commands_check)
