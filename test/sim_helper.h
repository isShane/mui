/**
 * @file sim_helper.h
 * @brief 测试辅助：模拟帧缓冲访问、越界违规计数、PNG 导出
 */

#ifndef SIM_HELPER_H
#define SIM_HELPER_H

#include <stdint.h>
#include "sim_config.h"

/** @brief 获取模拟帧缓冲首地址（可写） */
uint16_t *sim_get_fb(void);

/** @brief 获取累计越界违规次数 */
int sim_violations(void);

/** @brief 清零越界违规计数 */
void sim_clear_violations(void);

/** @brief 记录一次越界违规（供移植模拟层调用） */
void sim_record_violation(void);

/** @brief 清空模拟帧缓冲为黑色 */
void sim_clear_fb(void);

/**
 * @brief 导出帧缓冲为普通 PNG 图像（任意看图软件可直接打开）
 * @param path 输出文件路径
 */
void sim_export_png(const char *path);

#endif /* SIM_HELPER_H */
