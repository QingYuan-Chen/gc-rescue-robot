# 与 CLion 模板共用构建定义，避免重新导入 CubeMX 后丢失应用模块。
cmake_minimum_required(VERSION 3.28)
include(cmake/stm32_toolchain.cmake)

# 当前 Windows ARM 工具链的编译器发现过程需要此兼容策略。
cmake_policy(PUSH)
cmake_policy(SET CMP0125 OLD)
project(rescue C ASM)
cmake_policy(POP)

include(cmake/stm32_firmware.cmake)
