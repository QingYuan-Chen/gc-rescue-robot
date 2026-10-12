# 原工程参考模块的源文件与头文件目录注册。

# 保存参考模块根目录，供下方文件收集使用。
set(RESCUE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
# 初始化源文件和头文件路径列表
set(RESCUE_SOURCES "")
set(RESCUE_INCLUDES "")

# 收集根目录中的业务源文件，当前包含 car_task.c。
file(GLOB RESCUE_ROOT_C_FILES ${RESCUE_ROOT}/*.c)
list(APPEND RESCUE_SOURCES ${RESCUE_ROOT_C_FILES})

# 将 common.h 所在目录加入参考模块的头文件搜索路径。
list(APPEND RESCUE_INCLUDES ${RESCUE_ROOT})

# 保留原工程一级子目录的自动收集方式；新模块采用显式源文件列表。
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

# 将参考模块的源文件注册到现有固件目标。
target_sources(${PROJECT_NAME}.elf PRIVATE ${RESCUE_SOURCES})

# 将参考模块的头文件目录注册到现有固件目标。
target_include_directories(${PROJECT_NAME}.elf PRIVATE ${RESCUE_INCLUDES})

# 主工程已通过 add_subdirectory(gc_rescue) 接入本目录。
# USART2/USART3 的有效中断入口分别保留在 maixcam.c/gyro.c 中。
