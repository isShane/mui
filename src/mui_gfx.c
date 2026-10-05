/**
 * @file mui_gfx.c
 * @brief MUI 图形库核心实现
 *
 * 全部使用整数运算（无浮点、无动态内存），可直接在裸机环境运行。
 */

#include "mui.h"
#include "mui_out.h"    /* 输出层：按 mui_conf.h 选择直绘或缓冲后端 */
#include "mui_math.h"   /* 库内数学工具（定义在本文件，声明给其它模块用） */
#include "mui_anim.h"   /* 动画支撑（定义在本文件，声明给其它模块用） */

/* -------- 内部状态 -------- */

static int16_t mui_screen_w = 0;  /**< 屏幕宽度 */
static int16_t mui_screen_h = 0;  /**< 屏幕高度 */

/** @brief 当前绘制裁剪区（开区间 x <= px < x2, y <= py < y2），mui_init 复位为全屏 */
static mui_rect_t s_clip = { 0, 0, 0, 0 };

/**
 * @brief "全屏"的参照矩形：mui_reset_clip() 恢复到这个矩形
 *
 * 常规后端下它就是整屏；**条带后端下它是当前条带**。这样应用在绘制回调里
 * 照常调用 mui_reset_clip()（"我要画满全屏"）就自动变成"画满当前条带"，
 * 既有绘制代码不必改。
 */
static mui_rect_t s_clip_base = { 0, 0, 0, 0 };

/* -------- 内部工具 -------- */

/**
 * @brief 内部：把矩形裁剪到当前裁剪区（同时做 w/h 合法性检查）
 * @param x,y,w,h 输入输出：矩形，裁剪后就地更新
 * @return 1=有可见部分，0=完全不可见
 * @note  裁剪区本身保证落在屏幕内，故结果必然也在屏幕内，移植层契约不变。
 */
static uint8_t mui__clip_rect(int16_t *x, int16_t *y, int16_t *w, int16_t *h)
{
    int32_t x2, y2;

    if (*w < 1 || *h < 1) {
        return 0;
    }
    x2 = (int32_t)(*x) + *w;
    y2 = (int32_t)(*y) + *h;
    if (*x < s_clip.x)  { *x = s_clip.x; }
    if (*y < s_clip.y)  { *y = s_clip.y; }
    if (x2 > s_clip.x2) { x2 = s_clip.x2; }
    if (y2 > s_clip.y2) { y2 = s_clip.y2; }
    if (x2 - *x < 1 || y2 - *y < 1) {
        return 0;
    }
    *w = (int16_t)(x2 - *x);
    *h = (int16_t)(y2 - *y);
    return 1;
}

/**
 * @brief 整数平方根（逐位法：只用移位/比较/减法，无除法、无浮点）
 * @param n 输入值（n == 0 时返回 0）
 * @return 不超过 sqrt(n) 的最大整数
 * @note  早期版本用牛顿迭代（每轮一次 32 位除法），在无硬件除法的 M0 上约 900 周期/次；
 *        逐位法约 100~150 周期，圆/椭圆填充与抗锯齿都能受益，且结果同为 floor(sqrt(n))。
 *        非 static：库内其它模块（如控件）也用得到，声明见 mui_math.h。
 */
uint32_t mui_isqrt(uint32_t n)
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
#if !MUI_CFG_IS_STRIP
    /* 条带后端只按"宽 × 带高"分配，屏高不受 MUI_CFG_BUF_MAX_H 约束 */
    if (screen_h > (int16_t)MUI_CFG_BUF_MAX_H) {
        screen_h = (int16_t)MUI_CFG_BUF_MAX_H;
    }
#endif
#endif
    mui_screen_w = screen_w;
    mui_screen_h = screen_h;
    s_clip_base.x  = 0;
    s_clip_base.y  = 0;
    s_clip_base.x2 = screen_w;
    s_clip_base.y2 = screen_h;
    mui_reset_clip();
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

/* -------- 绘制裁剪区 -------- */

void mui_set_clip(int16_t x, int16_t y, int16_t w, int16_t h)
{
    int32_t x2, y2;

    if (w < 1 || h < 1) {
        s_clip.x = 0; s_clip.y = 0; s_clip.x2 = 0; s_clip.y2 = 0;  /* 空裁剪区 */
        return;
    }
    x2 = (int32_t)x + w;
    y2 = (int32_t)y + h;
    if (x < 0) { x = 0; }
    if (y < 0) { y = 0; }
    if (x2 > mui_screen_w) { x2 = mui_screen_w; }
    if (y2 > mui_screen_h) { y2 = mui_screen_h; }
    if (x2 <= x || y2 <= y) {
        s_clip.x = 0; s_clip.y = 0; s_clip.x2 = 0; s_clip.y2 = 0;  /* 与屏幕无交集 */
        return;
    }
    s_clip.x = x;
    s_clip.y = y;
    s_clip.x2 = (int16_t)x2;
    s_clip.y2 = (int16_t)y2;
}

void mui_reset_clip(void)
{
    /* 恢复到"全屏"参照：常规后端 = 整屏；条带后端 = 当前条带（见 s_clip_base） */
    s_clip = s_clip_base;
}

mui_rect_t mui_clip_save(void)
{
    return s_clip;
}

void mui_clip_restore(mui_rect_t c)
{
    s_clip = c;
}

/* -------- 整帧绘制通道 -------- */

void mui_screen_frame(mui_draw_fn draw, void *ctx)
{
#if MUI_CFG_IS_STRIP
    int16_t y = 0;
    int16_t sh = mui_strip_get_height();

    if (mui_screen_w < 1 || mui_screen_h < 1) {
        return;
    }
    if (sh < 1) {
        sh = 1;                     /* 防死循环 */
    }
    if (sh > mui_screen_h) {
        sh = mui_screen_h;
    }

    /* 同一条绘制代码按带重跑：每遍只保留该带的像素，画完立刻推屏 */
    while (y < mui_screen_h) {
        int16_t h = (int16_t)((mui_screen_h - y < sh) ? (mui_screen_h - y) : sh);

        mui_out_band_begin(y, h);
        s_clip_base.x  = 0;
        s_clip_base.y  = y;
        s_clip_base.x2 = mui_screen_w;
        s_clip_base.y2 = (int16_t)(y + h);
        mui_reset_clip();

        if (draw != NULL) {
            draw(ctx);
        }

        mui_out_band_flush();
        y = (int16_t)(y + h);
    }

    /* 收尾：把"全屏"参照恢复成整屏，避免帧外绘制被上一带的裁剪区限制 */
    s_clip_base.x  = 0;
    s_clip_base.y  = 0;
    s_clip_base.x2 = mui_screen_w;
    s_clip_base.y2 = mui_screen_h;
    mui_reset_clip();
#else
    if (draw != NULL) {
        draw(ctx);
    }
    mui_screen_flush();             /* 缓冲后端推屏；直绘下为空操作 */
#endif
}

/* -------- 脏矩形跟踪 -------- */

#if MUI_CFG_DIRTY_N > 0

static mui_rect_t s_dirty[MUI_CFG_DIRTY_N];   /**< 脏区列表（并集须覆盖所有已写像素） */
static uint8_t    s_dirty_count = 0;          /**< 当前脏区数量 */

/** @brief 内部：两个矩形的并集包围盒 */
static mui_rect_t mui__rect_union(mui_rect_t a, mui_rect_t b)
{
    mui_rect_t u;

    u.x  = (a.x  < b.x)  ? a.x  : b.x;
    u.y  = (a.y  < b.y)  ? a.y  : b.y;
    u.x2 = (a.x2 > b.x2) ? a.x2 : b.x2;
    u.y2 = (a.y2 > b.y2) ? a.y2 : b.y2;
    return u;
}

