/**
 * @file mui_out.c
 * @brief MUI 输出层实现：全屏单缓冲 / 全屏双缓冲
 *
 * 直绘模式（MUI_OUTPUT_DIRECT）下本文件编译为空 —— 接口在 mui_out.h 里直接
 * 映射到 mui_port_*，不产生任何代码与数据。
 *
 * 两种缓冲后端的共同前提：屏幕只写不读，缓冲里"本次没写过的像素"是上一批
 * 数据的残留，绝不能推上屏；因此每个后端都只推"本轮真正写过的像素"。
 *
 * 推送统一走 mui_port_draw_bitmap（一次开窗 + 连续写），两种移植层零改动。
 *
 * 双缓冲说明：屏只有一块 GRAM、无基址切换时，双缓冲并不能消除撕裂（撕裂来自
 * 写 GRAM 的过程被显示扫描看到），它只是把"屏上出现中间状态"的窗口缩短到一次
 * 整帧推送；又因 MUI 是增量绘制，每帧还要一次整屏拷贝让后台缓冲与屏内容对齐。
 * 有 DMA 的平台可把整帧推送改为异步，并用双缓冲消除等待。
 */

#include "mui_out.h"

#if MUI_CFG_HAS_BUFFER

#include <string.h>
#include "mui.h"

/** @brief 缓冲行距（定长 = 宽上限，与运行期宽度解耦，地址计算最简单） */
#define OUT_STRIDE  ((int16_t)MUI_CFG_BUF_MAX_W)

/* -------- 各后端的缓冲与状态 -------- */

#if MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL
static uint16_t s_fb[MUI_CFG_BUF_MAX_W * MUI_CFG_BUF_MAX_H];
#if MUI_CFG_DIRTY
static uint8_t  s_dirty;                /**< 脏区是否有效 */
static int16_t  s_dx0, s_dy0, s_dx1, s_dy1;  /**< 脏包围盒（半开区间） */
#endif

#else /* MUI_OUTPUT_FULL_DOUBLE */
static uint16_t s_fb[2][MUI_CFG_BUF_MAX_W * MUI_CFG_BUF_MAX_H];
static uint16_t *s_back = s_fb[0];      /**< 后台缓冲：绘制目标 */
static uint16_t *s_front = s_fb[1];     /**< 前台缓冲：刚推给屏的那块 */
#endif

/* 绘制目标缓冲：双缓冲下是后台缓冲，其余后端就是唯一那块缓冲 */
#if MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL_DOUBLE
#define OUT_BUF     s_back
#else
#define OUT_BUF     s_fb
#endif

static int16_t s_w = 0;                 /**< 运行期有效宽度（<= 缓冲上限） */
static int16_t s_h = 0;                 /**< 运行期有效高度 */

/* -------- 内部：全屏后端 -------- */

/** @brief 内部：把缓冲中指定屏幕矩形推给移植层 */
static void out_push(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (w < 1 || h < 1) {
        return;
    }
#if MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL
    mui_port_draw_bitmap(x, y, w, h, &s_fb[(size_t)y * OUT_STRIDE + x],
                         OUT_STRIDE);
#else
    mui_port_draw_bitmap(x, y, w, h, &s_back[(size_t)y * OUT_STRIDE + x],
                         OUT_STRIDE);
#endif
}

#if (MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL) && MUI_CFG_DIRTY

/** @brief 内部：把矩形并入脏包围盒 */
static void out_dirty_add(int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (w < 1 || h < 1) {
        return;
    }
    if (!s_dirty) {
        s_dx0 = x;
        s_dy0 = y;
        s_dx1 = (int16_t)(x + w);
        s_dy1 = (int16_t)(y + h);
        s_dirty = 1;
        return;
    }
    if (x < s_dx0) {
        s_dx0 = x;
    }
    if (y < s_dy0) {
        s_dy0 = y;
    }
    if ((int16_t)(x + w) > s_dx1) {
        s_dx1 = (int16_t)(x + w);
    }
    if ((int16_t)(y + h) > s_dy1) {
        s_dy1 = (int16_t)(y + h);
    }
}

