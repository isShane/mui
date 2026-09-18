/**
 * @file mui_gfx.c
 * @brief MUI 图形库核心实现
 *
 * 全部使用整数运算（无浮点、无动态内存），可直接在裸机环境运行。
 */

#include "mui.h"

/* -------- 内部状态 -------- */

static int16_t mui_screen_w = 0;  /**< 屏幕宽度 */
static int16_t mui_screen_h = 0;  /**< 屏幕高度 */

/* -------- 内部工具 -------- */

/**
 * @brief 整数平方根（牛顿迭代法，无浮点依赖）
 * @param n 输入值（n <= 0 时返回 0）
 * @return 不超过 sqrt(n) 的最大整数
 */
static uint32_t mui_isqrt(uint32_t n)
{
    uint32_t x;
    uint32_t y;

    if (n == 0) {
        return 0;
    }
    x = n;
    y = (x + 1u) / 2u;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2u;
    }
    return x;
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
    mui_screen_w = screen_w;
    mui_screen_h = screen_h;
    mui_port_init();
}

int16_t mui_get_width(void)
{
    return mui_screen_w;
}

int16_t mui_get_height(void)
{
    return mui_screen_h;
}

/* -------- 基础图元 -------- */

void mui_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= mui_screen_w || y >= mui_screen_h) {
        return;
    }
    mui_port_draw_pixel(x, y, color);
}

void mui_clear_screen(uint16_t color)
{
    mui_fill_rect(0, 0, mui_screen_w, mui_screen_h, color);
}

void mui_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
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
    mui_port_fill_rect(x, y, (int16_t)(x2 - x), (int16_t)(y2 - y), color);
}

void mui_draw_hline(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    mui_fill_rect(x, y, w, 1, color);
}

void mui_draw_vline(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    mui_fill_rect(x, y, 1, h, color);
}

void mui_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
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
        mui_draw_hline(x0, y0, (int16_t)(x1 - x0 + 1), color);
        return;
    }
    if (x0 == x1) {
        if (y0 > y1) {
            dy = y0; y0 = y1; y1 = dy;
        }
        mui_draw_vline(x0, y0, (int16_t)(y1 - y0 + 1), color);
        return;
    }

    /* -------- Bresenham 通用直线 -------- */
    dx = (int16_t)((x1 > x0) ? (x1 - x0) : (x0 - x1));
    dy = (int16_t)((y1 > y0) ? (y1 - y0) : (y0 - y1));
    sx = (x0 < x1) ? 1 : -1;
    sy = (y0 < y1) ? 1 : -1;
    err = (int16_t)(dx - dy);

    for (;;) {
        mui_draw_pixel(x0, y0, color);
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

void mui_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (w < 1 || h < 1) {
        return;
    }
    mui_draw_hline(x, y, w, color);              /* 上边 */
    mui_draw_hline(x, (int16_t)(y + h - 1), w, color); /* 下边 */
    if (h > 2) {
        mui_draw_vline(x, (int16_t)(y + 1), (int16_t)(h - 2), color);       /* 左边 */
        mui_draw_vline((int16_t)(x + w - 1), (int16_t)(y + 1), (int16_t)(h - 2), color); /* 右边 */
    }
}