/** @brief 内部：矩形面积 */
static int32_t mui__rect_area(mui_rect_t r)
{
    return (int32_t)(r.x2 - r.x) * (int32_t)(r.y2 - r.y);
}

/**
 * @brief 内部：把矩形并入脏区列表
 *
 * 策略（合并只放大、绝不丢弃，保证"并集覆盖所有写入像素"）：
 *   1) 已被某个脏区包含 → 直接返回；
 *   2) 有空槽 → 占用新槽；
 *   3) 槽已满 → 合并"并集面积增量最小"的一对脏区腾出槽位，再放入新矩形。
 * @note 入参须为已裁剪到屏幕内的合法矩形。
 */
static void mui__dirty_add(int16_t x, int16_t y, int16_t w, int16_t h)
{
    mui_rect_t r;
    uint8_t i, j;

    if (w < 1 || h < 1) {
        return;
    }
    r.x  = x;
    r.y  = y;
    r.x2 = (int16_t)(x + w);
    r.y2 = (int16_t)(y + h);

    /* 1) 已被包含 */
    for (i = 0; i < s_dirty_count; i++) {
        if (s_dirty[i].x <= r.x && s_dirty[i].y <= r.y &&
            s_dirty[i].x2 >= r.x2 && s_dirty[i].y2 >= r.y2) {
            return;
        }
    }

    /* 2) 有空槽 */
    if (s_dirty_count < (uint8_t)MUI_CFG_DIRTY_N) {
        s_dirty[s_dirty_count] = r;
        s_dirty_count++;
        return;
    }

    /* 3) 槽满：合并代价最小的一对，腾出槽位 */
    {
        int32_t best = -1;
        uint8_t bi = 0, bj = 1;

        for (i = 0; i < s_dirty_count; i++) {
            for (j = (uint8_t)(i + 1); j < s_dirty_count; j++) {
                int32_t cost =
                    mui__rect_area(mui__rect_union(s_dirty[i], s_dirty[j]))
                    - mui__rect_area(s_dirty[i]) - mui__rect_area(s_dirty[j]);
                if (best < 0 || cost < best) {
                    best = cost;
                    bi = i;
                    bj = j;
                }
            }
        }
        /* 合并到 bi，移除 bj（用末元素填补，保持列表紧凑） */
        s_dirty[bi] = mui__rect_union(s_dirty[bi], s_dirty[bj]);
        s_dirty[bj] = s_dirty[s_dirty_count - 1];
        s_dirty_count--;
    }
    s_dirty[s_dirty_count] = r;
    s_dirty_count++;
}

/** @brief 内部：若有脏区跟踪则记录（关闭时代码被编译掉） */
#define MUI_DIRTY_ADD(x, y, w, h)   mui__dirty_add((x), (y), (w), (h))

#else  /* MUI_CFG_DIRTY_N == 0：关闭，数组不占 RAM */

#define MUI_DIRTY_ADD(x, y, w, h)   ((void)0)

#endif /* MUI_CFG_DIRTY_N > 0 */

void mui_dirty_clear(void)
{
#if MUI_CFG_DIRTY_N > 0
    s_dirty_count = 0;
#endif
}

void mui_dirty_mark(int16_t x, int16_t y, int16_t w, int16_t h)
{
#if MUI_CFG_DIRTY_N > 0
    if (!mui__clip_rect(&x, &y, &w, &h)) {
        return;
    }
    mui__dirty_add(x, y, w, h);
#else
    (void)x; (void)y; (void)w; (void)h;
#endif
}

uint8_t mui_dirty_count(void)
{
#if MUI_CFG_DIRTY_N > 0
    return s_dirty_count;
#else
    return 0;
#endif
}

uint8_t mui_dirty_get(uint8_t idx, mui_rect_t *r)
{
#if MUI_CFG_DIRTY_N > 0
    if (r == NULL || idx >= s_dirty_count) {
        return 0;
    }
    *r = s_dirty[idx];
    return 1;
#else
    (void)idx; (void)r;
    return 0;
#endif
}

uint8_t mui_dirty_bounds(mui_rect_t *r)
{
#if MUI_CFG_DIRTY_N > 0
    uint8_t i;
    mui_rect_t b;

    if (s_dirty_count == 0) {
        return 0;
    }
    b = s_dirty[0];
    for (i = 1; i < s_dirty_count; i++) {
        b = mui__rect_union(b, s_dirty[i]);
    }
    if (r != NULL) {
        *r = b;
    }
    return 1;
#else
    (void)r;
    return 0;
#endif
}

/* -------- 基础图元 -------- */

void mui_pixel_draw(int16_t x, int16_t y, uint16_t color)
{
    if (x < s_clip.x || y < s_clip.y || x >= s_clip.x2 || y >= s_clip.y2) {
        return;
    }
    MUI_DIRTY_ADD(x, y, 1, 1);
    mui_out_draw_pixel(x, y, color);
}

void mui_screen_clear(uint16_t color)
{
    /* 清屏 = 填充当前裁剪区；需要真清全屏时先 mui_reset_clip() */
    mui_rect_fill(0, 0, mui_screen_w, mui_screen_h, color);
}

void mui_rect_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    if (!mui__clip_rect(&x, &y, &w, &h)) {
        return;
    }
    MUI_DIRTY_ADD(x, y, w, h);
    mui_out_fill_rect(x, y, w, h, color);
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
    int16_t stride = w;      /* 原始行宽：源偏移必须按它算，不能用裁剪后的 w */

    if (w < 1 || h < 1 || data == NULL) {
        return;
    }
    if (!mui__clip_rect(&x, &y, &w, &h)) {
        return;
    }
    MUI_DIRTY_ADD(x, y, w, h);
    mui_out_draw_bitmap(x, y, w, h,
                        data + (int32_t)(y - oy) * stride + (x - ox), stride);
}

void mui_image_draw_key(int16_t x, int16_t y, int16_t w, int16_t h,
                         const uint16_t *data, uint16_t key)
{
    int16_t ox = x, oy = y;
    int16_t stride = w;      /* 原始行宽 */
    int16_t row, col, seg_start;
    int16_t sx, sy;

    if (w < 1 || h < 1 || data == NULL) {
        return;
    }
    if (!mui__clip_rect(&x, &y, &w, &h)) {
        return;
    }
    sx = (int16_t)(x - ox);  /* 源子图左上角偏移（>= 0） */
    sy = (int16_t)(y - oy);

    MUI_DIRTY_ADD(x, y, w, h);
    /* -------- 逐行扫描：不透明像素合并成连续段批量写 -------- */
    for (row = 0; row < h; row++) {
        const uint16_t *line = data + (int32_t)(sy + row) * stride + sx;

        seg_start = -1;
        for (col = 0; col < w; col++) {
            if (line[col] != key) {
                if (seg_start < 0) {
                    seg_start = col;
                }
            } else if (seg_start >= 0) {
                mui_out_draw_bitmap((int16_t)(x + seg_start), (int16_t)(y + row),
                                    (int16_t)(col - seg_start), 1,
                                    &line[seg_start], stride);
                seg_start = -1;
            }
        }
        if (seg_start >= 0) {
            mui_out_draw_bitmap((int16_t)(x + seg_start), (int16_t)(y + row),
                                (int16_t)(w - seg_start), 1,
                                &line[seg_start], stride);
        }
    }
}

