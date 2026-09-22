/**
 * @file mui_gfx.c
 * @brief MUI 图形库核心实现
 *
 * 全部使用整数运算（无浮点、无动态内存），可直接在裸机环境运行。
 */

#include "mui.h"
#include "mui_out.h"    /* 输出层：按 mui_conf.h 选择直绘或缓冲后端 */

/* -------- 内部状态 -------- */

static int16_t mui_screen_w = 0;  /**< 屏幕宽度 */
static int16_t mui_screen_h = 0;  /**< 屏幕高度 */

/* -------- 内部工具 -------- */

/**
 * @brief 整数平方根（逐位法：只用移位/比较/减法，无除法、无浮点）
 * @param n 输入值（n <= 0 时返回 0）
 * @return 不超过 sqrt(n) 的最大整数
 * @note  早期版本用牛顿迭代（每轮一次 32 位除法），在无硬件除法的 M0 上约 900 周期/次；
 *        逐位法约 100~150 周期，圆/椭圆填充与抗锯齿都能受益，且结果同为 floor(sqrt(n))
 */
static uint32_t mui_isqrt(uint32_t n)
{
    uint32_t res = 0;
    uint32_t bit = (uint32_t)1 << 30;

    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

/**
 * @brief 沿边做定点插值，计算 y 行处边上的 x 坐标（<<8 定点）
 * @param x0 边起点横坐标
 * @param y0 边起点纵坐标
 * @param x1 边终点横坐标
 * @param y1 边终点纵坐标（须 y1 != y0，且 y 在 [y0,y1] 范围内）
 * @param y  目标行
 * @return 定点 x（实际值需右移8位）
 */
static int32_t mui_edge_interp(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               int16_t y)
{
    return ((int32_t)(x1 - x0) * (y - y0) * 256) / (y1 - y0)
           + ((int32_t)x0 << 8);
}

/* -------- 初始化 -------- */

void mui_init(int16_t screen_w, int16_t screen_h)
{
    if (screen_w < 1) {
        screen_w = 1;
    }
    if (screen_h < 1) {
        screen_h = 1;
    }
#if MUI_CFG_HAS_BUFFER
    /* 缓冲后端按配置的尺寸上限静态分配：运行期尺寸超过它必须钳制，防越界 */
    if (screen_w > (int16_t)MUI_CFG_BUF_MAX_W) {
        screen_w = (int16_t)MUI_CFG_BUF_MAX_W;
    }
    if (screen_h > (int16_t)MUI_CFG_BUF_MAX_H) {
        screen_h = (int16_t)MUI_CFG_BUF_MAX_H;
    }
#endif
    mui_screen_w = screen_w;
    mui_screen_h = screen_h;
    mui_out_init();
}

int16_t mui_screen_get_width(void)
{
    return mui_screen_w;
}

int16_t mui_screen_get_height(void)
{
    return mui_screen_h;
}

/* -------- 基础图元 -------- */

void mui_pixel_draw(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= mui_screen_w || y >= mui_screen_h) {
        return;
    }
    mui_out_draw_pixel(x, y, color);
}

void mui_screen_clear(uint16_t color)
{
    mui_rect_fill(0, 0, mui_screen_w, mui_screen_h, color);
}

void mui_rect_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    int32_t x2;
    int32_t y2;

    if (w < 1 || h < 1) {
        return;
    }
    x2 = (int32_t)x + w;
    y2 = (int32_t)y + h;
    /* -------- 裁剪到屏幕范围（int32 计算防止溢出） -------- */
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x2 > mui_screen_w) {
        x2 = mui_screen_w;
    }
    if (y2 > mui_screen_h) {
        y2 = mui_screen_h;
    }
    if (x2 - x < 1 || y2 - y < 1) {
        return;
    }
    mui_out_fill_rect(x, y, (int16_t)(x2 - x), (int16_t)(y2 - y), color);
}

void mui_hline_draw(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    mui_rect_fill(x, y, w, 1, color);
}

void mui_vline_draw(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    mui_rect_fill(x, y, 1, h, color);
}

