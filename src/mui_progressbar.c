/**
 * @file mui_progressbar.c
 * @brief MUI 进度条控件实现（保留模式对象 · 只重画变化区间 · 可选圆角抗锯齿）
 *
 * 抗锯齿（style->aa）不改变"只重画变化区间"的策略：混色只发生在被重画的那几列/行上，
 * 每列/行 1 次 8.8 定点开方（mui_sqrt_fp8，逐位法、无除法）+ 最多 2 次颜色混合。
 */

#include "mui_progressbar.h"
#include "mui_math.h"   /* 库内数学工具：整数开方 / 8.8 定点开方 */

/* -------- 内部常量 -------- */

/** @brief 边框线宽（像素）：仅当存在"可见边框"时，填充才退让这么宽 */
#define PB_BORDER 1

/* -------- 默认配色 -------- */

const mui_progressbar_style_t mui_progressbar_style_default = {
    .bg        = MUI_RGB565(0xE8, 0xE8, 0xE8),   /* 浅灰空槽 */
    .fg        = MUI_RGB565(0x3F, 0x77, 0xB8),   /* 进度蓝 */
    .border    = MUI_RGB565(0x9A, 0x9A, 0x9A),   /* 中灰边 */
    .radius    = 0,                              /* 自动（h/4，限 min(w,h)/2） */
    .dir       = MUI_PROGRESSBAR_HORIZONTAL,
    .aa        = 1,                              /* 默认开圆角抗锯齿（描边 AA 版已具备） */
    .screen_bg = MUI_WHITE                       /* AA 混色用的页面底色；缓冲后端自动回读 */
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
 * @brief 内部：第 n 条扫描线到角圆心的距离
 *
 * 把圆角矩形的形状映射到逐列（水平条）或逐行（垂直条）扫描：
 * n 为沿主轴的下标（0 基），len 为主轴长度，r 为圆角半径。
 * @return 到角圆心的距离（像素）；0 表示该扫描线在中间直段（内缩量为 0）
 */
static int16_t pb_corner_dist(int16_t n, int16_t len, int16_t r)
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
    return (d > r) ? r : d;
}

