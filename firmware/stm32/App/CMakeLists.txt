set(app_sources
    robot_app.c
    bsp/board_outputs.c
    bsp/debug_uart.c
    bsp/gimbal_test.c
    bsp/imu.c
    bsp/encoder.c
    bsp/motor.c
    bsp/time_us.c
    control/chassis_kinematics.c
    control/heading_control.c
    control/wheel_speed.c
    debug/chassis_commands.c
    debug/motor_commands.c
    debug/speed_commands.c
    runtime/chassis.c
    runtime/heading.c
    runtime/wheel_io.c
    debug/debug_console.c
    runtime/runtime_monitor.c
)
target_sources(rescue.elf PRIVATE ${app_sources})
target_include_directories(rescue.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 严格检查新模块，不改变生成代码和第三方库的告警策略。
set_source_files_properties(${app_sources}
    TARGET_DIRECTORY rescue.elf
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror"
)

# 有限数校验必须在 Release 中保留 IEEE 浮点语义。
set_property(SOURCE control/chassis_kinematics.c control/heading_control.c control/wheel_speed.c runtime/wheel_io.c runtime/chassis.c debug/chassis_commands.c debug/speed_commands.c
    TARGET_DIRECTORY rescue.elf APPEND PROPERTY COMPILE_OPTIONS -fno-fast-math)