void mui_line_draw(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint16_t color)
{
    int16_t dx;
    int16_t dy;
    int16_t sx;
    int16_t sy;
    int16_t err;

    /* -------- 特殊走向直接走矩形填充（更快） -------- */
    if (y0 == y1) {
        if (x0 > x1) {
            dx = x0; x0 = x1; x1 = dx;
        }
        mui_hline_draw(x0, y0, (int16_t)(x1 - x0 + 1), color);
        return;
    }
    if (x0 == x1) {
        if (y0 > y1) {
            dy = y0; y0 = y1; y1 = dy;
        }
        mui_vline_draw(x0, y0, (int16_t)(y1 - y0 + 1), color);
        return;
    }

    /* -------- Bresenham 通用直线 -------- */
    dx = (int16_t)((x1 > x0) ? (x1 - x0) : (x0 - x1));
    dy = (int16_t)((y1 > y0) ? (y1 - y0) : (y0 - y1));
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = (int16_t)(dx - dy);

    for (;;) {
        mui_pixel_draw(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int16_t e2 = (int16_t)(err * 2);
            if (e2 > -dy) {
                err = (int16_t)(err - dy);
                x0 = (int16_t)(x0 + sx);
            }
            if (e2 < dx) {
                err = (int16_t)(err + dx);
                y0 = (int16_t)(y0 + sy);
            }
        }
    }
}

/* -------- 矩形 -------- */

void mui_rect_draw(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (w < 1 || h < 1) {
        return;
    }
    mui_hline_draw(x, y, w, color);              /* 上边 */
    mui_hline_draw(x, (int16_t)(y + h - 1), w, color); /* 下边 */
    if (h > 2) {
        mui_vline_draw(x, (int16_t)(y + 1), (int16_t)(h - 2), color);       /* 左边 */
        mui_vline_draw((int16_t)(x + w - 1), (int16_t)(y + 1), (int16_t)(h - 2), color); /* 右边 */
    }
}

void mui_round_rect_fill(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t r, uint16_t color)
{
    int16_t i;
    int16_t limit;

    if (w < 1 || h < 1) {
        return;
    }
    /* -------- 圆角半径限制到短边的一半 -------- */
    limit = (w < h) ? (int16_t)(w / 2) : (int16_t)(h / 2);
    if (r > limit) {
        r = limit;
    }
    if (r < 0) {
        r = 0;
    }
    if (r == 0) {
        mui_rect_fill(x, y, w, h, color);
        return;
    }

    /* -------- 主体：中间竖条 + 左右条 -------- */
    mui_rect_fill((int16_t)(x + r), y, (int16_t)(w - 2 * r), h, color);
    mui_rect_fill(x, (int16_t)(y + r), r, (int16_t)(h - 2 * r), color);
    mui_rect_fill((int16_t)(x + w - r), (int16_t)(y + r), r,
                  (int16_t)(h - 2 * r), color);

    /* -------- 四角：逐行画 1/4 圆盘水平跨度 -------- */
    /* 角圆心：LT(x+r,y+r) RT(x+w-1-r,y+r) LB(x+r,y+h-1-r) RB(x+w-1-r,y+h-1-r) */
    for (i = 0; i < r; i++) {
        int16_t d = (int16_t)(r - i);   /* 该行到角圆心的垂直距离 */
        uint32_t r_sq = (uint32_t)r * (uint32_t)r;
        uint32_t d_sq = (uint32_t)d * (uint32_t)d;
        int16_t dx = (int16_t)mui_isqrt(r_sq - d_sq);
        /* 象限含圆心列，跨度宽 dx+1，保证与圆盘行宽 2*dx+1 一致 */
        mui_hline_draw((int16_t)(x + r - dx), (int16_t)(y + i),
                       (int16_t)(dx + 1), color);                            /* 左上 */
        mui_hline_draw((int16_t)(x + w - 1 - r), (int16_t)(y + i),
                       (int16_t)(dx + 1), color);                            /* 右上 */
        mui_hline_draw((int16_t)(x + r - dx), (int16_t)(y + h - 1 - i),
                       (int16_t)(dx + 1), color);                            /* 左下 */
        mui_hline_draw((int16_t)(x + w - 1 - r), (int16_t)(y + h - 1 - i),
                       (int16_t)(dx + 1), color);                            /* 右下 */
    }
}