/** @brief 内部：推送脏包围盒并清空（无脏区则什么都不做） */
static void out_push_dirty(void)
{
    if (!s_dirty) {
        return;
    }
    out_push(s_dx0, s_dy0, (int16_t)(s_dx1 - s_dx0), (int16_t)(s_dy1 - s_dy0));
    s_dirty = 0;
}

#endif /* FULL 且开脏区 */

/* -------- 输出层接口 -------- */

void mui_out_init(void)
{
    /* 初始化显示设备：直绘模式下 mui_out.h 把 mui_out_init() 宏映射为
     * mui_port_init()；缓冲模式下走到本函数，若不显式调用 mui_port_init，
     * LCD 将永远不会被初始化（引脚未配置、复位/初始化表未发）→ 上电白屏。 */
    mui_port_init();

    s_w = mui_screen_get_width();
    s_h = mui_screen_get_height();
    if (s_w > (int16_t)MUI_CFG_BUF_MAX_W) {
        s_w = (int16_t)MUI_CFG_BUF_MAX_W;
    }
    if (s_h > (int16_t)MUI_CFG_BUF_MAX_H) {
        s_h = (int16_t)MUI_CFG_BUF_MAX_H;
    }
#if (MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL) && MUI_CFG_DIRTY
    s_dirty = 0;
#endif
}

void mui_out_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    OUT_BUF[(size_t)y * OUT_STRIDE + x] = color;
#if MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL
#if MUI_CFG_DIRTY
    out_dirty_add(x, y, 1, 1);
#endif
#endif
}

void mui_out_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                       uint16_t color)
{
    int16_t row, col;
    int16_t y_end;

    if (w < 1 || h < 1) {
        return;
    }
    y_end = (int16_t)(y + h);

    for (row = y; row < y_end; row++) {
        uint16_t *dst = &OUT_BUF[(size_t)row * OUT_STRIDE + x];

        for (col = 0; col < w; col++) {
            dst[col] = color;
        }
    }
#if MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL
#if MUI_CFG_DIRTY
    out_dirty_add(x, y, w, h);
#endif
#endif
}

void mui_out_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                         const uint16_t *data, int16_t stride)
{
    int16_t row;
    int16_t y0 = y;
    int16_t y_end;

    if (w < 1 || h < 1 || data == NULL) {
        return;
    }
    y_end = (int16_t)(y + h);

    for (row = y; row < y_end; row++) {
        memcpy(&OUT_BUF[(size_t)row * OUT_STRIDE + x],
               data + ((size_t)(row - y0) * (size_t)stride),
               (size_t)w * sizeof(uint16_t));
    }
#if MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL
#if MUI_CFG_DIRTY
    out_dirty_add(x, y, w, h);
#endif
#endif
}

uint16_t mui_out_read_pixel(int16_t x, int16_t y)
{
    if (x < 0 || y < 0 || x >= s_w || y >= s_h) {
        return 0;
    }
    return OUT_BUF[(size_t)y * OUT_STRIDE + x];
}

void mui_out_flush(void)
{
    if (s_w < 1 || s_h < 1) {
        return;
    }

#if MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL
#if MUI_CFG_DIRTY
    out_push_dirty();
#else
    out_push(0, 0, s_w, s_h);   /* 关脏区则整块推屏 */
#endif

#else /* MUI_OUTPUT_FULL_DOUBLE */
    {
        uint16_t *tmp;

        /* ① 整帧推后台缓冲；② 交换前后台；③ 让新后台与屏内容一致（供下一帧增量绘制） */
        out_push(0, 0, s_w, s_h);
        tmp = s_front;
        s_front = s_back;
        s_back = tmp;
        memcpy(s_back, s_front,
               (size_t)OUT_STRIDE * (size_t)s_h * sizeof(uint16_t));
    }
#endif
}

/* -------- 对外公共接口（mui.h 声明的 mui_screen_flush） -------- */

void mui_screen_flush(void)
{
    mui_out_flush();
}

#endif /* MUI_CFG_HAS_BUFFER */