void mui_image_draw_mask(int16_t x, int16_t y, int16_t w, int16_t h,
                          const uint8_t *data, uint16_t fg, uint16_t bg)
{
    int16_t ox = x, oy = y;
    int16_t stride = w;      /* 原始行宽：裁剪后不能再用 w 反推行距 */
    int16_t row, col;
    int16_t sx, sy;

    if (w < 1 || h < 1 || data == NULL) {
        return;
    }
    if (!mui__clip_rect(&x, &y, &w, &h)) {
        return;
    }
    sx = (int16_t)(x - ox);  /* 源子图左上角偏移（>= 0） */
    sy = (int16_t)(y - oy);

    MUI_DIRTY_ADD(x, y, w, h);   /* 整块记脏一次，内部循环不再逐像素记 */
    for (row = 0; row < h; row++) {
        const uint8_t *line = data + (int32_t)(sy + row) * stride + sx;
        for (col = 0; col < w; col++) {
            uint8_t a = line[col];

            if (a < 8) {
                continue;
            }
            /* 已裁剪到屏幕内，直接写出（免去逐像素再判裁剪与脏区） */
            mui_out_draw_pixel((int16_t)(x + col), (int16_t)(y + row),
                               a >= 248 ? fg : mui_color_mix(fg, bg, a));
        }
    }
}

/* -------- 位图缩放 / 九宫格 -------- */

/**
 * @brief 内部：把源图的一段矩形最近邻缩放到目标矩形（可选色键透明）
 * @param stride 源图整幅宽度（行距）
 * @param sx,sy  源子矩形左上角
 * @param swn,shn 源子矩形宽高
 */
static void img_blit_scaled(int16_t dx, int16_t dy, int16_t dw, int16_t dh,
                            const uint16_t *data, int16_t stride,
                            int16_t sx, int16_t sy, int16_t swn, int16_t shn,
                            int key_on, uint16_t key)
{
    int16_t j, i;

    if (dw < 1 || dh < 1 || swn < 1 || shn < 1 || data == NULL) {
        return;
    }
    for (j = 0; j < dh; j++) {
        int16_t ry = (int16_t)(dy + j);
        int16_t v = (int16_t)(((int32_t)j * shn) / dh);
        const uint16_t *line = data + (int32_t)(sy + v) * stride + sx;

        for (i = 0; i < dw; i++) {
            int16_t rx = (int16_t)(dx + i);
            int16_t u = (int16_t)(((int32_t)i * swn) / dw);
            uint16_t c = line[u];

            if (key_on && c == key) {
                continue;
            }
            mui_pixel_draw(rx, ry, c);
        }
    }
}

void mui_image_draw_scaled(int16_t x, int16_t y, int16_t dw, int16_t dh,
                           int16_t sw, int16_t sh, const uint16_t *data)
{
    img_blit_scaled(x, y, dw, dh, data, sw, 0, 0, sw, sh, 0, 0);
}

void mui_image_draw_scaled_key(int16_t x, int16_t y, int16_t dw, int16_t dh,
                               int16_t sw, int16_t sh, const uint16_t *data,
                               uint16_t key)
{
    img_blit_scaled(x, y, dw, dh, data, sw, 0, 0, sw, sh, 1, key);
}

/** @brief 内部：九宫格核心（可选色键） */
static void img_nine(int16_t x, int16_t y, int16_t w, int16_t h,
                     int16_t sw, int16_t sh, const uint16_t *data,
                     int16_t l, int16_t t, int16_t r, int16_t b,
                     int key_on, uint16_t key)
{
    int16_t cw, ch, dmx, dmy;

    if (w < 1 || h < 1 || sw < 1 || sh < 1 || data == NULL) {
        return;
    }
    if (l < 0) { l = 0; }
    if (t < 0) { t = 0; }
    if (r < 0) { r = 0; }
    if (b < 0) { b = 0; }
    if (l + r > sw) { r = (int16_t)(sw - l); if (r < 0) { r = 0; } }
    if (t + b > sh) { b = (int16_t)(sh - t); if (b < 0) { b = 0; } }
    /* 目标比边框还小：把边框裁剪到目标尺寸内，避免目标矩形反转 */
    if (l > w) { l = w; }
    if (r > w - l) { r = (int16_t)(w - l); }
    if (t > h) { t = h; }
    if (b > h - t) { b = (int16_t)(h - t); }

    cw = (int16_t)(sw - l - r);
    ch = (int16_t)(sh - t - b);
    dmx = (int16_t)(w - l - r);
    dmy = (int16_t)(h - t - b);

    /* 四角：不缩放 */
    img_blit_scaled(x, y, l, t, data, sw, 0, 0, l, t, key_on, key);
    img_blit_scaled((int16_t)(x + w - r), y, r, t, data, sw,
                    (int16_t)(sw - r), 0, r, t, key_on, key);
    img_blit_scaled(x, (int16_t)(y + h - b), l, b, data, sw, 0,
                    (int16_t)(sh - b), l, b, key_on, key);
    img_blit_scaled((int16_t)(x + w - r), (int16_t)(y + h - b), r, b, data, sw,
                    (int16_t)(sw - r), (int16_t)(sh - b), r, b, key_on, key);

    /* 四边：单向拉伸 */
    if (dmx > 0) {
        img_blit_scaled((int16_t)(x + l), y, dmx, t, data, sw, l, 0, cw, t,
                        key_on, key);
        img_blit_scaled((int16_t)(x + l), (int16_t)(y + h - b), dmx, b, data, sw,
                        l, (int16_t)(sh - b), cw, b, key_on, key);
    }
    if (dmy > 0) {
        img_blit_scaled(x, (int16_t)(y + t), l, dmy, data, sw, 0, t, l, ch,
                        key_on, key);
        img_blit_scaled((int16_t)(x + w - r), (int16_t)(y + t), r, dmy, data, sw,
                        (int16_t)(sw - r), t, r, ch, key_on, key);
    }

    /* 中心：双向拉伸 */
    if (dmx > 0 && dmy > 0 && cw > 0 && ch > 0) {
        img_blit_scaled((int16_t)(x + l), (int16_t)(y + t), dmx, dmy, data, sw,
                        l, t, cw, ch, key_on, key);
    }
}

void mui_image_draw_nine(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t sw, int16_t sh, const uint16_t *data,
                         int16_t l, int16_t t, int16_t r, int16_t b)
{
    img_nine(x, y, w, h, sw, sh, data, l, t, r, b, 0, 0);
}

void mui_image_draw_nine_key(int16_t x, int16_t y, int16_t w, int16_t h,
                             int16_t sw, int16_t sh, const uint16_t *data,
                             int16_t l, int16_t t, int16_t r, int16_t b,
                             uint16_t key)
{
    img_nine(x, y, w, h, sw, sh, data, l, t, r, b, 1, key);
}

/* -------- 颜色混合与抗锯齿 -------- */

/** @brief 无除法版 round(sum / 255)：t = sum + 128
 *  @note  已穷举验证 t-128 ∈ [0, 65535] 时与 (sum + 127) / 255 逐值一致；
 *         M0 无硬件除法器，一次 `/255` 约 50 周期，这里换成两次移位一次加法。 */
static uint32_t mix_div255(uint32_t t)
{
    return (t + (t >> 8)) >> 8;
}