/**
 * @brief 画圆弧（中点圆算法，按象限掩码筛选绘制点）
 * @param cx      圆心横坐标
 * @param cy      圆心纵坐标
 * @param r       半径
 * @param quad    象限掩码：bit0 左上 bit1 右上 bit2 右下 bit3 左下（0xFF 画整圆）
 * @param color   RGB565 颜色
 */
static void mui_circle_draw_arc(int16_t cx, int16_t cy, int16_t r,
                                uint8_t quad, uint16_t color)
{
    int16_t px;
    int16_t py;
    int16_t err;

    if (r < 0) {
        return;
    }
    px = r;
    py = 0;
    err = (int16_t)(1 - r);

    while (px >= py) {
        /* 八对称点按所在象限筛选 */
        if (quad & 0x01) { /* 左上 */
            mui_pixel_draw((int16_t)(cx - px), (int16_t)(cy - py), color);
            mui_pixel_draw((int16_t)(cx - py), (int16_t)(cy - px), color);
        }
        if (quad & 0x02) { /* 右上 */
            mui_pixel_draw((int16_t)(cx + px), (int16_t)(cy - py), color);
            mui_pixel_draw((int16_t)(cx + py), (int16_t)(cy - px), color);
        }
        if (quad & 0x04) { /* 右下 */
            mui_pixel_draw((int16_t)(cx + px), (int16_t)(cy + py), color);
            mui_pixel_draw((int16_t)(cx + py), (int16_t)(cy + px), color);
        }
        if (quad & 0x08) { /* 左下 */
            mui_pixel_draw((int16_t)(cx - px), (int16_t)(cy + py), color);
            mui_pixel_draw((int16_t)(cx - py), (int16_t)(cy + px), color);
        }
        py++;
        if (err < 0) {
            err = (int16_t)(err + 2 * py + 1);
        } else {
            px--;
            err = (int16_t)(err + 2 * (py - px) + 1);
        }
    }
}

void mui_round_rect_draw(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t r, uint16_t color)
{
    int16_t limit;

    if (w < 1 || h < 1) {
        return;
    }
    limit = (w < h) ? (int16_t)(w / 2) : (int16_t)(h / 2);
    if (r > limit) {
        r = limit;
    }
    if (r < 0) {
        r = 0;
    }
    if (r == 0) {
        mui_rect_draw(x, y, w, h, color);
        return;
    }

    /* -------- 四条直边 -------- */
    mui_hline_draw((int16_t)(x + r), y, (int16_t)(w - 2 * r), color);
    mui_hline_draw((int16_t)(x + r), (int16_t)(y + h - 1), (int16_t)(w - 2 * r), color);
    mui_vline_draw(x, (int16_t)(y + r), (int16_t)(h - 2 * r), color);
    mui_vline_draw((int16_t)(x + w - 1), (int16_t)(y + r), (int16_t)(h - 2 * r), color);

    /* -------- 四角圆弧（圆心与实心版一致） -------- */
    mui_circle_draw_arc((int16_t)(x + r), (int16_t)(y + r), r, 0x01, color);                 /* 左上 */
    mui_circle_draw_arc((int16_t)(x + w - 1 - r), (int16_t)(y + r), r, 0x02, color);         /* 右上 */
    mui_circle_draw_arc((int16_t)(x + w - 1 - r), (int16_t)(y + h - 1 - r), r, 0x04, color); /* 右下 */
    mui_circle_draw_arc((int16_t)(x + r), (int16_t)(y + h - 1 - r), r, 0x08, color);         /* 左下 */
}

/* -------- 三角形 -------- */

void mui_triangle_draw(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color)
{
    mui_line_draw(x0, y0, x1, y1, color);
    mui_line_draw(x1, y1, x2, y2, color);
    mui_line_draw(x2, y2, x0, y0, color);
}

