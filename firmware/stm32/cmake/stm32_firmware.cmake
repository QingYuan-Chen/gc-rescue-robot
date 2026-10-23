# 只注册当前阶段需要的源码；后续模块接入时显式扩展清单。
set(hal_dir Drivers/STM32H7xx_HAL_Driver)
set(freertos_dir Middlewares/Third_Party/FreeRTOS/Source)
set(linker_script ${CMAKE_CURRENT_SOURCE_DIR}/STM32H750VBTX_FLASH.ld)

add_executable(rescue.elf
    Core/Startup/startup_stm32h750vbtx.s
    Core/Src/main.c
    Core/Src/freertos.c
    Core/Src/gpio.c
    Core/Src/usart.c
    Core/Src/stm32h7xx_hal_msp.c
    Core/Src/stm32h7xx_hal_timebase_tim.c
    Core/Src/stm32h7xx_it.c
    Core/Src/syscalls.c
    Core/Src/sysmem.c
    Core/Src/system_stm32h7xx.c
    ${hal_dir}/Src/stm32h7xx_hal.c
    ${hal_dir}/Src/stm32h7xx_hal_cortex.c
    ${hal_dir}/Src/stm32h7xx_hal_gpio.c
    ${hal_dir}/Src/stm32h7xx_hal_rcc.c
    ${hal_dir}/Src/stm32h7xx_hal_rcc_ex.c
    ${hal_dir}/Src/stm32h7xx_hal_pwr.c
    ${hal_dir}/Src/stm32h7xx_hal_pwr_ex.c
    ${hal_dir}/Src/stm32h7xx_hal_flash.c
    ${hal_dir}/Src/stm32h7xx_hal_flash_ex.c
    ${hal_dir}/Src/stm32h7xx_hal_tim.c
    ${hal_dir}/Src/stm32h7xx_hal_tim_ex.c
    ${hal_dir}/Src/stm32h7xx_hal_dma.c
    ${hal_dir}/Src/stm32h7xx_hal_uart.c
    ${hal_dir}/Src/stm32h7xx_hal_uart_ex.c
    ${freertos_dir}/croutine.c
    ${freertos_dir}/event_groups.c
    ${freertos_dir}/list.c
    ${freertos_dir}/queue.c
    ${freertos_dir}/stream_buffer.c
    ${freertos_dir}/tasks.c
    ${freertos_dir}/timers.c
    ${freertos_dir}/CMSIS_RTOS_V2/cmsis_os2.c
    ${freertos_dir}/portable/GCC/ARM_CM4F/port.c
    ${freertos_dir}/portable/MemMang/heap_4.c
    ${linker_script}
)

set_target_properties(rescue.elf PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    LINK_DEPENDS ${linker_script}
)
target_include_directories(rescue.elf PRIVATE
    Core/Inc
    ${hal_dir}/Inc
    ${hal_dir}/Inc/Legacy
    Drivers/CMSIS/Device/ST/STM32H7xx/Include
    Drivers/CMSIS/Include
    ${freertos_dir}/include
    ${freertos_dir}/portable/GCC/ARM_CM4F
    ${freertos_dir}/CMSIS_RTOS_V2
)
target_compile_definitions(rescue.elf PRIVATE DEBUG USE_PWR_LDO_SUPPLY USE_HAL_DRIVER STM32H750xx)

# 沿用已验证的处理器、浮点 ABI 和各配置优化选项。
set(cpu_options -mcpu=cortex-m7 -mthumb -mthumb-interwork -mfpu=fpv5-d16 -mfloat-abi=hard)
target_compile_options(rescue.elf PRIVATE
    ${cpu_options}
    -ffunction-sections -fdata-sections -fno-common -fmessage-length=0
    $<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>
    $<$<CONFIG:Debug>:-Og;-g>
    $<$<CONFIG:Release>:-Ofast>
    $<$<CONFIG:RelWithDebInfo>:-Ofast;-g>
    $<$<CONFIG:MinSizeRel>:-Os>
)
target_link_options(rescue.elf PRIVATE
    ${cpu_options}
    -Wl,--gc-sections,--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/rescue.map
    -T${linker_script}
)

add_subdirectory(App)

add_custom_command(TARGET rescue.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:rescue.elf> ${PROJECT_BINARY_DIR}/rescue.hex
    COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:rescue.elf> ${PROJECT_BINARY_DIR}/rescue.bin
    BYPRODUCTS ${PROJECT_BINARY_DIR}/rescue.hex ${PROJECT_BINARY_DIR}/rescue.bin
    COMMENT "生成 HEX 和 BIN 固件"
    VERBATIM
)
