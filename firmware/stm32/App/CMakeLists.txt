set(app_sources
    robot_app.c
    bsp/board_startup.c
    bsp/debug_uart.c
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