void mui_triangle_fill(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color)
{
    int16_t tx;
    int16_t ty;
    int16_t y;
    int16_t y_start;
    int16_t y_end;

    /* -------- 顶点按 y 从小到大排序：y0 <= y1 <= y2 -------- */
    if (y0 > y1) { tx = x0; x0 = x1; x1 = tx; ty = y0; y0 = y1; y1 = ty; }
    if (y1 > y2) { tx = x1; x1 = x2; x2 = tx; ty = y1; y1 = y2; y2 = ty; }
    if (y0 > y1) { tx = x0; x0 = x1; x1 = tx; ty = y0; y0 = y1; y1 = ty; }

    /* -------- 完全退化为一条水平线 -------- */
    if (y0 == y2) {
        int16_t xmin = x0, xmax = x0;
        if (x1 < xmin) { xmin = x1; }
        if (x1 > xmax) { xmax = x1; }
        if (x2 < xmin) { xmin = x2; }
        if (x2 > xmax) { xmax = x2; }
        mui_hline_draw(xmin, y0, (int16_t)(xmax - xmin + 1), color);
        return;
    }

    y_start = (y0 < 0) ? 0 : y0;
    y_end = (y2 >= mui_screen_h) ? (int16_t)(mui_screen_h - 1) : y2;

    /* -------- 逐行扫描：长边 P0-P2 定一边，另一边在 P1 处切换 -------- */
    for (y = y_start; y <= y_end; y++) {
        int32_t xa = mui_edge_interp(x0, y0, x2, y2, y);
        int32_t xb;

        if (y <= y1) {
            xb = (y0 == y1)
                     ? ((int32_t)x1 << 8)
                     : mui_edge_interp(x0, y0, x1, y1, y);
        } else {
            xb = (y1 == y2)
                     ? ((int32_t)x2 << 8)
                     : mui_edge_interp(x1, y1, x2, y2, y);
        }

        {
            int32_t left = (xa < xb) ? xa : xb;
            int32_t right = (xa > xb) ? xa : xb;
            int16_t lx = (int16_t)(left >> 8);
            int16_t rw = (int16_t)(((right - left) >> 8) + 1);
            mui_hline_draw(lx, y, rw, color);
        }
    }
}

/* -------- 圆与椭圆 -------- */

void mui_circle_fill(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    int16_t dy;

    if (r < 0) {
        return;
    }
    if (r == 0) {
        mui_pixel_draw(cx, cy, color);
        return;
    }
    /* -------- 逐行计算水平跨度（纯整数开方） -------- */
    for (dy = 0; dy <= r; dy++) {
        uint32_t r_sq = (uint32_t)r * (uint32_t)r;
        uint32_t dy_sq = (uint32_t)dy * (uint32_t)dy;
        int16_t dx = (int16_t)mui_isqrt(r_sq - dy_sq);
        int16_t w = (int16_t)(dx * 2 + 1);
        if (dy == 0) {
            mui_hline_draw((int16_t)(cx - dx), cy, w, color);
        } else {
            mui_hline_draw((int16_t)(cx - dx), (int16_t)(cy - dy), w, color);
            mui_hline_draw((int16_t)(cx - dx), (int16_t)(cy + dy), w, color);
        }
    }
}

void mui_circle_draw(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    mui_circle_draw_arc(cx, cy, r, 0x0F, color);
}

void mui_ellipse_fill(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                      uint16_t color)
{
    int16_t dy;

    if (rx < 1 || ry < 1) {
        return;
    }
    /* -------- 逐行跨度：dx = rx * sqrt(ry^2 - dy^2) / ry -------- */
    for (dy = 0; dy <= ry; dy++) {
        uint32_t ry_sq = (uint32_t)ry * (uint32_t)ry;
        uint32_t dy_sq = (uint32_t)dy * (uint32_t)dy;
        uint32_t t = mui_isqrt(ry_sq - dy_sq);
        /* 四舍五入到最近整数，减少边缘锯齿 */
        int16_t dx = (int16_t)(((uint32_t)rx * t + (uint32_t)ry / 2) / (uint32_t)ry);
        int16_t w = (int16_t)(dx * 2 + 1);
        if (dy == 0) {
            mui_hline_draw((int16_t)(cx - dx), cy, w, color);
        } else {
            mui_hline_draw((int16_t)(cx - dx), (int16_t)(cy - dy), w, color);
            mui_hline_draw((int16_t)(cx - dx), (int16_t)(cy + dy), w, color);
        }
    }
}

