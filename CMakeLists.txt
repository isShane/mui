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

# -------- 模拟分辨率（缓冲后端须与之一致） --------
# 必须与 simulator/sim_config.h 的 SIM_W/SIM_H 相同：缓冲后端按 MUI_CFG_BUF_MAX_W/H
# 静态分配帧缓冲，若小于屏幕尺寸，mui_init() 会把屏幕静默钳制到该尺寸。
# 两边不同步会触发 run-time 钳制；sim_config.h 里有编译期校验直接报错。
set(MUI_SIM_BUF_W 1024)
set(MUI_SIM_BUF_H 600)

# -------- Win32 窗口模拟器（界面开发主入口） + 单元测试（越界回归验证） --------
# 每套输出后端各生成一对可执行文件，便于逐一验证三种后端行为一致：
#   名称     MUI_CFG_OUTPUT_MODE   产物
#   direct   0（默认，直绘）        build\mui_sim.exe      / build\mui_test.exe
#   full     1（全屏单缓冲）        build\mui_sim_full.exe  / build\mui_test_full.exe
#   double   2（全屏双缓冲）        build\mui_sim_double.exe / build\mui_test_double.exe
function(mui_add_targets mode_id suffix)
    add_executable(mui_sim${suffix} ${MUI_SRC} ${APP_SRC} ${SIM_SRC} ${ASSET_SRC})
    target_include_directories(mui_sim${suffix} PRIVATE src app simulator simulator/pc_bsp assets)
    target_compile_definitions(mui_sim${suffix} PRIVATE
        MUI_CFG_OUTPUT_MODE=${mode_id}
        MUI_CFG_BUF_MAX_W=${MUI_SIM_BUF_W}
        MUI_CFG_BUF_MAX_H=${MUI_SIM_BUF_H})
    target_link_libraries(mui_sim${suffix} PRIVATE gdi32)
    set_target_properties(mui_sim${suffix} PROPERTIES WIN32_EXECUTABLE TRUE)  # 等效 -mwindows，无控制台黑窗

    add_executable(mui_test${suffix} ${MUI_SRC} ${TEST_SRC}
                   ${CMAKE_CURRENT_SOURCE_DIR}/simulator/png_export.c)  # PNG 导出器与模拟器共用
    target_include_directories(mui_test${suffix} PRIVATE src simulator test)
    target_compile_definitions(mui_test${suffix} PRIVATE
        MUI_CFG_OUTPUT_MODE=${mode_id}
        MUI_CFG_BUF_MAX_W=${MUI_SIM_BUF_W}
        MUI_CFG_BUF_MAX_H=${MUI_SIM_BUF_H})

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(mui_sim${suffix} PRIVATE -Wall)
        target_compile_options(mui_test${suffix} PRIVATE -Wall)
    endif()
endfunction()

mui_add_targets(0 "")
mui_add_targets(1 "_full")
mui_add_targets(2 "_double")

# -------- 低配对照：关闭全局抗锯齿（MUI_CFG_AA=0）的测试目标 --------
# 验证"关 AA → 全部走硬边"分支能编译且不依赖 AA 的断言仍然通过
add_executable(mui_test_noaa ${MUI_SRC} ${TEST_SRC}
               ${CMAKE_CURRENT_SOURCE_DIR}/simulator/png_export.c)
target_include_directories(mui_test_noaa PRIVATE src simulator test)
target_compile_definitions(mui_test_noaa PRIVATE MUI_CFG_OUTPUT_MODE=0 MUI_CFG_AA=0)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mui_test_noaa PRIVATE -Wall)
endif()

# -------- 低配对照：关闭脏矩形跟踪（MUI_CFG_DIRTY_N=0）的测试目标 --------
# 验证关闭时列表数组不占 RAM、且 mui_dirty_* API 仍可编译并恒返回 0
add_executable(mui_test_nodirty ${MUI_SRC} ${TEST_SRC}
               ${CMAKE_CURRENT_SOURCE_DIR}/simulator/png_export.c)
target_include_directories(mui_test_nodirty PRIVATE src simulator test)
target_compile_definitions(mui_test_nodirty PRIVATE MUI_CFG_OUTPUT_MODE=0 MUI_CFG_DIRTY_N=0)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mui_test_nodirty PRIVATE -Wall)
endif()

