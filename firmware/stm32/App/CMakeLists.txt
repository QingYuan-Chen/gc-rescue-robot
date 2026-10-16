set(app_sources
    robot_app.c
    bsp/board_startup.c
    bsp/debug_uart.c
    bsp/motor.c
    bsp/encoder.c
    control/pid.c
    control/wheel_drive.c
    control/wheel_pid.c
    debug/debug_console.c
    debug/pid_commands.c
    debug/vofa_stream.c
    debug/wheel_commands.c
    debug/wheel_status.c
    runtime/runtime_monitor.c
)
target_sources(rescue.elf PRIVATE ${app_sources})
target_include_directories(rescue.elf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 严格检查新模块，不改变生成代码和第三方库的告警策略。
set_source_files_properties(${app_sources}
    TARGET_DIRECTORY rescue.elf
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror"
)