void mui_fill_round_rect(int16_t x, int16_t y, int16_t w, int16_t h,
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
        mui_fill_rect(x, y, w, h, color);
        return;
    }

    /* -------- 主体：中间竖条 + 左右条 -------- */
    mui_fill_rect((int16_t)(x + r), y, (int16_t)(w - 2 * r), h, color);
    mui_fill_rect(x, (int16_t)(y + r), r, (int16_t)(h - 2 * r), color);
    mui_fill_rect((int16_t)(x + w - r), (int16_t)(y + r), r,
                  (int16_t)(h - 2 * r), color);

    /* -------- 四角：逐行画 1/4 圆盘水平跨度 -------- */
    /* 角圆心：LT(x+r,y+r) RT(x+w-1-r,y+r) LB(x+r,y+h-1-r) RB(x+w-1-r,y+h-1-r) */
    for (i = 0; i < r; i++) {
        int16_t d = (int16_t)(r - i);   /* 该行到角圆心的垂直距离 */
        uint32_t r_sq = (uint32_t)r * (uint32_t)r;
        uint32_t d_sq = (uint32_t)d * (uint32_t)d;
        int16_t dx = (int16_t)mui_isqrt(r_sq - d_sq);
        /* 象限含圆心列，跨度宽 dx+1，保证与圆盘行宽 2*dx+1 一致 */
        mui_draw_hline((int16_t)(x + r - dx), (int16_t)(y + i),
                       (int16_t)(dx + 1), color);                            /* 左上 */
        mui_draw_hline((int16_t)(x + w - 1 - r), (int16_t)(y + i),
                       (int16_t)(dx + 1), color);                            /* 右上 */
        mui_draw_hline((int16_t)(x + r - dx), (int16_t)(y + h - 1 - i),
                       (int16_t)(dx + 1), color);                            /* 左下 */
        mui_draw_hline((int16_t)(x + w - 1 - r), (int16_t)(y + h - 1 - i),
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
static void mui_draw_circle_arc(int16_t cx, int16_t cy, int16_t r,
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
            mui_draw_pixel((int16_t)(cx - px), (int16_t)(cy - py), color);
            mui_draw_pixel((int16_t)(cx - py), (int16_t)(cy - px), color);
        }
        if (quad & 0x02) { /* 右上 */
            mui_draw_pixel((int16_t)(cx + px), (int16_t)(cy - py), color);
            mui_draw_pixel((int16_t)(cx + py), (int16_t)(cy - px), color);
        }
        if (quad & 0x04) { /* 右下 */
            mui_draw_pixel((int16_t)(cx + px), (int16_t)(cy + py), color);
            mui_draw_pixel((int16_t)(cx + py), (int16_t)(cy + px), color);
        }
        if (quad & 0x08) { /* 左下 */
            mui_draw_pixel((int16_t)(cx - px), (int16_t)(cy + py), color);
            mui_draw_pixel((int16_t)(cx - py), (int16_t)(cy + px), color);
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

void mui_draw_round_rect(int16_t x, int16_t y, int16_t w, int16_t h,
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
        mui_draw_rect(x, y, w, h, color);
        return;
    }

    /* -------- 四条直边 -------- */
    mui_draw_hline((int16_t)(x + r), y, (int16_t)(w - 2 * r), color);
    mui_draw_hline((int16_t)(x + r), (int16_t)(y + h - 1), (int16_t)(w - 2 * r), color);
    mui_draw_vline(x, (int16_t)(y + r), (int16_t)(h - 2 * r), color);
    mui_draw_vline((int16_t)(x + w - 1), (int16_t)(y + r), (int16_t)(h - 2 * r), color);

    /* -------- 四角圆弧（圆心与实心版一致） -------- */
    mui_draw_circle_arc((int16_t)(x + r), (int16_t)(y + r), r, 0x01, color);                 /* 左上 */
    mui_draw_circle_arc((int16_t)(x + w - 1 - r), (int16_t)(y + r), r, 0x02, color);         /* 右上 */
    mui_draw_circle_arc((int16_t)(x + w - 1 - r), (int16_t)(y + h - 1 - r), r, 0x04, color); /* 右下 */
    mui_draw_circle_arc((int16_t)(x + r), (int16_t)(y + h - 1 - r), r, 0x08, color);         /* 左下 */
}

/* -------- 三角形 -------- */

void mui_draw_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color)
{
    mui_draw_line(x0, y0, x1, y1, color);
    mui_draw_line(x1, y1, x2, y2, color);
    mui_draw_line(x2, y2, x0, y0, color);
}

void mui_fill_triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
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
        mui_draw_hline(xmin, y0, (int16_t)(xmax - xmin + 1), color);
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
            mui_draw_hline(lx, y, rw, color);
        }
    }
}

/* -------- 圆与椭圆 -------- */

void mui_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    int16_t dy;

    if (r < 0) {
        return;
    }
    if (r == 0) {
        mui_draw_pixel(cx, cy, color);
        return;
    }
    /* -------- 逐行计算水平跨度（纯整数开方） -------- */
    for (dy = 0; dy <= r; dy++) {
        uint32_t r_sq = (uint32_t)r * (uint32_t)r;
        uint32_t dy_sq = (uint32_t)dy * (uint32_t)dy;
        int16_t dx = (int16_t)mui_isqrt(r_sq - dy_sq);
        int16_t w = (int16_t)(dx * 2 + 1);
        if (dy == 0) {
            mui_draw_hline((int16_t)(cx - dx), cy, w, color);
        } else {
            mui_draw_hline((int16_t)(cx - dx), (int16_t)(cy - dy), w, color);
            mui_draw_hline((int16_t)(cx - dx), (int16_t)(cy + dy), w, color);
        }
    }
}

void mui_draw_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    mui_draw_circle_arc(cx, cy, r, 0x0F, color);
}

void mui_fill_ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
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
            mui_draw_hline((int16_t)(cx - dx), cy, w, color);
        } else {
            mui_draw_hline((int16_t)(cx - dx), (int16_t)(cy - dy), w, color);
            mui_draw_hline((int16_t)(cx - dx), (int16_t)(cy + dy), w, color);
        }
    }
}

