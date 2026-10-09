# rescue/CMakeLists.txt

# ========== 1. 初始化变量（必须放在最开头） ==========
# 定义当前rescue目录的根路径，方便后续复用
set(RESCUE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
# 初始化源文件和头文件路径列表
set(RESCUE_SOURCES "")
set(RESCUE_INCLUDES "")

# ========== 2. 处理【rescue根目录】的文件（关键！解决common.h找不到） ==========
# 收集根目录下的所有 .c 文件（比如 sin_generate.c、turn.c）
file(GLOB RESCUE_ROOT_C_FILES ${RESCUE_ROOT}/*.c)
list(APPEND RESCUE_SOURCES ${RESCUE_ROOT_C_FILES})

# 把rescue根目录加入头文件搜索路径（核心！子目录的.h就能找到common.h）
list(APPEND RESCUE_INCLUDES ${RESCUE_ROOT})

# ========== 3. 处理所有一级子目录（Motor/、Time/、Rudder_control/） ==========
# 收集所有一级子目录
file(GLOB RESCUE_SUBDIRS LIST_DIRECTORIES true ${RESCUE_ROOT}/*)

foreach(DIR ${RESCUE_SUBDIRS})
    # 只处理目录，跳过文件
    if(IS_DIRECTORY ${DIR})
        # 收集该子目录下的所有 .c 文件
        file(GLOB SUBDIR_C_FILES ${DIR}/*.c)
        list(APPEND RESCUE_SOURCES ${SUBDIR_C_FILES})

        # 把子目录加入头文件搜索路径
        list(APPEND RESCUE_INCLUDES ${DIR})
    endif()
endforeach()

# ========== 4. 把收集到的内容添加到主工程 ==========
# 添加所有源文件到目标
target_sources(${PROJECT_NAME}.elf PRIVATE ${RESCUE_SOURCES})

# 添加所有头文件路径到目标
target_include_directories(${PROJECT_NAME}.elf PRIVATE ${RESCUE_INCLUDES})

#在add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT}) 下面添加 add_subdirectory(gc_rescue)
#在所有-mcpu=cortex-m7 -mthumb -mthumb-interwork 后面添加 -mfpu=fpv5-d16 -mfloat-abi=hard
#还要把h7xx_it.c里的USART2_IRQHandler 和 USART3_IRQHandler注释掉
