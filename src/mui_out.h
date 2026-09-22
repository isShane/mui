/**
 * @file mui_out.h
 * @brief MUI 内部输出层：按 src/mui_conf.h 的选择把绘制落到不同后端
 *
 * 这是库内部接口（只给 mui_gfx.c 用），不是移植层：
 *   - 直绘模式：用宏直接映射到 mui_port_*，零函数调用、零 Flash 开销；
 *   - 缓冲模式：函数在 mui_out.c 中实现，绘制只改内存，由 mui_out_flush() 推屏。
 * 入参约定与 mui_port.h 相同：坐标与宽高均已由 mui_gfx.c 裁剪为合法值。
 */

#ifndef MUI_OUT_H
#define MUI_OUT_H

#include <stdint.h>
#include "mui_conf.h"
#include "mui_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#if MUI_CFG_HAS_BUFFER

/** @brief 初始化输出层（缓冲复位；由 mui_init 调用） */
void mui_out_init(void);

/** @brief 写单个像素（已裁剪） */
void mui_out_draw_pixel(int16_t x, int16_t y, uint16_t color);

/** @brief 填充矩形（已裁剪） */
void mui_out_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t color);

/**
 * @brief 写入位图块（已裁剪）
 * @param stride 源图行宽（像素数）
 */
void mui_out_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                         const uint16_t *data, int16_t stride);

/** @brief 把本帧改动推送到屏幕（缓冲模式须在帧末调用） */
void mui_out_flush(void);

/** @brief 能否回读屏幕像素：缓冲后端 1（可读帧缓冲），直绘后端 0 */
#define MUI_OUT_CAN_READ_PIXEL  1

/**
 * @brief 回读当前像素（抗锯齿混底色用）
 *
 * 缓冲后端里帧缓冲就是"屏幕内容的镜像"，所以能直接读到边缘像素底下是什么，
 * 抗锯齿因此可以压在任何背景（图片/渐变）上；直绘后端没有这个能力。
 * @note 取不到时（越界）返回 0；缓冲里**从未绘制过**的区域是 0（不是屏幕真实内容），
 *       应用初始化时清一次屏即可保证缓冲与屏内容一致
 */
uint16_t mui_out_read_pixel(int16_t x, int16_t y);

#else /* 直绘：宏直连移植层，编译期消除 */

#define mui_out_init()                          mui_port_init()
#define mui_out_draw_pixel(x, y, c)             mui_port_draw_pixel((x), (y), (c))
#define mui_out_fill_rect(x, y, w, h, c)        mui_port_fill_rect((x), (y), (w), (h), (c))
#define mui_out_draw_bitmap(x, y, w, h, d, s)   mui_port_draw_bitmap((x), (y), (w), (h), (d), (s))
#define mui_out_flush()                         ((void)0)

/* 直绘模式屏幕只写不读（如 ST7735S 的 RD 常置高），回读做不到：
 * 抗锯齿等需要底色的地方改由调用方通过参数传入。 */
#define MUI_OUT_CAN_READ_PIXEL                  0
#define mui_out_read_pixel(x, y)                ((uint16_t)0)

#endif /* MUI_CFG_HAS_BUFFER */

#ifdef __cplusplus
}
#endif

#endif /* MUI_OUT_H */
