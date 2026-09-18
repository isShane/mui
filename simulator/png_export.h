/**
 * @file png_export.h
 * @brief 零依赖 PNG 导出（PC 侧工具，不进单片机）
 */

#ifndef PNG_EXPORT_H
#define PNG_EXPORT_H

#include <stdint.h>

/**
 * @brief 将 RGB565 像素缓冲导出为 24 位 PNG 文件
 * @param path  输出文件路径（如 "build/test_output.png"）
 * @param px    像素缓冲（行优先，w*h 个 RGB565）
 * @param w     宽（像素）
 * @param h     高（像素）
 * @return 0 成功；-1 文件创建失败；-2 内存不足
 */
int png_export_rgb565(const char *path, const uint16_t *px, int w, int h);

#endif /* PNG_EXPORT_H */