/** @brief 内部：内区域圆角半径（外圆角内收一个边框；无边框时等于外半径） */
static int16_t pb_inner_radius(int16_t r, int16_t bw)
{
    int16_t ri = (int16_t)(r - bw);

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
 * @brief 内部：画一个角部边界像素（按覆盖率与"页面底色"混色）
 * @param x,y        像素坐标
 * @param du,dv      相对角圆心的偏移（**半像素单位的倍数**，即 2*(px-cx)、2*(py-cy)；
 *                   符号无关，内部按距离平方计算）
 * @param ri         圆角半径
 * @param fg         填充色
 * @param screen_bg  页面底色（填充压在底板上，故不能回读——回读会重复叠加底板墨量）
 */
static void pb_edge_pixel(int16_t x, int16_t y, int16_t du, int16_t dv,
                          int16_t ri, uint16_t fg, uint16_t screen_bg)
{
    uint8_t a = mui_corner_alpha(du, dv, ri);

    if (a == 0) {
        return;
    }
    mui_pixel_draw(x, y, (a == 255) ? fg : mui_color_mix(fg, screen_bg, a));
}

/**
 * @brief 内部：把"一行的横向区间"画成指定颜色（竖直条用）
 *
 * 区间分三段：[lp, lf) 与 (rf, rp] 是圆角过渡带（按覆盖率与 screen_bg 混色），
 * [lf, rf] 是满覆盖段（一次批量写）。擦除时把 color 传成槽色，就等于"把这段
 * 恢复成槽"——边界带同样按覆盖率混色，因此**槽的圆角轮廓不会被擦成硬台阶**。
 * @param row     行号、lp/lf/rf/rp/cxl/cxr 全部是**内区域内局部坐标**（ix/iy 为原点）
 * @param cxl,cxr 左/右角圆心所在列（局部）
 * @param dy      该行到角圆心的距离（像素）
 */
static void pb_paint_hspan(int16_t ix, int16_t iy, int16_t row,
                           int16_t lp, int16_t lf, int16_t rf, int16_t rp,
                           int16_t cxl, int16_t cxr, int16_t dy, int16_t ri,
                           uint16_t color, uint16_t screen_bg)
{
    int16_t px;

    for (px = lp; px < lf; px++) {
        pb_edge_pixel((int16_t)(ix + px), (int16_t)(iy + row),
                      (int16_t)(2 * (px - cxl)), (int16_t)(2 * dy),
                      ri, color, screen_bg);
    }
    if (rf >= lf) {
        mui_hline_draw((int16_t)(ix + lf), (int16_t)(iy + row),
                       (int16_t)(rf - lf + 1), color);
    }
    for (px = (int16_t)(rf + 1); px <= rp; px++) {
        pb_edge_pixel((int16_t)(ix + px), (int16_t)(iy + row),
                      (int16_t)(2 * (px - cxr)), (int16_t)(2 * dy),
                      ri, color, screen_bg);
    }
}

/**
 * @brief 内部：把"一列的竖向区间"画成指定颜色（水平条用）
 * @param col     列号、tp/tf/bf/bp/cyl/cyb 全部是**内区域内局部坐标**
 * @param cyl,cyb 上/下角圆心所在行（局部）
 * @param dx      该列到角圆心的距离（像素）
 */
static void pb_paint_vspan(int16_t ix, int16_t iy, int16_t col,
                           int16_t tp, int16_t tf, int16_t bf, int16_t bp,
                           int16_t cyl, int16_t cyb, int16_t dx, int16_t ri,
                           uint16_t color, uint16_t screen_bg)
{
    int16_t py;

    for (py = tp; py < tf; py++) {
        pb_edge_pixel((int16_t)(ix + col), (int16_t)(iy + py),
                      (int16_t)(2 * dx), (int16_t)(2 * (py - cyl)),
                      ri, color, screen_bg);
    }
    if (bf >= tf) {
        mui_vline_draw((int16_t)(ix + col), (int16_t)(iy + tf),
                       (int16_t)(bf - tf + 1), color);
    }
    for (py = (int16_t)(bf + 1); py <= bp; py++) {
        pb_edge_pixel((int16_t)(ix + col), (int16_t)(iy + py),
                      (int16_t)(2 * dx), (int16_t)(2 * (py - cyb)),
                      ri, color, screen_bg);
    }
}

/**
 * @brief 内部：重画水平条填充的指定列区间
 *
 * 逐列处理：先按「内区域圆角形状」把该列恢复成槽（旧填充残影一并清除），再画前景色。
 * 该列首尾的跨度用 mui_corner_span()/mui_corner_alpha() 计算 —— 与抗锯齿圆角
 * 矩形完全同一套公式，因此**填充轮廓与底板逐像素一致**：无边框（border == bg）
 * 且 value = 100 时填充把底板完全盖住，不会露出"填充比底板小一圈"的边。
 * 填充末端（水平条右端）保持直角不收圆角，避免"进度越短越像胶囊"。
 * @param ix,iy,iw,ih 内区域几何
 * @param f_new       新填充列数（0 表示清空）
 * @param lo,hi       需重画的列区间（相对内区域左边界，0 基，右开）
 * @param ri          内圆角半径上限
 * @param erase       非 0 时先擦底（增量刷新用）；内区域已是纯 bg 时传 0
 * @param aa          非 0 时该列首尾的边界像素带按覆盖率混色（抗锯齿）
 * @param bg          底板（槽）颜色
 * @param screen_bg   页面底色，供边界像素混色
 */
static void pb_paint_fill_h(int16_t ix, int16_t iy, int16_t iw, int16_t ih,
                            int16_t f_new, int16_t lo, int16_t hi,
                            int16_t ri, uint8_t erase, uint8_t aa,
                            uint16_t fg, uint16_t bg, uint16_t screen_bg)
{
    int16_t col;

    if (hi <= lo) {
        return;
    }

    for (col = lo; col < hi; col++) {
        int16_t dy = pb_corner_dist(col, iw, ri);   /* 到角圆心的列距 */
        int16_t dxf = ri;                           /* 满覆盖半跨度 */
        int16_t dxp = ri;                           /* 含过渡带的半跨度 */
        int16_t cyl = ri;                           /* 局部：上角圆心行 */
        int16_t cyb = (int16_t)(ih - 1 - ri);       /* 局部：下角圆心行 */
        int16_t tp, tf, bp, bf;

        if (dy > 0) {
            mui_corner_span(dy, ri, &dxf, &dxp);
        }
        if (!aa) {
            dxp = dxf;                              /* 不抗锯齿：不画过渡带 */
        }

        /* 该列的首尾（局部）：以上下两个角圆心为准各内收 dxf / dxp 像素 */
        tp = (int16_t)(cyl - dxp);
        tf = (int16_t)(cyl - dxf);
        bp = (int16_t)(cyb + dxp);
        bf = (int16_t)(cyb + dxf);
        if (tp < 0) { tp = 0; }
        if (bp > (int16_t)(ih - 1)) { bp = (int16_t)(ih - 1); }
        if (tf < tp) { tf = tp; }
        if (bf > bp) { bf = bp; }

        if (erase) {
            pb_paint_vspan(ix, iy, col, tp, tf, bf, bp, cyl, cyb, dy, ri,
                           bg, screen_bg);          /* 恢复成槽（带混色的轮廓） */
        }
        if (col >= f_new) {
            continue;
        }
        pb_paint_vspan(ix, iy, col, tp, tf, bf, bp, cyl, cyb, dy, ri,
                       fg, screen_bg);
    }
}

/**
 * @brief 内部：重画垂直条填充的指定行区间
 *
 * 与水平条对称（行下标自底部向上换算），跨度同样走 mui_corner_span()/
 * mui_corner_alpha()，故填充轮廓与底板逐像素一致；末端（顶端）保持直角。
 * @param ix,iy,iw,ih 内区域几何
 * @param f_new       新填充行数（0 表示清空）
 * @param lo,hi       需重画的行区间（相对内区域顶边界，0 基，右开）
 * @param ri          内圆角半径上限
 * @param erase       非 0 时先擦底（增量刷新用）
 * @param aa          非 0 时该行左右边界像素带按覆盖率混色（抗锯齿）
 * @param bg          底板（槽）颜色
 * @param screen_bg   页面底色，供边界像素混色
 */
static void pb_paint_fill_v(int16_t ix, int16_t iy, int16_t iw, int16_t ih,
                            int16_t f_new, int16_t lo, int16_t hi,
                            int16_t ri, uint8_t erase, uint8_t aa,
                            uint16_t fg, uint16_t bg, uint16_t screen_bg)
{
    int16_t row;

    if (hi <= lo) {
        return;
    }

    for (row = lo; row < hi; row++) {
        int16_t dy = pb_corner_dist(row, ih, ri);   /* 到角圆心的行距 */
        int16_t dxf = ri;
        int16_t dxp = ri;
        int16_t cxl = ri;                           /* 局部：左角圆心列 */
        int16_t cxr = (int16_t)(iw - 1 - ri);       /* 局部：右角圆心列 */
        int16_t lp, lf, rp, rf;

        if (dy > 0) {
            mui_corner_span(dy, ri, &dxf, &dxp);
        }
        if (!aa) {
            dxp = dxf;
        }

        /* 该行的左右端点（局部）：以左右两个角圆心为准各内收 dxf / dxp 像素 */
        lp = (int16_t)(cxl - dxp);
        lf = (int16_t)(cxl - dxf);
        rp = (int16_t)(cxr + dxp);
        rf = (int16_t)(cxr + dxf);
        if (lp < 0) { lp = 0; }
        if (rp > (int16_t)(iw - 1)) { rp = (int16_t)(iw - 1); }
        if (lf < lp) { lf = lp; }
        if (rf > rp) { rf = rp; }

        if (erase) {
            pb_paint_hspan(ix, iy, row, lp, lf, rf, rp, cxl, cxr, dy, ri,
                           bg, screen_bg);          /* 恢复成槽（带混色的轮廓） */
        }
        if ((int16_t)(ih - 1 - row) >= f_new) {     /* 该行还没填充到 */
            continue;
        }
        pb_paint_hspan(ix, iy, row, lp, lf, rf, rp, cxl, cxr, dy, ri,
                       fg, screen_bg);
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
    int16_t r, ix, iy, iw, ih, ri, f_new, lo, hi, bw;
    uint8_t erase = 1;
    uint8_t aa = (uint8_t)(s->aa ? 1 : 0);
    uint8_t stroke;

    r = pb_radius(pb->w, pb->h, s);
    /* border 与 bg 同色 = 视觉无边框：既不画描边，填充也铺满整个形状。
     * 这一点很重要——"只有填充、没有描边"的设计（例如按位图 1:1 复刻的胶囊）
     * 要求填充覆盖整块底板；若仍按线宽退让 1px，边沿会露出一圈槽色，
     * 看起来就像"填充比底板小一号"。 */
    stroke = (uint8_t)((s->border != s->bg) ? 1 : 0);

    if (f_old < 0) {
        erase = 0;          /* 底板刚重画，内区域已是纯 bg，无需擦除 */
        f_old = 0;
        if (aa) {
            /* 底板圆角带抗锯齿；描边也用抗锯齿版（硬边版会盖掉混色像素） */
            mui_round_rect_fill_aa(pb->x, pb->y, pb->w, pb->h, r, s->bg,
                                   s->screen_bg);
            if (stroke) {
                mui_round_rect_draw_aa(pb->x, pb->y, pb->w, pb->h, r,
                                       s->border, s->screen_bg);
            }
        } else {
            mui_round_rect_fill(pb->x, pb->y, pb->w, pb->h, r, s->bg);
            if (stroke) {
                mui_round_rect_draw(pb->x, pb->y, pb->w, pb->h, r, s->border);
            }
        }
    }

    bw = stroke ? (int16_t)PB_BORDER : 0;   /* 填充退让量 = 描边线宽（无描边则 0） */
    ix = (int16_t)(pb->x + bw);
    iy = (int16_t)(pb->y + bw);
    iw = (int16_t)(pb->w - 2 * bw);
    ih = (int16_t)(pb->h - 2 * bw);
    if (iw < 1 || ih < 1) {
        return 0;
    }

    ri = pb_inner_radius(r, bw);
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
        pb_paint_fill_v(ix, iy, iw, ih, f_new, lo, hi, ri, erase, aa,
                        s->fg, s->bg, s->screen_bg);
    } else {
        /* 受影响列区间：从新旧填充较短者往回 ri 列，到较长者 */
        lo = (int16_t)((f_old < f_new ? f_old : f_new) - ri);
        if (lo < 0) {
            lo = 0;
        }
        hi = (int16_t)(f_old > f_new ? f_old : f_new);
        pb_paint_fill_h(ix, iy, iw, ih, f_new, lo, hi, ri, erase, aa,
                        s->fg, s->bg, s->screen_bg);
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