# -------- 控件总览 demo（demo\demo_ui.c）：与正式界面 mui_sim 互不干扰 --------
# 用 simulator 宿主但不绑 app_ui.c，故剔除 sim_app_bind.c，改用 demo\demo_bind.c 装配
file(GLOB DEMO_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/demo/*.c)
set(DEMO_UI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/demo/demo_ui.c)
set(DEMO_SIM_SRC ${SIM_SRC})
list(REMOVE_ITEM DEMO_SIM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/simulator/sim_app_bind.c)
# 无头截图入口只给 mui_demo_shot 用，不编进窗口版 demo（否则入口/符号重复）
list(REMOVE_ITEM DEMO_SRC ${CMAKE_CURRENT_SOURCE_DIR}/demo/demo_shot_main.c)
add_executable(mui_demo ${MUI_SRC} ${DEMO_SRC} ${DEMO_SIM_SRC} ${ASSET_SRC})
target_include_directories(mui_demo PRIVATE src demo simulator simulator/pc_bsp assets)
target_compile_definitions(mui_demo PRIVATE MUI_CFG_OUTPUT_MODE=0)  # 直绘后端（与实机一致）
target_link_libraries(mui_demo PRIVATE gdi32)
set_target_properties(mui_demo PROPERTIES WIN32_EXECUTABLE TRUE)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mui_demo PRIVATE -Wall)
endif()

# -------- demo 无头截图：不开窗口，把 demo 界面渲染成 PNG（核对布局 / 无 GUI 环境看效果） --------
add_executable(mui_demo_shot ${MUI_SRC} ${DEMO_UI_SRC} ${ASSET_SRC}
               ${CMAKE_CURRENT_SOURCE_DIR}/demo/demo_shot_main.c
               ${CMAKE_CURRENT_SOURCE_DIR}/test/mui_port_sim.c
               ${CMAKE_CURRENT_SOURCE_DIR}/test/sim_helper.c
               ${CMAKE_CURRENT_SOURCE_DIR}/simulator/png_export.c)
target_include_directories(mui_demo_shot PRIVATE src demo simulator simulator/pc_bsp test assets)
target_compile_definitions(mui_demo_shot PRIVATE MUI_CFG_OUTPUT_MODE=0)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mui_demo_shot PRIVATE -Wall)
endif()

# ================= 条带缓冲后端（MUI_CFG_OUTPUT_MODE=3） =================
# RAM 只留一条横带 = 宽 × 带高，同一份界面代码按带重放 N 遍（见 mui_screen_frame）。
# 与直绘目标共用界面源码，只换后端，便于"同界面、两种后端"逐像素对照。
set(MUI_STRIP_H 32)

# 1) 窗口版：可直接跑起来交互（1024x32x2 = 64KB 缓冲，PC 上无所谓）
add_executable(mui_demo_strip ${MUI_SRC} ${DEMO_SRC} ${DEMO_SIM_SRC} ${ASSET_SRC})
target_include_directories(mui_demo_strip PRIVATE src demo simulator simulator/pc_bsp assets)
target_compile_definitions(mui_demo_strip PRIVATE
    MUI_CFG_OUTPUT_MODE=3
    MUI_CFG_BUF_MAX_W=${MUI_SIM_BUF_W}
    MUI_CFG_BUF_MAX_H=${MUI_SIM_BUF_H}
    MUI_CFG_STRIP_H=${MUI_STRIP_H})
target_link_libraries(mui_demo_strip PRIVATE gdi32)
set_target_properties(mui_demo_strip PROPERTIES WIN32_EXECUTABLE TRUE)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mui_demo_strip PRIVATE -Wall)
endif()

# 2) 条带截图：产物另存 demo_shot_strip.png，便于与直绘的 demo_shot.png 对比
add_executable(mui_demo_shot_strip ${MUI_SRC} ${DEMO_UI_SRC} ${ASSET_SRC}
               ${CMAKE_CURRENT_SOURCE_DIR}/demo/demo_shot_main.c
               ${CMAKE_CURRENT_SOURCE_DIR}/test/mui_port_sim.c
               ${CMAKE_CURRENT_SOURCE_DIR}/test/sim_helper.c
               ${CMAKE_CURRENT_SOURCE_DIR}/simulator/png_export.c)
target_include_directories(mui_demo_shot_strip PRIVATE src demo simulator simulator/pc_bsp test assets)
target_compile_definitions(mui_demo_shot_strip PRIVATE
    MUI_CFG_OUTPUT_MODE=3
    MUI_CFG_BUF_MAX_W=${MUI_SIM_BUF_W}
    MUI_CFG_STRIP_H=${MUI_STRIP_H}
    MUI_DEMO_SHOT_NAME="demo_shot_strip.png")
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mui_demo_shot_strip PRIVATE -Wall)
endif()

# 3) 回归测试：带高 1~32 与上限逐像素一致 + 重放幂等 + 帧外 set_* 不写屏 + 零越界
add_executable(mui_test_strip ${MUI_SRC}
               ${CMAKE_CURRENT_SOURCE_DIR}/test/strip/test_strip_main.c
               ${CMAKE_CURRENT_SOURCE_DIR}/test/mui_port_sim.c
               ${CMAKE_CURRENT_SOURCE_DIR}/test/sim_helper.c
               ${CMAKE_CURRENT_SOURCE_DIR}/simulator/png_export.c)
target_include_directories(mui_test_strip PRIVATE src simulator test)
target_compile_definitions(mui_test_strip PRIVATE
    MUI_CFG_OUTPUT_MODE=3
    MUI_CFG_BUF_MAX_W=${MUI_SIM_BUF_W}
    MUI_CFG_STRIP_H=${MUI_STRIP_H})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mui_test_strip PRIVATE -Wall)
endif()
