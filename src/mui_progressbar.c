/**
 * @file mui_progressbar.c
 * @brief MUI 进度条控件实现（保留模式对象 · 只重画变化区间）
 */

#include "mui_progressbar.h"

/* -------- 内部常量 -------- */

/** @brief 边框线宽（像素） */
#define PB_BORDER 1

/* -------- 默认配色 -------- */

const mui_progressbar_style_t mui_progressbar_style_default = {
    MUI_RGB565(0xE8, 0xE8, 0xE8),   /* bg：浅灰空槽 */
    MUI_RGB565(0x3F, 0x77, 0xB8),   /* fg：进度蓝 */
    MUI_RGB565(0x9A, 0x9A, 0x9A),   /* border：中灰边 */
    0,                              /* radius：自动（h/4） */
    MUI_PROGRESSBAR_HORIZONTAL               /* dir：水平 */
};

/* -------- 内部：几何计算 -------- */

/**
 * @brief 内部：计算外框圆角半径
 */
static int16_t pb_radius(int16_t w, int16_t h, const mui_progressbar_style_t *s)
{
    int16_t limit = (int16_t)((w < h ? w : h) / 2);
    int16_t r = s->radius > 0 ? s->radius : (int16_t)(h / 4);

    if (r > limit) {
        r = limit;
    }
    return r > 0 ? r : 0;
}

/**
 * @brief 内部：整数平方根（半径 <= 屏高，小循环足够）
 */
static int16_t pb_isqrt(int32_t v)
{
    int16_t x = 0;

    while ((int32_t)(x + 1) * (x + 1) <= v) {
        x++;
    }
    return x;
}

/**
 * @brief 内部：圆角矩形中第 n 条扫描线距两端的像素内缩量
 *
 * 把圆角矩形的形状映射到逐列（水平条）或逐行（垂直条）扫描：
 * n 为沿主轴的下标（0 基），len 为圆角矩形主轴长度，r 为圆角半径。
 * @return 该扫描线首尾各需内缩的像素数
 */
static int16_t pb_inset(int16_t n, int16_t len, int16_t r)
{
    int16_t d;

    if (r <= 0) {
        return 0;
    }
    if (n < r) {
        d = (int16_t)(r - n);
    } else if (n >= len - r) {
        d = (int16_t)(n - (len - r) + 1);
    } else {
        return 0;
    }
    if (d > r) {
        d = r;
    }
    return (int16_t)(r - pb_isqrt((int32_t)(r * r - d * d)));
}

/** @brief 内部：内区域圆角半径（外圆角内收一个边框） */
static int16_t pb_inner_radius(int16_t r)
{
    int16_t ri = (int16_t)(r - PB_BORDER);

    return ri > 0 ? ri : 0;
}

/** @brief 内部：由进度值求主轴填充长度（像素） */
static int16_t pb_fill_len(int16_t len, uint8_t value)
{
    if (len <= 0) {
        return 0;
    }
    if (value > 100) {
        value = 100;
    }
    return (int16_t)((int32_t)len * value / 100);
}

/* -------- 内部：填充绘制 -------- */

/**
 * @brief 内部：重画水平条填充的指定列区间
 *
 * 逐列处理：先按「内区域圆角形状」擦回底色（旧填充残影一并清除），再画
 * 前景色。填充内缩只跟随内区域形状，末端保持直角不收圆角，因此既不会
 * 溢出到底板圆角之外，也不会出现"填充越短两端越圆"的胶囊效果。
 * @param ix,iy,iw,ih 内区域几何
 * @param f_new       新填充列数（0 表示清空）
 * @param lo,hi       需重画的列区间（相对内区域左边界，0 基，右开）
 * @param ri          内圆角半径上限
 * @param erase       非 0 时先擦底（增量刷新用）；内区域已是纯 bg 时传 0
 */
static void pb_paint_fill_h(int16_t ix, int16_t iy, int16_t iw, int16_t ih,
                            int16_t f_new, int16_t lo, int16_t hi,
                            int16_t ri, uint8_t erase,
                            uint16_t fg, uint16_t bg)
{
    int16_t col, base, y0, hh;

    if (hi <= lo) {
        return;
    }

    for (col = lo; col < hi; col++) {
        base = pb_inset(col, iw, ri);      /* 内区域该列首尾内缩：填充与它对齐 */
        if (erase) {
            hh = (int16_t)(ih - 2 * base);
            if (hh > 0) {
                mui_vline_draw((int16_t)(ix + col), (int16_t)(iy + base), hh, bg);
            }
        }
        if (col >= f_new) {
            continue;
        }
        y0 = (int16_t)(iy + base);
        hh = (int16_t)(ih - 2 * base);
        if (hh > 0) {
            mui_vline_draw((int16_t)(ix + col), y0, hh, fg);
        }
    }
}

/**
 * @brief 内部：重画垂直条填充的指定行区间
 *
 * 与水平条对称：行下标自底部向上换算后再求内缩，填充末端（顶端）保持直角。
 * @param ix,iy,iw,ih 内区域几何
 * @param f_new       新填充行数（0 表示清空）
 * @param lo,hi       需重画的行区间（相对内区域顶边界，0 基，右开）
 * @param ri          内圆角半径上限
 * @param erase       非 0 时先擦底（增量刷新用）
 */