void mui_ellipse_draw(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                      uint16_t color)
{
    int16_t i;

    if (rx < 1 || ry < 1) {
        return;
    }
    if (rx == ry) {
        mui_circle_draw(cx, cy, rx, color);
        return;
    }

    /* -------- 行列双扫描：与实心跨度法几何一致，曲线饱满无断点 -------- */
    /* 按行：每行画左右端点 */
    for (i = 0; i <= ry; i++) {
        uint32_t ry_sq = (uint32_t)ry * (uint32_t)ry;
        uint32_t dy_sq = (uint32_t)i * (uint32_t)i;
        uint32_t t = mui_isqrt(ry_sq - dy_sq);
        int16_t dx = (int16_t)(((uint32_t)rx * t + (uint32_t)ry / 2) / (uint32_t)ry);
        mui_pixel_draw((int16_t)(cx - dx), (int16_t)(cy - i), color);
        mui_pixel_draw((int16_t)(cx + dx), (int16_t)(cy - i), color);
        if (i != 0) {
            mui_pixel_draw((int16_t)(cx - dx), (int16_t)(cy + i), color);
            mui_pixel_draw((int16_t)(cx + dx), (int16_t)(cy + i), color);
        }
    }
    /* 按列：每列画上下端点（补充斜率大区段的连续性） */
    for (i = 0; i <= rx; i++) {
        uint32_t rx_sq = (uint32_t)rx * (uint32_t)rx;
        uint32_t dx_sq = (uint32_t)i * (uint32_t)i;
        uint32_t t = mui_isqrt(rx_sq - dx_sq);
        int16_t dy = (int16_t)(((uint32_t)ry * t + (uint32_t)rx / 2) / (uint32_t)rx);
        mui_pixel_draw((int16_t)(cx - i), (int16_t)(cy - dy), color);
        mui_pixel_draw((int16_t)(cx - i), (int16_t)(cy + dy), color);
        if (i != 0) {
            mui_pixel_draw((int16_t)(cx + i), (int16_t)(cy - dy), color);
            mui_pixel_draw((int16_t)(cx + i), (int16_t)(cy + dy), color);
        }
    }
}

/* -------- 位图 -------- */

void mui_image_draw(int16_t x, int16_t y, int16_t w, int16_t h,
                     const uint16_t *data)
{
    int16_t ox = x, oy = y;  /* 原始坐标：裁剪后计算源数据偏移用 */
    int32_t x2, y2;

    if (w < 1 || h < 1 || data == NULL) {
        return;
    }
    x2 = (int32_t)x + w;
    y2 = (int32_t)y + h;
    /* -------- 裁剪到屏幕范围（int32 计算防止溢出） -------- */
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x2 > mui_screen_w) {
        x2 = mui_screen_w;
    }
    if (y2 > mui_screen_h) {
        y2 = mui_screen_h;
    }
    if (x2 - x < 1 || y2 - y < 1) {
        return;
    }

    mui_out_draw_bitmap(x, y, (int16_t)(x2 - x), (int16_t)(y2 - y),
                        data + (int32_t)(y - oy) * w + (x - ox), w);
}

void mui_image_draw_key(int16_t x, int16_t y, int16_t w, int16_t h,
                         const uint16_t *data, uint16_t key)
{
    int16_t ox = x, oy = y;
    int32_t x2, y2;
    int16_t row, col, seg_start;

    if (w < 1 || h < 1 || data == NULL) {
        return;
    }
    x2 = (int32_t)x + w;
    y2 = (int32_t)y + h;
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if (x2 > mui_screen_w) {
        x2 = mui_screen_w;
    }
    if (y2 > mui_screen_h) {
        y2 = mui_screen_h;
    }
    if (x2 - x < 1 || y2 - y < 1) {
        return;
    }

    /* -------- 逐行扫描：不透明像素合并成连续段批量写 -------- */
    for (row = y; row < (int16_t)y2; row++) {
        const uint16_t *line = data + (int32_t)(row - oy) * w + (x - ox);
        int16_t vis_w = (int16_t)(x2 - x);  /* 本行可见宽度 */

        seg_start = -1;
        for (col = 0; col < vis_w; col++) {
            if (line[col] != key) {
                if (seg_start < 0) {
                    seg_start = col;
                }
            } else if (seg_start >= 0) {
                mui_out_draw_bitmap((int16_t)(x + seg_start), row,
                                    (int16_t)(col - seg_start), 1,
                                    &line[seg_start], w);
                seg_start = -1;
            }
        }
        if (seg_start >= 0) {
            mui_out_draw_bitmap((int16_t)(x + seg_start), row,
                                (int16_t)(vis_w - seg_start), 1,
                                &line[seg_start], w);
        }
    }
}

