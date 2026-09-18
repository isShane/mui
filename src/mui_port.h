/**
 * @file mui_port.h
 * @brief MUI 移植层接口（用户必须在本工程中实现以下四个函数）
 *
 * 移植层只负责"把像素写到屏幕"，所有坐标保证已经过裁剪：
 *   0 <= x < 屏宽, 0 <= y < 屏高, w >= 1, h >= 1
 * 颜色统一为 RGB565（16位，高字节在前需用户自行处理大小端）。
 */

#ifndef MUI_PORT_H
#define MUI_PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化底层显示设备（复位、SPI初始化、开显示等）
 */
void mui_port_init(void);

/**
 * @brief 画单个像素
 * @param x     横坐标（已裁剪，合法）
 * @param y     纵坐标（已裁剪，合法）
 * @param color RGB565 颜色
 */
void mui_port_draw_pixel(int16_t x, int16_t y, uint16_t color);

/**
 * @brief 填充矩形区域（性能关键，强烈建议用LCD开窗口+批量写实现）
 * @param x     左上角横坐标（已裁剪，合法）
 * @param y     左上角纵坐标（已裁剪，合法）
 * @param w     宽度（>= 1）
 * @param h     高度（>= 1）
 * @param color RGB565 颜色
 */
void mui_port_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief 批量写入多位图块（位图显示的性能关键，建议开窗口+连续写实现）
 * @param x      左上角横坐标（已裁剪，合法）
 * @param y      左上角纵坐标（已裁剪，合法）
 * @param w      块宽度（>= 1）
 * @param h      块高度（>= 1）
 * @param data   块左上角像素地址
 * @param stride 源图行宽（像素数，即下一行相对本行首的偏移，>= w）
 * @note  逐行写入：第 row 行数据在 data + row*stride，每行 w 个连续像素；
 *        行与行之间在源图中不连续，不可一次读 w*h 个
 */
void mui_port_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                          const uint16_t *data, int16_t stride);

#ifdef __cplusplus
}
#endif

#endif /* MUI_PORT_H */
