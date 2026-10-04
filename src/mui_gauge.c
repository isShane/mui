/**
 * @file mui_gauge.c
 * @brief MUI 仪表盘控件实现
 */

#include "mui_gauge.h"

const mui_gauge_style_t mui_gauge_style_default = {
    MUI_RGB565(0x33, 0x3D, 0x4E),   /* track：深灰蓝轨道 */
    MUI_RGB565(0x04, 0xAA, 0xF4),   /* fill ：主题蓝 */
    MUI_WHITE,                      /* text ：白字 */
    MUI_WHITE,                      /* bg   ：默认白底（AA 混色用） */
    0,                              /* thick：自动 */
    135, 405,                       /* a0 / a1：270° 扫角 */
    1,                              /* aa   ：开 */
    1,                              /* show_value */
    0                               /* percent */
};

/** @brief 内部：实际环宽（thick<=0 时取外半径的 1/5，至少 1px） */
static int16_t gauge_thick(const mui_gauge_t *g, const mui_gauge_style_t *s)
{
    int16_t t = (s->thick > 0) ? s->thick : (int16_t)(g->r / 5);

    if (t < 1) {
        t = 1;
    }
    return t;
}

/** @brief 内部：把整数（可选百分号后缀）写入缓冲，返回长度；不用 stdio */
static int16_t gauge_fmt(char *buf, int16_t v, uint8_t percent)
{
    char tmp[8];
    int16_t n = 0;
    int16_t i = 0;
    uint8_t neg = 0;
    uint32_t u;

    if (v < 0) {
        neg = 1;
        u = (uint32_t)(-(int32_t)v);
    } else {
        u = (uint32_t)v;
    }
    do {
        tmp[n++] = (char)('0' + (char)(u % 10u));
        u /= 10u;
    } while (u != 0 && n < 7);
    if (neg) {
        buf[i++] = '-';
    }
    while (n > 0) {
        buf[i++] = tmp[--n];
    }
    if (percent) {
        buf[i++] = '%';
    }
    buf[i] = '\0';
    return i;
}

int16_t mui_gauge_value_angle(const mui_gauge_t *g, int16_t value)
{
    const mui_gauge_style_t *s;
    int32_t span;
    int32_t off;

    if (g == NULL) {
        return 0;
    }
    s = g->style ? g->style : &mui_gauge_style_default;
    if (s->a1 <= s->a0 || g->max <= g->min) {
        return s->a0;
    }
    span = (int32_t)s->a1 - s->a0;
    if (value < g->min) {
        value = g->min;
    }
    if (value > g->max) {
        value = g->max;
    }
    off = (int32_t)(value - g->min) * span / (int32_t)(g->max - g->min);
    return (int16_t)(s->a0 + off);
}

void mui_gauge_init(mui_gauge_t *g, int16_t cx, int16_t cy, int16_t r,
                    int16_t min, int16_t max, int16_t value,
                    const mui_gauge_style_t *style)
{
    if (g == NULL) {
        return;
    }
    g->cx = cx;
    g->cy = cy;
    g->r = r;
    g->style = style;
    g->font = NULL;
    if (max <= min) {
        max = (int16_t)(min + 1);
    }
    g->min = min;
    g->max = max;
    if (value < min) { value = min; }
    if (value > max) { value = max; }
    g->value = value;
    g->visible = 1;
    g->drawn = 0;
    g->last_value = value;
    g->text_drawn = 0;
}

/** @brief 内部：用底色擦掉上一次画在中心的数值（未画过则什么都不做） */
static void gauge_erase_text(const mui_gauge_t *g, uint16_t bg)
{
    if (!g->text_drawn) {
        return;
    }
    /* 四周各外扩 1px：字形的抗锯齿像素可能略微探出整格边界 */
    mui_rect_fill((int16_t)(g->text_rect.x - 1), (int16_t)(g->text_rect.y - 1),
                  (int16_t)(g->text_rect.x2 - g->text_rect.x + 2),
                  (int16_t)(g->text_rect.y2 - g->text_rect.y + 2), bg);
}

void mui_gauge_set_value(mui_gauge_t *g, int16_t value)
{
    if (g == NULL) {
        return;
    }
    if (value < g->min) { value = g->min; }
    if (value > g->max) { value = g->max; }
    g->value = value;
}

int16_t mui_gauge_get_value(const mui_gauge_t *g)
{
    return (g != NULL) ? g->value : 0;
}

void mui_gauge_set_range(mui_gauge_t *g, int16_t min, int16_t max)
{
    if (g == NULL) {
        return;
    }
    if (max <= min) {
        max = (int16_t)(min + 1);
    }
    g->min = min;
    g->max = max;
    if (g->value < min) { g->value = min; }
    if (g->value > max) { g->value = max; }
    g->drawn = 0;                      /* 映射关系变了：强制重绘 */
}