void mui_image_draw_mask(int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *data, uint16_t fg, uint16_t bg)
{
    int16_t sx = 0, sy = 0;
    int16_t row, col;

    if (w < 1 || h < 1 || data == NULL) {
        return;
    }
    if (x < 0) {
        sx = (int16_t)(-x);
        w = (int16_t)(w + x);
        x = 0;
    }
    if (y < 0) {
        sy = (int16_t)(-y);
        h = (int16_t)(h + y);
        y = 0;
    }
    if (x >= mui_screen_w || y >= mui_screen_h || w <= 0 || h <= 0) {
        return;
    }
    if (x + w > mui_screen_w) {
        w = (int16_t)(mui_screen_w - x);
    }
    if (y + h > mui_screen_h) {
        h = (int16_t)(mui_screen_h - y);
    }

    for (row = 0; row < h; row++) {
        const uint8_t *line = data + (int32_t)(sy + row) * (sx + w) + sx;
        for (col = 0; col < w; col++) {
            uint8_t a = line[col];

            if (a < 8) {
                continue;
            }
            mui_out_draw_pixel((int16_t)(x + col), (int16_t)(y + row),
                               a >= 248 ? fg : mui_color_mix(fg, bg, a));
        }
    }
}

/* -------- 颜色混合与抗锯齿 -------- */