static void pb_paint_fill_v(int16_t ix, int16_t iy, int16_t iw, int16_t ih,
                            int16_t f_new, int16_t lo, int16_t hi,
                            int16_t ri, uint8_t erase,
                            uint16_t fg, uint16_t bg)
{
    int16_t row, d, base, x0, ww;

    if (hi <= lo) {
        return;
    }

    for (row = lo; row < hi; row++) {
        base = pb_inset(row, ih, ri);      /* 内区域该行左右内缩：填充与它对齐 */
        if (erase) {
            ww = (int16_t)(iw - 2 * base);
            if (ww > 0) {
                mui_hline_draw((int16_t)(ix + base), (int16_t)(iy + row), ww, bg);
            }
        }
        d = (int16_t)(ih - 1 - row);       /* 距内区域底部的行数：0 = 最底行 */
        if (d >= f_new) {
            continue;
        }
        x0 = (int16_t)(ix + base);
        ww = (int16_t)(iw - 2 * base);
        if (ww > 0) {
            mui_hline_draw(x0, (int16_t)(iy + row), ww, fg);
        }
    }
}

/* -------- 内部：绘制主流程 -------- */

/**
 * @brief 内部：绘制进度条
 * @param pb    进度条对象
 * @param f_old 旧填充长度（主轴像素）；传 -1 表示需连底板边框一起全量重画
 * @return 本次绘制后的填充长度（供调用者同步增量缓存）
 */
static int16_t pb_repaint(const mui_progressbar_t *pb, int16_t f_old)
{
    const mui_progressbar_style_t *s =
        pb->style ? pb->style : &mui_progressbar_style_default;
    int16_t r, ix, iy, iw, ih, ri, f_new, lo, hi;
    uint8_t erase = 1;

    r = pb_radius(pb->w, pb->h, s);
    if (f_old < 0) {
        erase = 0;          /* 底板刚重画，内区域已是纯 bg，无需擦除 */
        f_old = 0;
        mui_round_rect_fill(pb->x, pb->y, pb->w, pb->h, r, s->bg);
        mui_round_rect_draw(pb->x, pb->y, pb->w, pb->h, r, s->border);
    }

    ix = (int16_t)(pb->x + PB_BORDER);
    iy = (int16_t)(pb->y + PB_BORDER);
    iw = (int16_t)(pb->w - 2 * PB_BORDER);
    ih = (int16_t)(pb->h - 2 * PB_BORDER);
    if (iw < 1 || ih < 1) {
        return 0;
    }

    ri = pb_inner_radius(r);
    f_new = pb_fill_len(s->dir == MUI_PROGRESSBAR_VERTICAL ? ih : iw, pb->value);

    if (s->dir == MUI_PROGRESSBAR_VERTICAL) {
        /* 受影响行区间：从新旧填充的最高点，到较低者再往下 ri 行（圆角区） */
        lo = (int16_t)(ih - (f_old > f_new ? f_old : f_new));
        hi = (int16_t)(ih - (f_old < f_new ? f_old : f_new) + ri);
        if (lo < 0) {
            lo = 0;
        }
        if (hi > ih) {
            hi = ih;
        }
        pb_paint_fill_v(ix, iy, iw, ih, f_new, lo, hi, ri, erase, s->fg, s->bg);
    } else {
        /* 受影响列区间：从新旧填充较短者往回 ri 列，到较长者 */
        lo = (int16_t)((f_old < f_new ? f_old : f_new) - ri);
        if (lo < 0) {
            lo = 0;
        }
        hi = (int16_t)(f_old > f_new ? f_old : f_new);
        pb_paint_fill_h(ix, iy, iw, ih, f_new, lo, hi, ri, erase, s->fg, s->bg);
    }

    return f_new;
}

/* -------- OO 进度条：对象接口 -------- */

void mui_progressbar_init(mui_progressbar_t *pb, int16_t x, int16_t y,
                          int16_t w, int16_t h,
                          const mui_progressbar_style_t *style)
{
    if (pb == NULL) {
        return;
    }
    pb->x = x;
    pb->y = y;
    pb->w = w;
    pb->h = h;
    pb->style = style;      /* NULL 在绘制时 fallback 默认 */
    pb->value = 0;
    pb->visible = 1;
    pb->drawn = 0;
    pb->last_fill = 0;
}

void mui_progressbar_set_value(mui_progressbar_t *pb, uint8_t value)
{
    if (pb == NULL) {
        return;
    }
    if (value > 100) {
        value = 100;
    }
    if (pb->drawn && value == pb->value) {
        return;             /* 进度未变：不擦不画 */
    }

    pb->value = value;
    if (!pb->visible) {
        return;
    }

    if (!pb->drawn) {
        pb->last_fill = pb_repaint(pb, -1);
        pb->drawn = 1;
    } else {
        pb->last_fill = pb_repaint(pb, pb->last_fill);
    }
}

void mui_progressbar_set_pos(mui_progressbar_t *pb, int16_t x, int16_t y)
{
    const mui_progressbar_style_t *s;

    if (pb == NULL || !pb->drawn) {
        return;
    }

    s = pb->style ? pb->style : &mui_progressbar_style_default;
    mui_rect_fill((int16_t)(pb->x - 1), (int16_t)(pb->y - 1),
                  (int16_t)(pb->w + 2), (int16_t)(pb->h + 2), s->bg);
    pb->x = x;
    pb->y = y;
    pb->last_fill = pb_repaint(pb, -1);
}

void mui_progressbar_set_visible(mui_progressbar_t *pb, uint8_t visible)
{
    if (pb == NULL) {
        return;
    }
    pb->visible = visible ? 1 : 0;
    if (pb->visible) {
        pb->last_fill = pb_repaint(pb, -1);   /* 重新显示：全量重绘 */
        pb->drawn = 1;
    } else {
        pb->drawn = 0;                        /* 隐藏不擦除，下次显示走全量 */
    }
}

void mui_progressbar_draw(const mui_progressbar_t *pb)
{
    if (pb == NULL || !pb->visible) {
        return;
    }
    (void)pb_repaint(pb, -1);
}
