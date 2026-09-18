# MUI 项目构建脚本（MinGW/MSVC 均可，产物输出到 build\）
# 用法：
#   cmake -S . -B build\cmake
#   cmake --build build\cmake
# 之后直接运行 build\mui_sim.exe / build\mui_test.exe

cmake_minimum_required(VERSION 3.16)
project(mui C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# -------- 源文件自动收集（目录里新增 .c 文件自动编入，无需改本文件） --------
file(GLOB MUI_SRC  CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB APP_SRC  CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app/*.c)
file(GLOB SIM_SRC  CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/simulator/*.c)
file(GLOB TEST_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test/*.c)
file(GLOB ASSET_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.c)  # 图片资源

# 可执行文件统一输出到 build\
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

# -------- Win32 窗口模拟器（界面开发主入口） --------
add_executable(mui_sim ${MUI_SRC} ${APP_SRC} ${SIM_SRC} ${ASSET_SRC})
target_include_directories(mui_sim PRIVATE src app simulator assets)
target_link_libraries(mui_sim PRIVATE gdi32)
set_target_properties(mui_sim PROPERTIES WIN32_EXECUTABLE TRUE)  # 等效 -mwindows，无控制台黑窗

# -------- 单元测试（库几何/越界回归验证） --------
add_executable(mui_test ${MUI_SRC} ${TEST_SRC}
               ${CMAKE_CURRENT_SOURCE_DIR}/simulator/png_export.c)  # PNG 导出器与模拟器共用
target_include_directories(mui_test PRIVATE src simulator test)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mui_sim PRIVATE -Wall)
    target_compile_options(mui_test PRIVATE -Wall)
endif()