uint16_t mui_color_mix(uint16_t fg, uint16_t bg, uint8_t alpha)
{
    uint32_t w_fg = alpha;
    uint32_t w_bg = 255u - alpha;
    uint32_t r = ((((fg >> 11) & 0x1F) * w_fg + ((bg >> 11) & 0x1F) * w_bg)
                  + 127) / 255;
    uint32_t g = ((((fg >> 5) & 0x3F) * w_fg + ((bg >> 5) & 0x3F) * w_bg)
                  + 127) / 255;
    uint32_t b = ((((fg & 0x1F) * w_fg) + (bg & 0x1F) * w_bg)
                  + 127) / 255;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void mui_line_draw_aa(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      uint16_t fg, uint16_t bg)
{
    int16_t t;
    int16_t dx;
    int16_t dy;
    int16_t grad_fp;
    int32_t y_fp;
    int16_t x;
    uint8_t steep;

    /* -------- 陡峭线交换 x/y，统一为水平主导 -------- */
    steep = (uint8_t)(((y1 > y0) ? (y1 - y0) : (y0 - y1))
                      > ((x1 > x0) ? (x1 - x0) : (x0 - x1)));
    if (steep) {
        t = x0; x0 = y0; y0 = t;
        t = x1; x1 = y1; y1 = t;
    }
    if (x0 > x1) {
        t = x0; x0 = x1; x1 = t;
        t = y0; y0 = y1; y1 = t;
    }
    dx = (int16_t)(x1 - x0);
    dy = y1 - y0;                    /* 带符号 */
    if (dx == 0) {
        /* 交换后仍无横向跨度（原线为水平/垂直）：直接画 */
        mui_line_draw(x0, y0, x1, y1, fg);
        return;
    }
    grad_fp = (int16_t)(((int32_t)dy << 8) / dx);   /* 斜率定点 8 位 */
    y_fp = ((int32_t)y0 << 8) + 128;                 /* 半像素中心 */

    for (x = x0; x <= x1; x++) {
        int16_t y = (int16_t)(y_fp >> 8);
        uint8_t frac = (uint8_t)(y_fp & 0xFF);
        uint8_t a_hi = (uint8_t)(255 - frac);   /* 上侧像素前景强度 */
        if (a_hi != 0) {
            uint16_t c_hi = mui_color_mix(fg, bg, a_hi);
            if (steep) {
                mui_pixel_draw(y, x, c_hi);      /* 坐标换回屏幕系 */
            } else {
                mui_pixel_draw(x, y, c_hi);
            }
        }
        if (frac != 0) {
            uint16_t c_lo = mui_color_mix(fg, bg, frac);
            if (steep) {
                mui_pixel_draw((int16_t)(y + 1), x, c_lo);
            } else {
                mui_pixel_draw(x, (int16_t)(y + 1), c_lo);
            }
        }
        y_fp += grad_fp;
    }
}

void mui_circle_draw_aa(int16_t cx, int16_t cy, int16_t r,
                        uint16_t fg, uint16_t bg)
{
    int16_t x;
    int16_t x_max;

    if (r < 0) {
        return;
    }
    if (r == 0) {
        mui_pixel_draw(cx, cy, fg);
        return;
    }
    x_max = (int16_t)mui_isqrt((uint32_t)r * r / 2);  /* 45° 处截止 */

    for (x = 0; x <= x_max; x++) {
        uint32_t v = (uint32_t)r * r - (uint32_t)x * x;
        uint32_t y_fp = mui_isqrt(v << 16);           /* 8 位小数定点 */
        int16_t y = (int16_t)(y_fp >> 8);
        uint8_t frac = (uint8_t)(y_fp & 0xFF);
        uint8_t a_in = (uint8_t)(255 - frac);         /* 内侧像素强度 */
        int16_t y1 = (int16_t)(y + 1);

        /* 圆上每点画"内侧 + 外侧"双像素，八对称展开 */
        if (a_in != 0) {
            uint16_t c = mui_color_mix(fg, bg, a_in);
            mui_pixel_draw((int16_t)(cx + x), (int16_t)(cy - y), c);
            mui_pixel_draw((int16_t)(cx - x), (int16_t)(cy - y), c);
            mui_pixel_draw((int16_t)(cx + x), (int16_t)(cy + y), c);
            mui_pixel_draw((int16_t)(cx - x), (int16_t)(cy + y), c);
            mui_pixel_draw((int16_t)(cx + y), (int16_t)(cy - x), c);
            mui_pixel_draw((int16_t)(cx - y), (int16_t)(cy - x), c);
            mui_pixel_draw((int16_t)(cx + y), (int16_t)(cy + x), c);
            mui_pixel_draw((int16_t)(cx - y), (int16_t)(cy + x), c);
        }
        if (frac != 0) {
            uint16_t c = mui_color_mix(fg, bg, frac);
            mui_pixel_draw((int16_t)(cx + x), (int16_t)(cy - y1), c);
            mui_pixel_draw((int16_t)(cx - x), (int16_t)(cy - y1), c);
            mui_pixel_draw((int16_t)(cx + x), (int16_t)(cy + y1), c);
            mui_pixel_draw((int16_t)(cx - x), (int16_t)(cy + y1), c);
            mui_pixel_draw((int16_t)(cx + y1), (int16_t)(cy - x), c);
            mui_pixel_draw((int16_t)(cx - y1), (int16_t)(cy - x), c);
            mui_pixel_draw((int16_t)(cx + y1), (int16_t)(cy + x), c);
            mui_pixel_draw((int16_t)(cx - y1), (int16_t)(cy + x), c);
        }
    }
}

/* -------- 抗锯齿圆角矩形 -------- */

/**
 * @brief sqrt(n) 的 8 位定点值（即 floor(sqrt(n) * 256)），无除法
 * @note  n >= 65536 时退化为 4 位小数以避开 n << 16 溢出
 *        （圆角半径 r <= 90 时总是走 8 位小数路径，精度与逐像素版一致）
 */
static uint32_t mui_sqrt_fp8(uint32_t n)
{
    if (n < 65536u) {
        return mui_isqrt(n << 16);
    }
    return mui_isqrt(n << 8) << 4;
}

/**
 * @brief 角区像素对圆角的覆盖率
 * @param ux 像素中心相对角圆心的横向距离（半像素单位，即 2*(px-cx)）
 * @param uy 同上，纵向
 * @param r  圆角半径（像素）
 * @return 0~255：dist <= r 时为 255（实心），dist >= r + 0.5 时为 0
 */
static uint8_t mui_corner_alpha(int16_t ux, int16_t uy, int16_t r)
{
    uint32_t d = mui_sqrt_fp8((uint32_t)(ux * ux) + (uint32_t)(uy * uy));
    int32_t  cov = (int32_t)(r + 1) * 512 - (int32_t)d;   /* 半径 r+0.5 像素 + 半像素修正 */

    if (cov <= 0) {
        return 0;
    }
    if (cov >= 512) {
        return 255;
    }
    return (uint8_t)((cov * 255) >> 9);
}

/**
 * @brief 抗锯齿落笔：能回读屏幕就用真实底色，否则用调用方给的 bg
 */
static void mui_pixel_draw_aa(int16_t x, int16_t y, uint16_t fg, uint16_t bg,
                              uint8_t a)
{
    uint16_t base;

    if (a == 0) {
        return;
    }
    if (MUI_OUT_CAN_READ_PIXEL) {
        base = mui_out_read_pixel(x, y);
    } else {
        base = bg;
    }
    mui_pixel_draw(x, y, (a == 255) ? fg : mui_color_mix(fg, base, a));
}

void mui_round_rect_fill_aa(int16_t x, int16_t y, int16_t w, int16_t h,
                            int16_t r, uint16_t fg, uint16_t bg)
{
    int16_t limit;
    int16_t cxl;
    int16_t cxr;
    int16_t band;

    if (w < 1 || h < 1) {
        return;
    }
    limit = (w < h) ? (int16_t)(w / 2) : (int16_t)(h / 2);
    if (r > limit) {
        r = limit;
    }
    if (r < 0) {
        r = 0;
    }
    if (r == 0) {
        mui_rect_fill(x, y, w, h, fg);
        return;
    }

    /* -------- 主体"十字"（与硬边版同构）：角带的中段也由中间竖条覆盖 -------- */
    mui_rect_fill((int16_t)(x + r), y, (int16_t)(w - 2 * r), h, fg);
    mui_rect_fill(x, (int16_t)(y + r), r, (int16_t)(h - 2 * r), fg);
    mui_rect_fill((int16_t)(x + w - r), (int16_t)(y + r), r,
                  (int16_t)(h - 2 * r), fg);

    cxl = (int16_t)(x + r);              /* 左角圆心列 */
    cxr = (int16_t)(x + w - 1 - r);      /* 右角圆心列 */

    /* -------- 上下两个角带：每行 1 次开方求跨度 → 批量填充 + 两端边界像素混色 -------- */
    for (band = 0; band < 2; band++) {
        int16_t cy = (band == 0) ? (int16_t)(y + r) : (int16_t)(y + h - 1 - r);
        int16_t k;

        for (k = 0; k < r; k++) {
            int16_t py = (band == 0) ? (int16_t)(y + k) : (int16_t)(y + h - 1 - k);
            int16_t dy = (int16_t)((py > cy) ? (py - cy) : (cy - py));
            uint32_t dy2 = (uint32_t)dy * dy;
            int16_t dxf = (int16_t)mui_isqrt((uint32_t)r * r - dy2);   /* 满覆盖半跨度（像素） */
            int16_t dxp = (int16_t)(mui_isqrt(4u * (uint32_t)(r + 1) * (r + 1)
                                              - 4u * dy2) >> 1);       /* 可能仍有覆盖的半跨度 */
            int16_t px;

            /* 满覆盖段（dist <= r）：一行一次批量填充；中间与主体重叠也无害 */
            if (dxf > 0) {
                mui_rect_fill((int16_t)(cxl - dxf), py,
                              (int16_t)(cxr - cxl + 2 * dxf + 1), 1, fg);
            }

            /* 左端边界像素 */
            px = (int16_t)(cxl - dxp);
            if (px < x) {
                px = x;
            }
            for (; px < (int16_t)(cxl - dxf); px++) {
                mui_pixel_draw_aa(px, py, fg, bg,
                                  mui_corner_alpha((int16_t)(2 * (px - cxl)),
                                                   (int16_t)(2 * (py - cy)), r));
            }

            /* 右端边界像素 */
            for (px = (int16_t)(cxr + dxf + 1); px <= (int16_t)(cxr + dxp); px++) {
                if (px > (int16_t)(x + w - 1)) {
                    break;
                }
                mui_pixel_draw_aa(px, py, fg, bg,
                                  mui_corner_alpha((int16_t)(2 * (px - cxr)),
                                                   (int16_t)(2 * (py - cy)), r));
            }
        }
    }
}
