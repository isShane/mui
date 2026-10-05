/**
 * @file mui_slider.c
 * @brief MUI 滑块控件实现（保留模式 · 触摸拖动 · 可选抗锯齿）
 */

#include "mui_slider.h"
#include "mui_math.h"   /* 库内公共：整数开方 / 8.8 定点开方 / 数值格式化 */

const mui_slider_style_t mui_slider_style_default = {
    .track       = MUI_RGB565(0x33, 0x3D, 0x4E),
    .fill        = MUI_RGB565(0x04, 0xAA, 0xF4),
    .knob        = MUI_WHITE,
    .knob_border = MUI_RGB565(0x04, 0xAA, 0xF4),
    .screen_bg   = MUI_WHITE,
    .thickness   = 0,                 /* 自动：短边/4 */
    .knob_r      = 0,                 /* 自动：thickness*3/4 */
    .dir         = MUI_SLIDER_HORIZONTAL,
    .aa          = 1,
};

/* -------- 内部工具 -------- */

static int16_t sl_clamp(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

static int16_t sl_len(const mui_slider_t *s, const mui_slider_style_t *st)
{
    return (st->dir == MUI_SLIDER_HORIZONTAL) ? s->w : s->h;
}

/** @brief 值 → 主轴像素坐标（水平=x，垂直=y；垂直时上=max） */
static int16_t sl_pos(const mui_slider_t *s, const mui_slider_style_t *st,
                      int16_t v)
{
    int16_t len  = sl_len(s, st);
    int16_t span = (int16_t)(s->max - s->min);
    int16_t base = (st->dir == MUI_SLIDER_HORIZONTAL) ? s->x : s->y;
    int16_t p;

    if (len <= 1 || span <= 0) {
        return base;
    }
    p = (int16_t)(((int32_t)(v - s->min) * (len - 1)) / span);
    if (st->dir == MUI_SLIDER_HORIZONTAL) {
        return (int16_t)(base + p);
    }
    return (int16_t)(base + (len - 1 - p));
}

/** @brief 主轴像素坐标 → 值 */
static int16_t sl_val(const mui_slider_t *s, const mui_slider_style_t *st,
                      int16_t pos)
{
    int16_t len  = sl_len(s, st);
    int16_t span = (int16_t)(s->max - s->min);
    int16_t base = (st->dir == MUI_SLIDER_HORIZONTAL) ? s->x : s->y;
    int16_t p;

    if (len <= 1) {
        return s->min;
    }
    p = sl_clamp((int16_t)(pos - base), 0, (int16_t)(len - 1));
    if (st->dir == MUI_SLIDER_HORIZONTAL) {
        return (int16_t)(s->min + ((int32_t)span * p) / (len - 1));
    }
    return (int16_t)(s->max - ((int32_t)span * p) / (len - 1));
}

/**
 * @brief 内部：轨道厚度与圆头半径
 *
 * 绘制与命中判定必须用同一套参数，否则圆头外观与触摸热区会不一致
 * （例如 knob_r 自动推导时，命中区若另写兜底值就会比画出来的圆头小一圈）。
 * @param t  输出：轨道厚度（>= 2）
 * @param kr 输出：圆头半径（>= 2）
 */
static void sl_geom(const mui_slider_t *s, const mui_slider_style_t *st,
                    int16_t *t, int16_t *kr)
{
    int16_t tt = st->thickness > 0
        ? st->thickness
        : (int16_t)((st->dir == MUI_SLIDER_HORIZONTAL ? s->h : s->w) / 4);
    int16_t kk;

    if (tt < 2) { tt = 2; }
    kk = st->knob_r > 0 ? st->knob_r : (int16_t)(tt * 3 / 4);
    if (kk < 2) { kk = 2; }
    *t = tt;
    *kr = kk;
}

/**
 * @brief 内部：本控件实际占用的矩形（= 绘制前要擦除的范围）
 *
 * 圆头比轨道粗，会越出 mui_slider_t 的外接矩形，故实际占位要在各方向外扩
 * 圆头半径；布局时必须按这个范围避让相邻控件。绘制与 mui_slider_get_bounds
 * 共用本函数，保证"占位"与"擦除范围"永远一致。
 */
static void sl_dirty_rect(const mui_slider_t *s, const mui_slider_style_t *st,
                          int16_t *x, int16_t *y, int16_t *w, int16_t *h)
{
    int16_t t, kr, half;

    sl_geom(s, st, &t, &kr);
    half = (kr > t / 2) ? kr : (int16_t)(t / 2);

    if (st->dir == MUI_SLIDER_HORIZONTAL) {
        *x = (int16_t)(s->x - half);
        *y = (int16_t)(s->y + s->h / 2 - half - 1);
        *w = (int16_t)(s->w + 2 * half);
        *h = (int16_t)(2 * half + 3);
    } else {
        *x = (int16_t)(s->x + s->w / 2 - half - 1);
        *y = (int16_t)(s->y - half);
        *w = (int16_t)(2 * half + 3);
        *h = (int16_t)(s->h + 2 * half);
    }
}

/**
 * @brief 画滑块圆头的外盘：边色 aa 圆盘，但每个像素按其"真实底色"混色。
 *
 * 圆头比轨道粗：与轨道同宽的那一条（"带"）压在 填充/轨道 上，其余压在"页面底色"上。
 * 若统一按页面底色混，圆头与填充的交界会冒出一条黑线——故这里逐像素选底色。
 * 方向必须分开判：水平条自左向右填充、垂直条自下向上填充，圆头两侧的
 * 填充/轨道分布是**旋转 90°** 的关系，用错方向会让交界处混出暗边。
 */
static void slider_rim(uint8_t dir, int16_t cx, int16_t cy, int16_t kr, int16_t t,
                       uint16_t color, uint16_t fill, uint16_t track,
                       uint16_t screen_bg, uint8_t aa)
{
    int16_t half = (int16_t)(t / 2);
    int16_t y0 = (int16_t)(cy - kr - 1);
    int16_t y1 = (int16_t)(cy + kr + 1);
    int16_t dy;

    for (dy = y0; dy <= y1; dy++) {
        int16_t yy = (int16_t)(dy - cy);
        int32_t dy2 = (int32_t)yy * yy;
        int32_t dxo2 = (int32_t)(kr + 1) * (kr + 1) - dy2;
        int16_t dxo, x;
        if (dxo2 < 0) { continue; }
        dxo = (int16_t)mui_isqrt((uint32_t)dxo2);
        for (x = (int16_t)(cx - dxo); x <= (int16_t)(cx + dxo); x++) {
            int16_t xx = (int16_t)(x - cx);
            int32_t r2 = (int32_t)xx * xx + dy2;
            uint16_t bg;
            uint8_t in_band;

            /* 水平条：轨道横穿圆头（|yy| <= 半厚），圆心左侧=填充、右侧=轨道；
             * 垂直条：轨道纵穿圆头（|xx| <= 半厚），圆心上方=轨道、下方=填充。 */
            if (dir == MUI_SLIDER_HORIZONTAL) {
                in_band = (uint8_t)(yy >= -half && yy <= half);
                bg = (x <= cx) ? fill : track;
            } else {
                in_band = (uint8_t)(xx >= -half && xx <= half);
                bg = (dy <= cy) ? track : fill;
            }
            /* 圆头伸出轨道的那部分压在页面底色上：注册了图案回调就按图案取色 */
            bg = in_band ? bg : mui_aa_base_at(x, dy, screen_bg);
            if (!aa) {
                if (r2 <= (int32_t)kr * kr) {
                    mui_pixel_draw(x, dy, color);
                }
                continue;
            }
            {
                int32_t d_f = (int32_t)mui_sqrt_fp8((uint32_t)r2);
                int32_t cov8 = 128 - (d_f - ((int32_t)kr << 8));
                uint8_t a;
                if (cov8 <= 0) { continue; }
                if (cov8 > 256) { cov8 = 256; }
                a = (uint8_t)((cov8 * 255) >> 8);
                /* 底色：注册了图案回调就按图案取色，否则沿用上面按几何判出的底色
                 * （不改回读：圆头可能压在上一帧自己的墨迹上） */
                mui_pixel_draw(x, dy,
                               mui_color_mix(color,
                                             mui_aa_base_at(x, dy, bg), a));
            }
        }
    }
}

/* -------- 对象接口 -------- */

void mui_slider_init(mui_slider_t *s, int16_t x, int16_t y, int16_t w, int16_t h,
                     int16_t min, int16_t max, int16_t value,
                     const mui_slider_style_t *style)
{
    if (s == NULL) {
        return;
    }
    s->x = x; s->y = y; s->w = w; s->h = h;
    s->style = style;
    if (min > max) {
        int16_t t = min; min = max; max = t;
    }
    s->min = min;
    s->max = max;
    s->value = sl_clamp(value, min, max);
    s->dragging = 0;
    s->enabled = 1;
    s->visible = 1;
    s->show_value = 0;
    s->font = NULL;
}

void mui_slider_set_range(mui_slider_t *s, int16_t min, int16_t max)
{
    if (s == NULL) {
        return;
    }
    if (min > max) {
        int16_t t = min; min = max; max = t;
    }
    s->min = min;
    s->max = max;
    s->value = sl_clamp(s->value, min, max);
}

void mui_slider_set_value(mui_slider_t *s, int16_t value)
{
    if (s != NULL) {
        s->value = sl_clamp(value, s->min, s->max);
    }
}

int16_t mui_slider_get_value(const mui_slider_t *s)
{
    return (s != NULL) ? s->value : 0;
}

void mui_slider_set_style(mui_slider_t *s, const mui_slider_style_t *style)
{
    if (s != NULL) {
        s->style = style;
    }
}

void mui_slider_set_pos(mui_slider_t *s, int16_t x, int16_t y)
{
    if (s != NULL) {
        s->x = x; s->y = y;
    }
}

void mui_slider_set_visible(mui_slider_t *s, uint8_t visible)
{
    if (s != NULL) {
        s->visible = visible ? 1 : 0;
    }
}

void mui_slider_set_font(mui_slider_t *s, const mui_font_t *font,
                         uint8_t show_value)
{
    if (s != NULL) {
        s->font = font;
        s->show_value = show_value ? 1 : 0;
    }
}

void mui_slider_draw(const mui_slider_t *s)
{
    const mui_slider_style_t *st;
    int16_t t, kr, pos, cx, cy;
    uint8_t aa;

    if (s == NULL || !s->visible) {
        return;
    }
    st = s->style ? s->style : &mui_slider_style_default;
    sl_geom(s, st, &t, &kr);
    aa = (uint8_t)(st->aa && MUI_CFG_AA);

    /* 先按页面底色擦除本控件占用的区域：滑块圆头比轨道高，若只重画轨道，
     * 拖动时旧滑块会在轨道外留下拖影。故每次绘制前把整条（含圆头越界）
     * 擦成 screen_bg —— 因此滑块应放在与 screen_bg 同色的底上。 */
    {
        int16_t ex, ey, ew, eh;

        sl_dirty_rect(s, st, &ex, &ey, &ew, &eh);
        mui_rect_fill(ex, ey, ew, eh, st->screen_bg);
    }

    pos = sl_pos(s, st, s->value);

    if (st->dir == MUI_SLIDER_HORIZONTAL) {
        int16_t ty = (int16_t)(s->y + s->h / 2 - t / 2);
        int16_t fillw = (int16_t)(pos - s->x + 1);
        cx = pos;
        cy = (int16_t)(s->y + s->h / 2);
        if (fillw < 0) { fillw = 0; }

        if (aa) {
            mui_round_rect_fill_aa(s->x, ty, s->w, t, (int16_t)(t / 2),
                                   st->track, st->screen_bg);
        } else {
            mui_round_rect_fill(s->x, ty, s->w, t, (int16_t)(t / 2), st->track);
        }
        if (fillw > 0) {
            if (aa) {
                mui_round_rect_fill_aa(s->x, ty, fillw, t, (int16_t)(t / 2),
                                       st->fill, st->screen_bg);
            } else {
                mui_round_rect_fill(s->x, ty, fillw, t, (int16_t)(t / 2),
                                    st->fill);
            }
        }
    } else {
        int16_t tx = (int16_t)(s->x + s->w / 2 - t / 2);
        int16_t fillh = (int16_t)(s->y + s->h - pos);
        cx = (int16_t)(s->x + s->w / 2);
        cy = pos;
        if (fillh < 0) { fillh = 0; }

        if (aa) {
            mui_round_rect_fill_aa(tx, s->y, t, s->h, (int16_t)(t / 2),
                                   st->track, st->screen_bg);
        } else {
            mui_round_rect_fill(tx, s->y, t, s->h, (int16_t)(t / 2), st->track);
        }
        if (fillh > 0) {
            int16_t fy = (int16_t)(s->y + s->h - fillh);
            if (aa) {
                mui_round_rect_fill_aa(tx, fy, t, fillh, (int16_t)(t / 2),
                                       st->fill, st->screen_bg);
            } else {
                mui_round_rect_fill(tx, fy, t, fillh, (int16_t)(t / 2), st->fill);
            }
        }
    }

    /* 滑块（圆头）：外盘逐像素按"真实底色"混色（左=填充/右=轨道/上下=页面底色），
     * 内芯再叠一枚纯色圆盘。这样圆头与填充交界处不会出现黑线，边缘也平滑。 */
    if (st->knob_border != st->knob) {
        slider_rim(st->dir, cx, cy, kr, t, st->knob_border, st->fill, st->track,
                   st->screen_bg, aa);
        if (aa) {
            mui_circle_fill_aa(cx, cy, (int16_t)(kr - 1), st->knob,
                               st->knob_border);
        } else if (kr > 1) {
            mui_circle_fill(cx, cy, (int16_t)(kr - 1), st->knob);
        }
    } else {
        slider_rim(st->dir, cx, cy, kr, t, st->knob, st->fill, st->track,
                   st->screen_bg, aa);
    }

    /* 数值文字 */
    if (s->show_value && s->font != NULL) {
        char buf[8];
        int16_t tw;

        mui_num16_fmt(buf, s->value, '\0');       /* 不用 stdio：省 1.5~6KB Flash */
        tw = mui_text_width(buf, s->font, 1);
        if (st->dir == MUI_SLIDER_HORIZONTAL) {
            mui_text_draw((int16_t)(cx - tw / 2),
                          (int16_t)(s->y - s->font->line_height - 2),
                          buf, s->font, st->knob_border, st->screen_bg, 1);
        } else {
            mui_text_draw((int16_t)(cx + kr + 4),
                          (int16_t)(cy - s->font->line_height / 2),
                          buf, s->font, st->knob_border, st->screen_bg, 1);
        }
    }
}

uint8_t mui_slider_get_bounds(const mui_slider_t *s, mui_rect_t *r)
{
    const mui_slider_style_t *st;
    int16_t ex, ey, ew, eh;

    if (s == NULL || r == NULL) {
        return 0;
    }
    st = s->style ? s->style : &mui_slider_style_default;
    sl_dirty_rect(s, st, &ex, &ey, &ew, &eh);
    r->x  = ex;
    r->y  = ey;
    r->x2 = (int16_t)(ex + ew);
    r->y2 = (int16_t)(ey + eh);
    return 1;
}

uint8_t mui_slider_touch(mui_slider_t *s, mui_touch_event_t ev)
{
    const mui_slider_style_t *st;
    int16_t x, y, nv, ex, ey, ew, eh;
    uint8_t hit;

    if (s == NULL || !s->enabled || ev == MUI_TOUCH_NONE) {
        return 0;
    }
    st = s->style ? s->style : &mui_slider_style_default;
    mui_touch_get_xy(&x, &y);

    /* 命中区 = 实际占位（与绘制/擦除共用同一套几何），保证"看得见就点得到" */
    sl_dirty_rect(s, st, &ex, &ey, &ew, &eh);
    hit = mui_rect_contains(ex, ey, ew, eh, x, y);

    switch (ev) {
    case MUI_TOUCH_DOWN:
        if (!hit) {
            return 0;
        }
        s->dragging = 1;
        nv = sl_val(s, st, (st->dir == MUI_SLIDER_HORIZONTAL) ? x : y);
        if (nv != s->value) { s->value = nv; return 1; }
        return 0;

    case MUI_TOUCH_MOVE:
        if (!s->dragging) {
            return 0;
        }
        nv = sl_val(s, st, (st->dir == MUI_SLIDER_HORIZONTAL) ? x : y);
        if (nv != s->value) { s->value = nv; return 1; }
        return 0;

    case MUI_TOUCH_UP:
        s->dragging = 0;
        return 0;

    case MUI_TOUCH_CLICK:
        if (!hit) {
            s->dragging = 0;
            return 0;
        }
        nv = sl_val(s, st, (st->dir == MUI_SLIDER_HORIZONTAL) ? x : y);
        s->dragging = 0;
        if (nv != s->value) { s->value = nv; return 1; }
        return 0;

    default:
        return 0;
    }
}