void mui_draw_ellipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                      uint16_t color)
{
    int16_t i;

    if (rx < 1 || ry < 1) {
        return;
    }
    if (rx == ry) {
        mui_draw_circle(cx, cy, rx, color);
        return;
    }

    /* -------- 行列双扫描：与实心跨度法几何一致，曲线饱满无断点 -------- */
    /* 按行：每行画左右端点 */
    for (i = 0; i <= ry; i++) {
        uint32_t ry_sq = (uint32_t)ry * (uint32_t)ry;
        uint32_t dy_sq = (uint32_t)i * (uint32_t)i;
        uint32_t t = mui_isqrt(ry_sq - dy_sq);
        int16_t dx = (int16_t)(((uint32_t)rx * t + (uint32_t)ry / 2) / (uint32_t)ry);
        mui_draw_pixel((int16_t)(cx - dx), (int16_t)(cy - i), color);
        mui_draw_pixel((int16_t)(cx + dx), (int16_t)(cy - i), color);
        if (i != 0) {
            mui_draw_pixel((int16_t)(cx - dx), (int16_t)(cy + i), color);
            mui_draw_pixel((int16_t)(cx + dx), (int16_t)(cy + i), color);
        }
    }
    /* 按列：每列画上下端点（补充斜率大区段的连续性） */
    for (i = 0; i <= rx; i++) {
        uint32_t rx_sq = (uint32_t)rx * (uint32_t)rx;
        uint32_t dx_sq = (uint32_t)i * (uint32_t)i;
        uint32_t t = mui_isqrt(rx_sq - dx_sq);
        int16_t dy = (int16_t)(((uint32_t)ry * t + (uint32_t)rx / 2) / (uint32_t)rx);
        mui_draw_pixel((int16_t)(cx - i), (int16_t)(cy - dy), color);
        mui_draw_pixel((int16_t)(cx - i), (int16_t)(cy + dy), color);
        if (i != 0) {
            mui_draw_pixel((int16_t)(cx + i), (int16_t)(cy - dy), color);
            mui_draw_pixel((int16_t)(cx + i), (int16_t)(cy + dy), color);
        }
    }
}

/* -------- 位图 -------- */

void mui_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
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

    mui_port_draw_bitmap(x, y, (int16_t)(x2 - x), (int16_t)(y2 - y),
                         data + (int32_t)(y - oy) * w + (x - ox), w);
}

void mui_draw_bitmap_key(int16_t x, int16_t y, int16_t w, int16_t h,
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
                mui_port_draw_bitmap((int16_t)(x + seg_start), row,
                                     (int16_t)(col - seg_start), 1,
                                     &line[seg_start], w);
                seg_start = -1;
            }
        }
        if (seg_start >= 0) {
            mui_port_draw_bitmap((int16_t)(x + seg_start), row,
                                 (int16_t)(vis_w - seg_start), 1,
                                 &line[seg_start], w);
        }
    }
}

void mui_draw_bitmap_alpha(int16_t x, int16_t y, int16_t w, int16_t h,
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
            mui_port_draw_pixel((int16_t)(x + col), (int16_t)(y + row),
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

void mui_draw_line_aa(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
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
        mui_draw_line(x0, y0, x1, y1, fg);
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
                mui_draw_pixel(y, x, c_hi);      /* 坐标换回屏幕系 */
            } else {
                mui_draw_pixel(x, y, c_hi);
            }
        }
        if (frac != 0) {
            uint16_t c_lo = mui_color_mix(fg, bg, frac);
            if (steep) {
                mui_draw_pixel((int16_t)(y + 1), x, c_lo);
            } else {
                mui_draw_pixel(x, (int16_t)(y + 1), c_lo);
            }
        }
        y_fp += grad_fp;
    }
}

void mui_draw_circle_aa(int16_t cx, int16_t cy, int16_t r,
                        uint16_t fg, uint16_t bg)
{
    int16_t x;
    int16_t x_max;

    if (r < 0) {
        return;
    }
    if (r == 0) {
        mui_draw_pixel(cx, cy, fg);
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
            mui_draw_pixel((int16_t)(cx + x), (int16_t)(cy - y), c);
            mui_draw_pixel((int16_t)(cx - x), (int16_t)(cy - y), c);
            mui_draw_pixel((int16_t)(cx + x), (int16_t)(cy + y), c);
            mui_draw_pixel((int16_t)(cx - x), (int16_t)(cy + y), c);
            mui_draw_pixel((int16_t)(cx + y), (int16_t)(cy - x), c);
            mui_draw_pixel((int16_t)(cx - y), (int16_t)(cy - x), c);
            mui_draw_pixel((int16_t)(cx + y), (int16_t)(cy + x), c);
            mui_draw_pixel((int16_t)(cx - y), (int16_t)(cy + x), c);
        }
        if (frac != 0) {
            uint16_t c = mui_color_mix(fg, bg, frac);
            mui_draw_pixel((int16_t)(cx + x), (int16_t)(cy - y1), c);
            mui_draw_pixel((int16_t)(cx - x), (int16_t)(cy - y1), c);
            mui_draw_pixel((int16_t)(cx + x), (int16_t)(cy + y1), c);
            mui_draw_pixel((int16_t)(cx - x), (int16_t)(cy + y1), c);
            mui_draw_pixel((int16_t)(cx + y1), (int16_t)(cy - x), c);
            mui_draw_pixel((int16_t)(cx - y1), (int16_t)(cy - x), c);
            mui_draw_pixel((int16_t)(cx + y1), (int16_t)(cy + x), c);
            mui_draw_pixel((int16_t)(cx - y1), (int16_t)(cy + x), c);
        }
    }
}
