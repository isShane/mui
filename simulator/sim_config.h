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

#define SIM_W 128  /**< 模拟屏宽 */
#define SIM_H 128  /**< 模拟屏高 */

#endif /* SIM_CONFIG_H */