void mui_gauge_set_style(mui_gauge_t *g, const mui_gauge_style_t *style)
{
    if (g != NULL) {
        g->style = style;
        g->drawn = 0;
    }
}

void mui_gauge_set_font(mui_gauge_t *g, const mui_font_t *font)
{
    if (g != NULL) {
        g->font = font;
        g->drawn = 0;
    }
}

void mui_gauge_set_pos(mui_gauge_t *g, int16_t cx, int16_t cy)
{
    if (g != NULL && (g->cx != cx || g->cy != cy)) {
        g->cx = cx;
        g->cy = cy;
        g->drawn = 0;
    }
}

void mui_gauge_set_radius(mui_gauge_t *g, int16_t r)
{
    if (g != NULL && g->r != r) {
        g->r = r;
        g->drawn = 0;
    }
}

void mui_gauge_set_visible(mui_gauge_t *g, uint8_t visible)
{
    if (g == NULL) {
        return;
    }
    visible = (uint8_t)(visible ? 1 : 0);
    if (g->visible != visible) {
        g->visible = visible;
        g->drawn = 0;                  /* 重新显示时全量重绘 */
    }
}

void mui_gauge_draw(mui_gauge_t *g)
{
    const mui_gauge_style_t *s;
    int16_t rin, rout, a_end;
    uint8_t aa;

    if (g == NULL || !g->visible) {
        return;
    }
#if !MUI_CFG_IS_STRIP
    if (g->drawn && g->last_value == g->value) {
        return;                        /* 值未变 → 不写屏 */
    }
    /* 条带后端下每帧按带重放，屏上旧内容不复存在 → 一律全量重绘 */
#endif

    s = g->style ? g->style : &mui_gauge_style_default;
    rout = g->r;
    if (rout < 2) {
        return;
    }
    rin = (int16_t)(rout - gauge_thick(g, s));
    if (rin < 1) {
        rin = 1;
    }
    if (rin >= rout) {
        rin = (int16_t)(rout - 1);
    }
#if MUI_CFG_AA
    aa = s->aa;
#else
    aa = 0;
#endif

    /* 轨道：整环 */
    if (aa) {
        mui_ring_fill_aa(g->cx, g->cy, rin, rout, 0, 360, s->track, s->bg);
    } else {
        mui_ring_fill(g->cx, g->cy, rin, rout, 0, 360, s->track);
    }

    /* 进度弧 */
    a_end = mui_gauge_value_angle(g, g->value);
    if (a_end > s->a0) {
        if (aa) {
            mui_ring_fill_aa(g->cx, g->cy, rin, rout, s->a0, a_end, s->fill, s->bg);
        } else {
            mui_ring_fill(g->cx, g->cy, rin, rout, s->a0, a_end, s->fill);
        }
    }

    /* 中心数值（复用文本对齐：内孔矩形内居中）
     *
     * 注意：内孔在圆环的覆盖范围之外，重画圆环**擦不掉**旧数字。而数值是
     * 逐帧变化的（缓动/实时数据），若不清旧值，新数字会直接压在旧数字上，
     * 宽度一变就叠成一团糊字。故这里记下上次数值占的矩形，重绘前先按它擦除。 */
    if (s->show_value && g->font != NULL) {
        char buf[12];
        int16_t tw, th, tx, ty;

        if (s->percent) {
            int32_t p = 0;
            if (g->max > g->min) {
                p = ((int32_t)(g->value - g->min) * 100) / (g->max - g->min);
            }
            gauge_fmt(buf, (int16_t)p, 1);
        } else {
            gauge_fmt(buf, g->value, 0);
        }

        /* 用与 mui_text_draw_rect 同一套对齐函数算出占位，保证擦除框==绘制框 */
        tw = mui_text_width(buf, g->font, 1);
        th = mui_text_height(g->font, 1, 0);
        tx = mui_text_align_x((int16_t)(g->cx - rin), (int16_t)(rin * 2), tw,
                              MUI_ALIGN_CENTER);
        ty = mui_text_align_y((int16_t)(g->cy - rin), (int16_t)(rin * 2), th);

        gauge_erase_text(g, s->bg);

        mui_text_draw(tx, ty, buf, g->font, s->text, s->bg, 1);

        g->text_rect.x  = tx;
        g->text_rect.y  = ty;
        g->text_rect.x2 = (int16_t)(tx + tw);
        g->text_rect.y2 = (int16_t)(ty + th);
        g->text_drawn = 1;
    } else if (g->text_drawn) {
        gauge_erase_text(g, s->bg);      /* 关掉数值显示：把旧数字擦掉 */
        g->text_drawn = 0;
    }

    g->drawn = 1;
    g->last_value = g->value;
}

void mui_gauge_invalidate(mui_gauge_t *g)
{
    if (g != NULL) {
        g->drawn = 0;
    }
}
