/**
 * @file sim_config.h
 * @brief PC 模拟统一分辨率配置（单元测试与 Win32 模拟器共用）
 *
 * 修改分辨率只需要改这里，重新编译即可：
 *   gcc ... test/mui_test.exe   -> PPM 测试程序
 *   gcc ... test/mui_sim.exe    -> Win32 窗口模拟器
 * 注意：宽度/高度过小会导致部分测试用例断言失败（属预期行为）。
 */

#ifndef SIM_CONFIG_H
#define SIM_CONFIG_H

#define SIM_W 1024 /**< 模拟屏宽 */
#define SIM_H 600  /**< 模拟屏高 */

/* -------- 编译期校验：缓冲后端必须装得下这块屏 -------- */
#include "mui_conf.h"

#if MUI_CFG_HAS_BUFFER && (SIM_W > MUI_CFG_BUF_MAX_W)
#error "缓冲后端要求 MUI_CFG_BUF_MAX_W >= SIM_W；请同步 CMakeLists.txt 里的 MUI_SIM_BUF_W"
#endif

#if MUI_CFG_HAS_BUFFER && !MUI_CFG_IS_STRIP && (SIM_H > MUI_CFG_BUF_MAX_H)
#error "全屏缓冲后端要求 MUI_CFG_BUF_MAX_H >= SIM_H；请同步 CMakeLists.txt 里的 MUI_SIM_BUF_H"
#endif

#endif /* SIM_CONFIG_H */