uint16_t mui_color_mix(uint16_t fg, uint16_t bg, uint8_t alpha)
{
    uint32_t w_fg, w_bg, r, g, b;

    if (alpha == 0) {
        return bg;                      /* 全透明：整数式的结果本来就等于 bg */
    }
    if (alpha == 255) {
        return fg;                      /* 全覆盖：同上，省 3 次混色运算 */
    }
    w_fg = alpha;
    w_bg = 255u - alpha;
    r = mix_div255((((fg >> 11) & 0x1F) * w_fg + ((bg >> 11) & 0x1F) * w_bg) + 128u);
    g = mix_div255((((fg >> 5) & 0x3F) * w_fg + ((bg >> 5) & 0x3F) * w_bg) + 128u);
    b = mix_div255((((fg & 0x1F) * w_fg) + (bg & 0x1F) * w_bg) + 128u);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void mui_line_draw_aa(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      uint16_t fg, uint16_t bg)
{
#if !MUI_CFG_AA
    mui_line_draw(x0, y0, x1, y1, fg);
    return;
#endif
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

/**
 * @brief 画一段抗锯齿圆弧（内部；Wu 式 1px 描边带，半径 r±0.5 的覆盖率）
 *
 * 逐列求圆上点的精确位置（8.8 定点开方得 y 与小数部分 frac），把 255 的墨量按
 * (255-frac) / frac 分给"内侧/外侧"两个像素，各象限两两对称展开。
 * 抗锯齿圆、抗锯齿圆角矩形的四角都用它，保证两者观感一致。
 * @param cx,cy 圆心
 * @param r     半径
 * @param quad  象限掩码：bit0 左上 bit1 右上 bit2 右下 bit3 左下（与硬边版一致）
 * @param fg    前景色（描边色）
 * @param bg    背景色（直绘后端用；缓冲后端自动回读真实底色）
 * @note  与硬边版 `mui_circle_draw_arc` 相比，本函数在第 7/8 个八分圆边界处
 *        （45° 对角）不会补画"带外"的那个像素——那是 Wu 阶梯的正常特性，
 *        外观上是平滑斜线，不是断口。
 */
static void mui_circle_arc_aa(int16_t cx, int16_t cy, int16_t r, uint8_t quad,
                              uint16_t fg, uint16_t bg)
{
    int16_t x;
    int16_t x_max;

    if (r < 0 || quad == 0) {
        return;
    }
    if (r == 0) {
        mui_pixel_draw(cx, cy, fg);
        return;
    }
    x_max = (int16_t)mui_isqrt((uint32_t)r * r / 2);  /* 45° 处截止 */

    for (x = 0; x <= x_max; x++) {
        uint32_t y_fp = mui_sqrt_fp8((uint32_t)r * r - (uint32_t)x * x);
        int16_t y = (int16_t)(y_fp >> 8);
        uint8_t frac = (uint8_t)(y_fp & 0xFF);
        int16_t y1 = (int16_t)(y + 1);

        /* 圆上每点画"内侧 + 外侧"双像素，按象限掩码取点 */
        if (frac != 0xFF) {                     /* 内侧强度 = 255 - frac */
            uint16_t c = mui_color_mix(fg, bg, (uint8_t)(255 - frac));

            if (quad & 0x01) {                  /* 左上 */
                mui_pixel_draw((int16_t)(cx - x), (int16_t)(cy - y), c);
                mui_pixel_draw((int16_t)(cx - y), (int16_t)(cy - x), c);
            }
            if (quad & 0x02) {                  /* 右上 */
                mui_pixel_draw((int16_t)(cx + x), (int16_t)(cy - y), c);
                mui_pixel_draw((int16_t)(cx + y), (int16_t)(cy - x), c);
            }
            if (quad & 0x04) {                  /* 右下 */
                mui_pixel_draw((int16_t)(cx + x), (int16_t)(cy + y), c);
                mui_pixel_draw((int16_t)(cx + y), (int16_t)(cy + x), c);
            }
            if (quad & 0x08) {                  /* 左下 */
                mui_pixel_draw((int16_t)(cx - x), (int16_t)(cy + y), c);
                mui_pixel_draw((int16_t)(cx - y), (int16_t)(cy + x), c);
            }
        }
        if (frac != 0) {                        /* 外侧强度 = frac */
            uint16_t c = mui_color_mix(fg, bg, frac);

            if (quad & 0x01) {
                mui_pixel_draw((int16_t)(cx - x), (int16_t)(cy - y1), c);
                mui_pixel_draw((int16_t)(cx - y1), (int16_t)(cy - x), c);
            }
            if (quad & 0x02) {
                mui_pixel_draw((int16_t)(cx + x), (int16_t)(cy - y1), c);
                mui_pixel_draw((int16_t)(cx + y1), (int16_t)(cy - x), c);
            }
            if (quad & 0x04) {
                mui_pixel_draw((int16_t)(cx + x), (int16_t)(cy + y1), c);
                mui_pixel_draw((int16_t)(cx + y1), (int16_t)(cy + x), c);
            }
            if (quad & 0x08) {
                mui_pixel_draw((int16_t)(cx - x), (int16_t)(cy + y1), c);
                mui_pixel_draw((int16_t)(cx - y1), (int16_t)(cy + x), c);
            }
        }
    }
}

void mui_circle_draw_aa(int16_t cx, int16_t cy, int16_t r,
                        uint16_t fg, uint16_t bg)
{
#if !MUI_CFG_AA
    mui_circle_draw(cx, cy, r, fg);
    return;
#endif
    mui_circle_arc_aa(cx, cy, r, 0x0F, fg, bg);   /* 整圆 = 四个象限拼起来 */
}

#if MUI_CFG_AA
/** @brief 圆盘边缘像素：按覆盖率与 bg 混色落笔（n = 到圆心的距离平方） */
static void mui_disc_edge_pixel(int16_t x, int16_t y, int32_t n, int32_t r_f,
                                uint16_t fg, uint16_t bg)
{
    int32_t cov8 = 128 - ((int32_t)mui_sqrt_fp8((uint32_t)n) - r_f);

    if (cov8 <= 0) {
        return;
    }
    if (cov8 > 256) {
        cov8 = 256;
    }
    mui_pixel_draw(x, y, mui_color_mix(fg, bg, (uint8_t)((cov8 * 255) >> 8)));
}
#endif /* MUI_CFG_AA */

void mui_circle_fill_aa(int16_t cx, int16_t cy, int16_t r,
                        uint16_t fg, uint16_t bg)
{
#if !MUI_CFG_AA
    (void)bg;
    mui_circle_fill(cx, cy, r, fg);
#else
    int16_t dy, y0, y1;
    int32_t r_f, n_full;

    if (r < 0) { return; }
    r_f = (int32_t)r << 8;
    /* 满覆盖（cov8 >= 256）的充要条件：d <= r - 0.5px ⟺ 4*n <= (2r-1)^2。
     * 每行用一次开方解出满覆盖半宽，中间一次 hline 批量落笔，只有两侧
     * 过渡带逐像素混色 —— 满覆盖像素的结果本来就恒等于 fg（alpha=255）。 */
    n_full = ((int32_t)(2 * r - 1) * (2 * r - 1)) >> 2;
    y0 = (int16_t)(cy - r - 1);
    y1 = (int16_t)(cy + r + 1);
    for (dy = y0; dy <= y1; dy++) {
        int16_t yy = (int16_t)(dy - cy);
        int32_t dy2 = (int32_t)yy * yy;
        int32_t dxo2 = (int32_t)(r + 1) * (r + 1) - dy2;
        int16_t dxo, x, xfull = -1;
        int16_t xl_end, xr_beg;

        if (dxo2 < 0) { continue; }
        dxo = (int16_t)mui_isqrt((uint32_t)dxo2);
        if (r >= 1 && dy2 <= n_full) {
            xfull = (int16_t)mui_isqrt((uint32_t)(n_full - dy2));
        }
        xl_end = (int16_t)(cx + dxo);              /* 默认整行走逐像素 */
        xr_beg = (int16_t)(cx + dxo + 1);          /* 右段为空 */
        if (xfull >= 0) {
            xl_end = (int16_t)(cx - xfull - 1);
            xr_beg = (int16_t)(cx + xfull + 1);
            mui_hline_draw((int16_t)(cx - xfull), dy,
                           (int16_t)(2 * xfull + 1), fg);
        }
        for (x = (int16_t)(cx - dxo); x <= xl_end; x++) {
            int16_t xx = (int16_t)(x - cx);
            mui_disc_edge_pixel(x, dy, (int32_t)xx * xx + dy2, r_f, fg, bg);
        }
        for (x = xr_beg; x <= (int16_t)(cx + dxo); x++) {
            int16_t xx = (int16_t)(x - cx);
            mui_disc_edge_pixel(x, dy, (int32_t)xx * xx + dy2, r_f, fg, bg);
        }
    }
#endif
}

/* -------- 抗锯齿圆角矩形 -------- */

/**
 * @brief sqrt(n) 的 8 位定点值（即 floor(sqrt(n) * 256)），无除法
 * @note  n >= 65536 时退化为 4 位小数以避开 n << 16 溢出
 *        （圆角半径 r <= 90 时总是走 8 位小数路径，精度与逐像素版一致）
 *        非 static：库内其它模块（如控件）也用得到，声明见 mui_math.h。
 */
uint32_t mui_sqrt_fp8(uint32_t n)
{
    if (n < 65536u) {
        return mui_isqrt(n << 16);
    }
    return mui_isqrt(n << 8) << 4;
}

/**
 * @brief 圆角在"距角圆心 dy 像素"处的半跨度（声明见 mui_math.h）
 */
void mui_corner_span(int16_t dy, int16_t r, int16_t *dxf, int16_t *dxp)
{
    uint32_t dy2;

    if (dy < 0) {
        dy = (int16_t)(-dy);
    }
    dy2 = (uint32_t)dy * (uint32_t)dy;
    if (dxf != NULL) {
        uint32_t v = (uint32_t)r * r;
        *dxf = (v > dy2) ? (int16_t)mui_isqrt(v - dy2) : 0;
    }
    if (dxp != NULL) {
        uint32_t v = 4u * (uint32_t)(r + 1) * (uint32_t)(r + 1);
        uint32_t d4 = 4u * dy2;
        *dxp = (v > d4) ? (int16_t)(mui_isqrt(v - d4) >> 1) : 0;
    }
}

/**
 * @brief 角区像素对圆角的覆盖率（声明见 mui_math.h）
 */
uint8_t mui_corner_alpha(int16_t ux, int16_t uy, int16_t r)
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
 * @brief 把 int16 写进缓冲（可选单字符后缀），返回写入长度（不含结尾 NUL）
 * @param buf    输出缓冲，容量必须 >= 8（"-32768" + 后缀 + NUL）
 * @param v      要格式化的值
 * @param suffix 追加字符（如 '%'）；传 '\0' 表示不追加
 * @note  除 10 是常数除法（编译器转乘法移位），不引入 stdio / 浮点
 */
int16_t mui_num16_fmt(char *buf, int16_t v, char suffix)
{
    char tmp[8];
    int16_t n = 0;
    int16_t i = 0;
    uint32_t u;

    if (v < 0) {
        buf[i++] = '-';
        u = (uint32_t)(-(int32_t)v);
    } else {
        u = (uint32_t)v;
    }
    do {
        tmp[n++] = (char)('0' + (char)(u % 10u));
        u /= 10u;
    } while (u != 0 && n < 7);
    while (n > 0) {
        buf[i++] = tmp[--n];
    }
    if (suffix != '\0') {
        buf[i++] = suffix;
    }
    buf[i] = '\0';
    return i;
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
#if !MUI_CFG_AA
    (void)bg;
    mui_round_rect_fill(x, y, w, h, r, fg);
    return;
#endif
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
            int16_t dxf;
            int16_t dxp;
            int16_t px;

            /* 跨度统一走 mui_corner_span()：与进度条填充共用同一套公式 */
            mui_corner_span(dy, r, &dxf, &dxp);

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

void mui_round_rect_draw_aa(int16_t x, int16_t y, int16_t w, int16_t h,
                            int16_t r, uint16_t fg, uint16_t bg)
{
#if !MUI_CFG_AA
    (void)bg;
    mui_round_rect_draw(x, y, w, h, r, fg);
    return;
#endif
    int16_t limit;

    if (w < 1 || h < 1) {
        return;
    }
    /* -------- 圆角半径限制到短边的一半（与硬边版同口径） -------- */
    limit = (w < h) ? (int16_t)(w / 2) : (int16_t)(h / 2);
    if (r > limit) {
        r = limit;
    }
    if (r < 0) {
        r = 0;
    }
    if (r == 0) {
        mui_rect_draw(x, y, w, h, fg);      /* 轴对齐直边本就无锯齿 */
        return;
    }

    /* -------- 四条直边：1px 轴对齐，覆盖率恒为 1，不需要混合 -------- */
    mui_hline_draw((int16_t)(x + r), y, (int16_t)(w - 2 * r), fg);
    mui_hline_draw((int16_t)(x + r), (int16_t)(y + h - 1),
                   (int16_t)(w - 2 * r), fg);
    mui_vline_draw(x, (int16_t)(y + r), (int16_t)(h - 2 * r), fg);
    mui_vline_draw((int16_t)(x + w - 1), (int16_t)(y + r),
                   (int16_t)(h - 2 * r), fg);

    /* -------- 四角圆弧：圆心与硬边版逐项一致，可直接替换 -------- */
    mui_circle_arc_aa((int16_t)(x + r), (int16_t)(y + r), r, 0x01, fg, bg);
    mui_circle_arc_aa((int16_t)(x + w - 1 - r), (int16_t)(y + r), r, 0x02,
                      fg, bg);
    mui_circle_arc_aa((int16_t)(x + w - 1 - r), (int16_t)(y + h - 1 - r), r,
                      0x04, fg, bg);
    mui_circle_arc_aa((int16_t)(x + r), (int16_t)(y + h - 1 - r), r, 0x08,
                      fg, bg);
}

/* -------- 圆弧 / 圆环扇区（annular sector） -------- */

/* 整数正弦表：0..90°（×256），5° 步进，线性插值到 1° */
static const int16_t MUI_SIN_TAB[19] = {
    0, 22, 44, 66, 88, 108, 128, 147, 164, 181,
    196, 210, 222, 232, 240, 247, 252, 255, 256
};

static int16_t mui_sinv(int16_t a)   /* a: 0..90 */
{
    int16_t i = (int16_t)(a / 5);
    int16_t f = (int16_t)(a % 5);
    if (i > 17) { i = 17; }
    return (int16_t)(MUI_SIN_TAB[i] + ((MUI_SIN_TAB[i + 1] - MUI_SIN_TAB[i]) * f) / 5);
}

static int16_t mui_sin_deg(int16_t deg)
{
    deg = (int16_t)(((deg % 360) + 360) % 360);
    switch (deg / 90) {
        case 0: return mui_sinv((int16_t)(deg % 90));
        case 1: return mui_sinv((int16_t)(90 - (deg % 90)));
        case 2: return (int16_t)(-mui_sinv((int16_t)(deg % 90)));
        default: return (int16_t)(-mui_sinv((int16_t)(90 - (deg % 90))));
    }
}

static int16_t mui_cos_deg(int16_t deg) { return mui_sin_deg((int16_t)(deg + 90)); }

void mui_ring_fill(int16_t cx, int16_t cy, int16_t rin, int16_t rout,
                   int16_t a0, int16_t a1, uint16_t color)
{
    int16_t dy, y0, y1;
    int32_t rin2, rout2, span;
    int16_t ca0, sa0, ca1, sa1;
    int8_t  full;

    if (rout <= 0 || rin >= rout) { return; }
    if (rin < 0) { rin = 0; }

    a0 = (int16_t)(((a0 % 360) + 360) % 360);
    a1 = (int16_t)(((a1 % 360) + 360) % 360);
    if (a1 <= a0) { a1 = (int16_t)(a1 + 360); }
    full = (int8_t)((a1 - a0 >= 360) ? 1 : 0);
    span = (int32_t)a1 - a0;

    rin2  = (int32_t)rin * rin;
    rout2 = (int32_t)rout * rout;
    ca0 = mui_cos_deg(a0); sa0 = mui_sin_deg(a0);
    ca1 = mui_cos_deg(a1); sa1 = mui_sin_deg(a1);

    y0 = (int16_t)(cy - rout);
    y1 = (int16_t)(cy + rout);
    for (dy = y0; dy <= y1; dy++) {
        int16_t yy = (int16_t)(dy - cy);
        int32_t dy2 = (int32_t)yy * yy;
        int32_t dxo2 = rout2 - dy2;
        int16_t dxo, x;
        if (dxo2 < 0) { continue; }
        dxo = (int16_t)mui_isqrt((uint32_t)dxo2);

        if (full) {
            /* 整圆/整环：该行是"外圆弦 − 内孔弦"的两段，直接用 hline 批量写。
             * 内孔半宽取 ceil(sqrt(rin²−dy²))，与逐像素的 r2 >= rin2 判据逐位等价。 */
            if (dy2 < rin2) {
                uint32_t rr = (uint32_t)(rin2 - dy2);
                int16_t dxi = (int16_t)mui_isqrt(rr);
                if ((uint32_t)dxi * (uint32_t)dxi < rr) {
                    dxi = (int16_t)(dxi + 1);
                }
                if (dxi > dxo) {
                    continue;                      /* 该行整段落在孔洞里 */
                }
                mui_hline_draw((int16_t)(cx - dxo), dy,
                               (int16_t)(dxo - dxi + 1), color);
                mui_hline_draw((int16_t)(cx + dxi), dy,
                               (int16_t)(dxo - dxi + 1), color);
                continue;
            }
            mui_hline_draw((int16_t)(cx - dxo), dy, (int16_t)(2 * dxo + 1), color);
            continue;
        }
        for (x = (int16_t)(cx - dxo); x <= (int16_t)(cx + dxo); x++) {
            int16_t xx = (int16_t)(x - cx);
            int32_t r2 = (int32_t)xx * xx + dy2;
            int32_t c0, c1;
            if (r2 > rout2) { continue; }
            if (r2 < rin2)  { continue; }
            c0 = (int32_t)ca0 * yy - (int32_t)sa0 * xx;
            c1 = (int32_t)xx * sa1 - (int32_t)yy * ca1;
            /* 角度判定：跨度<=180° 取两半平面之交，>180° 取并集（否则会取到补楔形） */
            if ((span <= 180) ? (c0 >= 0 && c1 >= 0) : (c0 >= 0 || c1 >= 0)) {
                mui_pixel_draw(x, dy, color);
            }
        }
    }
}

/* 由 8.8 有符号距离求 8.8 覆盖率（0..256）：coverage = clamp(0.5 - sd, 0, 1) */
static int32_t mui_sd_cov8(int32_t sd8)
{
    int32_t cov8 = 128 - sd8;
    if (cov8 <= 0) { return 0; }
    if (cov8 > 256) { cov8 = 256; }
    return cov8;
}

/* 角度归一：a0∈[0,360)，a1>a0；跨度>=360 记 full */
static void mui_arc_norm(int16_t *a0, int16_t *a1, int16_t *full)
{
    *a0 = (int16_t)(((*a0 % 360) + 360) % 360);
    *a1 = (int16_t)(((*a1 % 360) + 360) % 360);
    if (*a1 <= *a0) { *a1 = (int16_t)(*a1 + 360); }
    *full = (int16_t)((*a1 - *a0 >= 360) ? 1 : 0);
}

/**
 * @brief 圆弧绘制核心：形状 = { 到圆弧中心线(半径 r, 角度[a0,a1]) 的距离 <= 半厚 }
 *        该定义天然生成圆头端帽；整个形状用有符号距离做一致的抗锯齿。
 * @param aa 0=硬边（sd<=0 填色），1=按覆盖率与 bg 混色
 */
static void mui_arc_core(int16_t cx, int16_t cy, int16_t r, int16_t a0, int16_t a1,
                         int16_t thickness, int aa, uint16_t fg, uint16_t bg)
{
    int16_t dy, y0, y1, ca0, sa0, ca1, sa1, full;
    int32_t r_f, half_f, rout, span;
    int16_t e0x, e0y, e1x, e1y;
    uint32_t ro2p1;
    uint32_t ring_lo2, ring_hi2;    /* 径向过渡带的平方界（带外必不落墨） */
    int32_t cap_thr;                /* 端帽的切比雪夫剔除阈值（8.8） */

    if (r <= 0 || thickness <= 0) { return; }
    rout = (int32_t)r + (int32_t)((thickness + 1) / 2);

    mui_arc_norm(&a0, &a1, &full);
    span = (int32_t)a1 - a0;

    r_f    = (int32_t)r << 8;
    half_f = ((int32_t)thickness << 8) / 2;
    ca0 = mui_cos_deg(a0); sa0 = mui_sin_deg(a0);
    ca1 = mui_cos_deg(a1); sa1 = mui_sin_deg(a1);
    e0x = (int16_t)(((int32_t)r * ca0) >> 8);
    e0y = (int16_t)(((int32_t)r * sa0) >> 8);
    e1x = (int16_t)(((int32_t)r * ca1) >> 8);
    e1y = (int16_t)(((int32_t)r * sa1) >> 8);

    ro2p1 = (uint32_t)(rout + 1) * (uint32_t)(rout + 1);

    /* 开方很贵（M0 无除法器），先用整数界把"覆盖率必为 0"的像素剔掉，只让过渡带开方：
     * ① 径向（full 弧，或落在楔形内的像素）：r2 与 (中心线 ±(半厚+0.5px))² 比较；
     * ② 端帽（楔形外）：切比雪夫距离是欧氏距离的下界，够不着半个像素就直接跳过。
     * 两者都只剔除"必定不落墨"的像素，落墨结果与逐像素开方版逐位一致。 */
    {
        int32_t a_lo = r_f - half_f - 128;              /* 径向带内界（8.8，可负） */
        int32_t b_hi = r_f + half_f + 128;              /* 径向带外界（8.8） */
        int32_t ap = (a_lo > 0) ? (a_lo >> 8) : 0;      /* 下界取 floor（保守剔除） */
        int32_t bp = (b_hi >> 8) + 1;                   /* 上界取 ceil（保守剔除） */
        ring_lo2 = (uint32_t)(ap * ap);
        ring_hi2 = (uint32_t)(bp * bp);
        cap_thr  = aa ? (half_f + 128) : (half_f + 1);  /* m<<8 >= 它即无墨 */
    }

    y0 = (int16_t)(cy - rout - 1);
    y1 = (int16_t)(cy + rout + 1);
    for (dy = y0; dy <= y1; dy++) {
        int16_t yy = (int16_t)(dy - cy);
        int32_t dy2 = (int32_t)yy * yy;
        int32_t dxo2 = (int32_t)ro2p1 - dy2;
        int16_t dxo, x;
        if (dxo2 < 0) { continue; }
        dxo = (int16_t)mui_isqrt((uint32_t)dxo2);
        for (x = (int16_t)(cx - dxo); x <= (int16_t)(cx + dxo); x++) {
            int16_t xx = (int16_t)(x - cx);
            int32_t r2 = (int32_t)xx * xx + dy2;
            int32_t dist8 = -1;                 /* <0 表示尚未定出到形状的距离 */
            int32_t sd8, cov8;

            if ((uint32_t)r2 > ro2p1) { continue; }
            if (!full) {
                int32_t h0 = (int32_t)ca0 * yy - (int32_t)sa0 * xx;
                int32_t h1 = (int32_t)xx * sa1 - (int32_t)yy * ca1;
                int32_t ins = (span <= 180) ? (h0 >= 0 && h1 >= 0)
                                            : (h0 >= 0 || h1 >= 0);
                if (!ins) {
                    int32_t ax = xx - e0x, ay = yy - e0y;   /* 到端帽圆心的向量 */
                    int32_t bx = xx - e1x, by = yy - e1y;
                    int32_t axa = (ax < 0) ? -ax : ax;
                    int32_t aya = (ay < 0) ? -ay : ay;
                    int32_t bxa = (bx < 0) ? -bx : bx;
                    int32_t bya = (by < 0) ? -by : by;
                    int32_t m0 = (axa > aya) ? axa : aya;   /* 切比雪夫 <= 欧氏 */
                    int32_t m1 = (bxa > bya) ? bxa : bya;
                    int32_t m  = (m0 < m1) ? m0 : m1;       /* 两端帽都够不着 */
                    if ((m << 8) >= cap_thr) { continue; }
                    {
                        int32_t q0 = (int32_t)mui_sqrt_fp8((uint32_t)(ax * ax + ay * ay));
                        int32_t q1 = (int32_t)mui_sqrt_fp8((uint32_t)(bx * bx + by * by));
                        dist8 = (q0 < q1) ? q0 : q1;
                    }
                }
            }
            if (dist8 < 0) {                           /* 径向对齐中心线 */
                int32_t d_f;
                if ((uint32_t)r2 < ring_lo2 || (uint32_t)r2 > ring_hi2) { continue; }
                d_f = (int32_t)mui_sqrt_fp8((uint32_t)r2);
                dist8 = d_f - r_f;
                if (dist8 < 0) { dist8 = -dist8; }
            }
            sd8 = dist8 - half_f;
            if (!aa) {
                if (sd8 <= 0) { mui_pixel_draw(x, dy, fg); }
                continue;
            }
            cov8 = mui_sd_cov8(sd8);
            if (cov8 <= 0) { continue; }
            mui_pixel_draw(x, dy, mui_color_mix(fg, bg, (uint8_t)((cov8 * 255) >> 8)));
        }
    }
}

void mui_arc_draw(int16_t cx, int16_t cy, int16_t r, int16_t a0, int16_t a1,
                  int16_t thickness, uint16_t color)
{
    if (thickness <= 0) {
        mui_circle_draw(cx, cy, r, color);
        return;
    }
    mui_arc_core(cx, cy, r, a0, a1, thickness, 0, color, color);
}

void mui_arc_draw_aa(int16_t cx, int16_t cy, int16_t r, int16_t a0, int16_t a1,
                     int16_t thickness, uint16_t fg, uint16_t bg)
{
#if !MUI_CFG_AA
    (void)bg;
    mui_arc_draw(cx, cy, r, a0, a1, thickness, fg);
    return;
#endif
    if (thickness <= 0) {
        mui_circle_draw_aa(cx, cy, r, fg, bg);
        return;
    }
    mui_arc_core(cx, cy, r, a0, a1, thickness, 1, fg, bg);
}

void mui_ring_fill_aa(int16_t cx, int16_t cy, int16_t rin, int16_t rout,
                      int16_t a0, int16_t a1, uint16_t fg, uint16_t bg)
{
#if !MUI_CFG_AA
    (void)bg;
    mui_ring_fill(cx, cy, rin, rout, a0, a1, fg);
#else
    int16_t dy, y0, y1, ca0, sa0, ca1, sa1, full;
    int32_t rin_f, rout_f, span;
    int32_t rinm2, rinp2, routm2, routp2;   /* 4*r^2 阈值：(2r-1)^2 / (2r+1)^2 */
    uint32_t ro2p1;
    const int32_t BIG = 1 << 20;            /* 射线在背后时的"很远"哨兵 */

    if (rout <= 0 || rin >= rout) { return; }
    if (rin < 0) { rin = 0; }

    mui_arc_norm(&a0, &a1, &full);
    span = (int32_t)a1 - a0;

    rin_f  = (int32_t)rin << 8;
    rout_f = (int32_t)rout << 8;
    ca0 = mui_cos_deg(a0); sa0 = mui_sin_deg(a0);
    ca1 = mui_cos_deg(a1); sa1 = mui_sin_deg(a1);

    rinm2  = (int32_t)(2 * rin - 1) * (2 * rin - 1);    /* d < rin-0.5 */
    rinp2  = (int32_t)(2 * rin + 1) * (2 * rin + 1);    /* d > rin+0.5 */
    routm2 = (int32_t)(2 * rout - 1) * (2 * rout - 1);  /* d > rout-0.5 */
    routp2 = (int32_t)(2 * rout + 1) * (2 * rout + 1);  /* d < rout+0.5 */

    ro2p1 = (uint32_t)(rout + 1) * (uint32_t)(rout + 1);
    y0 = (int16_t)(cy - rout - 1);
    y1 = (int16_t)(cy + rout + 1);
    for (dy = y0; dy <= y1; dy++) {
        int16_t yy = (int16_t)(dy - cy);
        int32_t dy2 = (int32_t)yy * yy;
        int32_t dxo2 = (int32_t)ro2p1 - dy2;
        int16_t dxo, x;
        if (dxo2 < 0) { continue; }
        dxo = (int16_t)mui_isqrt((uint32_t)dxo2);
        for (x = (int16_t)(cx - dxo); x <= (int16_t)(cx + dxo); x++) {
            int16_t xx = (int16_t)(x - cx);
            int32_t r2 = (int32_t)xx * xx + dy2;
            int32_t q4 = r2 << 2;               /* 4*d^2：用整数比较代替开方 */
            int32_t dmin = 0, ins = 1, sd8, cov8;
            int16_t radial_full;

            /* 环带外 / 内孔：纯整数比较，直接剔除，不做开方 */
            if (q4 > routp2 || q4 < rinm2) { continue; }
            radial_full = (int16_t)((q4 >= rinp2 && q4 <= routm2) ? 1 : 0);

            if (!full) {
                int32_t h0 = (int32_t)ca0 * yy - (int32_t)sa0 * xx;
                int32_t h1 = (int32_t)xx * sa1 - (int32_t)yy * ca1;
                int32_t t0 = (int32_t)ca0 * xx + (int32_t)sa0 * yy;
                int32_t t1 = (int32_t)ca1 * xx + (int32_t)sa1 * yy;
                int32_t a0v = (h0 < 0) ? -h0 : h0;
                int32_t a1v = (h1 < 0) ? -h1 : h1;
                /* 只在本射线"正前方"才算贴近该边；否则给一个很远的哨兵，
                 * 避免点落在边界射线反向延长线上时误判为 0 距离而出现杂线 */
                int32_t d0 = (t0 >= 0) ? a0v : BIG;
                int32_t d1 = (t1 >= 0) ? a1v : BIG;
                dmin = (d0 < d1) ? d0 : d1;         /* 到最近边界的距离（8.8） */
                ins  = (span <= 180) ? (h0 >= 0 && h1 >= 0)
                                     : (h0 >= 0 || h1 >= 0);
                if (radial_full) {
                    if (dmin >= 128) {              /* 离角边 >= 0.5px：全覆盖或全剔除 */
                        if (ins) { mui_pixel_draw(x, dy, fg); }
                        continue;
                    }
                    sd8 = ins ? -dmin : dmin;       /* 仅角边过渡带，无需开方 */
                    cov8 = mui_sd_cov8(sd8);
                    if (cov8 > 0) {
                        mui_pixel_draw(x, dy,
                            mui_color_mix(fg, bg, (uint8_t)((cov8 * 255) >> 8)));
                    }
                    continue;
                }
            } else if (radial_full) {
                mui_pixel_draw(x, dy, fg);          /* 整环内部：直接纯色 */
                continue;
            }

            /* 到这里只剩内/外圆边界过渡带：需一次 8.8 定点开方 */
            {
                int32_t d_f = (int32_t)mui_sqrt_fp8((uint32_t)r2);
                int32_t sdb = rin_f - d_f;
                int32_t sdo = d_f - rout_f;
                sd8 = (sdb > sdo) ? sdb : sdo;
                if (!full) {
                    int32_t sd_w = ins ? -dmin : dmin;
                    if (sd_w > sd8) { sd8 = sd_w; }
                }
                cov8 = mui_sd_cov8(sd8);
                if (cov8 > 0) {
                    mui_pixel_draw(x, dy,
                        mui_color_mix(fg, bg, (uint8_t)((cov8 * 255) >> 8)));
                }
            }
        }
    }
#endif
}

/* -------- 动画支撑（声明见 mui_anim.h） -------- */

/* 毫秒时间基：now 供查询，delta 供每帧推进 */
static uint32_t s_tick_now = 0;
static uint32_t s_tick_last = 0;
static uint16_t s_tick_delta = 0;

void mui_tick_update(uint32_t now_ms)
{
    uint32_t d = now_ms - s_tick_last;   /* 无符号减法天然处理 32 位回绕 */

    s_tick_delta = (d > 0xFFFFu) ? 0xFFFFu : (uint16_t)d;
    s_tick_last = now_ms;
    s_tick_now = now_ms;
}

uint32_t mui_time_ms(void)
{
    return s_tick_now;
}

uint16_t mui_tick_delta(void)
{
    return s_tick_delta;
}

int16_t mui_ease(uint8_t type, int16_t t256)
{
    int32_t t;
    int32_t u;

    if (t256 < 0) {
        t256 = 0;
    } else if (t256 > 256) {
        t256 = 256;
    }
    t = t256;                            /* 8.8 定点：256 = 1.0 */

    switch (type) {
    case MUI_EASE_IN_QUAD:               /* t^2 */
        return (int16_t)((t * t) / 256);

    case MUI_EASE_OUT_QUAD:              /* 1-(1-t)^2 */
        u = 256 - t;
        return (int16_t)(256 - (u * u) / 256);

    case MUI_EASE_IN_OUT_QUAD:           /* 2t^2 / 1-2(1-t)^2 */
        if (t < 128) {
            return (int16_t)((2 * t * t) / 256);
        }
        u = 256 - t;
        return (int16_t)(256 - (2 * u * u) / 256);

    case MUI_EASE_IN_CUBIC:              /* t^3 */
        return (int16_t)((t * t * t) / 65536);

    case MUI_EASE_OUT_CUBIC:             /* 1-(1-t)^3 */
        u = 256 - t;
        return (int16_t)(256 - (u * u * u) / 65536);

    case MUI_EASE_IN_OUT_CUBIC:          /* 4t^3 / 1-4(1-t)^3 */
        if (t < 128) {
            return (int16_t)((4 * t * t * t) / 65536);
        }
        u = 256 - t;
        return (int16_t)(256 - (4 * u * u * u) / 65536);

    case MUI_EASE_OUT_BACK: {
        /* 1 + c3*(s-1)^3 + c1*(s-1)^2，c1=1.70158(≈436)，c3=c1+1(≈692)，8.8 定点 */
        int32_t u2;
        int32_t u3;

        u = t - 256;                     /* s-1：范围 -256..0 */
        u2 = (u * u) / 256;              /* (s-1)^2，8.8 */
        u3 = (u * u * u) / 65536;        /* (s-1)^3，8.8 */
        return (int16_t)(256 + (692 * u3) / 256 + (436 * u2) / 256);
    }

    case MUI_EASE_LINEAR:
    default:
        return (int16_t)t;
    }
}

void mui_anim_init(mui_anim_t *a, int32_t from, uint8_t ease)
{
    if (a == NULL) {
        return;
    }
    a->from = from;
    a->to = from;
    a->value = from;
    a->dur = 0;
    a->t = 0;
    a->ease = ease;
    a->done = 1;
}

void mui_anim_start(mui_anim_t *a, int32_t from, int32_t to, uint16_t dur_ms)
{
    if (a == NULL) {
        return;
    }
    a->from = from;
    a->to = to;
    a->value = from;
    a->dur = dur_ms;
    a->t = 0;
    a->done = 0;
    if (dur_ms == 0) {                   /* 零时长：立即到位 */
        a->value = to;
        a->done = 1;
    }
}

void mui_anim_to(mui_anim_t *a, int32_t to, uint16_t dur_ms)
{
    if (a == NULL) {
        return;
    }
    mui_anim_start(a, a->value, to, dur_ms);   /* 从当前值出发，避免跳变 */
}

void mui_anim_stop(mui_anim_t *a)
{
    if (a != NULL) {
        a->done = 1;
    }
}

void mui_anim_jump(mui_anim_t *a, int32_t value)
{
    if (a == NULL) {
        return;
    }
    a->from = value;
    a->to = value;
    a->value = value;
    a->t = 0;
    a->done = 1;
}

void mui_anim_update(mui_anim_t *a, uint16_t dt_ms)
{
    uint32_t t;
    int16_t e;

    if (a == NULL || a->done) {
        return;
    }
    t = (uint32_t)a->t + dt_ms;
    if (t >= a->dur) {
        t = a->dur;
        a->done = 1;
    }
    a->t = (uint16_t)t;
    e = mui_ease(a->ease, (int16_t)((t * 256) / a->dur));
    a->value = a->from + (int32_t)((a->to - a->from) * e) / 256;
}

int32_t mui_anim_value(const mui_anim_t *a)
{
    return (a != NULL) ? a->value : 0;
}

uint8_t mui_anim_done(const mui_anim_t *a)
{
    return (a != NULL) ? a->done : 1;
}
