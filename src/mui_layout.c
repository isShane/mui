/**
 * @file mui_layout.c
 * @brief MUI 容器与布局实现（纯整数、零动态内存）
 */

#include "mui_layout.h"

/* ======================= 裁剪容器 ======================= */

void mui_container_init(mui_container_t *c, int16_t x, int16_t y,
                        int16_t w, int16_t h)
{
    if (c == NULL) {
        return;
    }
    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    c->sx = 0;
    c->sy = 0;
    c->cw = 0;
    c->ch = 0;
    c->open = 0;
    c->clip.x = 0;
    c->clip.y = 0;
    c->clip.x2 = 0;
    c->clip.y2 = 0;
}

void mui_container_begin(mui_container_t *c)
{
    mui_rect_t p;
    int32_t x, y, x2, y2;

    if (c == NULL) {
        return;
    }
    p = mui_clip_save();
    c->clip = p;
    c->open = 1;

    /* 容器矩形 ∩ 父裁剪区；因父裁剪区必在屏幕内，结果也必在屏幕内 */
    x = c->x;
    y = c->y;
    x2 = (int32_t)c->x + c->w;
    y2 = (int32_t)c->y + c->h;
    if (x < p.x)   { x = p.x; }
    if (y < p.y)   { y = p.y; }
    if (x2 > p.x2) { x2 = p.x2; }
    if (y2 > p.y2) { y2 = p.y2; }
    if (x2 <= x || y2 <= y) {
        mui_set_clip(0, 0, 0, 0);              /* 无交集 → 空裁剪区 */
        return;
    }
    mui_set_clip((int16_t)x, (int16_t)y, (int16_t)(x2 - x), (int16_t)(y2 - y));
}

void mui_container_end(mui_container_t *c)
{
    if (c == NULL || !c->open) {
        return;
    }
    mui_clip_restore(c->clip);
    c->open = 0;
}

int16_t mui_container_ox(const mui_container_t *c)
{
    return (int16_t)(c->x - c->sx);
}

int16_t mui_container_oy(const mui_container_t *c)
{
    return (int16_t)(c->y - c->sy);
}

int16_t mui_container_scroll_max_x(const mui_container_t *c)
{
    int16_t m;

    if (c == NULL) {
        return 0;
    }
    m = (int16_t)(c->cw - c->w);
    return (m > 0) ? m : 0;
}

int16_t mui_container_scroll_max_y(const mui_container_t *c)
{
    int16_t m;

    if (c == NULL) {
        return 0;
    }
    m = (int16_t)(c->ch - c->h);
    return (m > 0) ? m : 0;
}

void mui_container_set_scroll(mui_container_t *c, int16_t sx, int16_t sy)
{
    int16_t mx, my;

    if (c == NULL) {
        return;
    }
    mx = mui_container_scroll_max_x(c);
    my = mui_container_scroll_max_y(c);
    if (sx < 0) { sx = 0; } else if (sx > mx) { sx = mx; }
    if (sy < 0) { sy = 0; } else if (sy > my) { sy = my; }
    c->sx = sx;
    c->sy = sy;
}

void mui_container_scroll_by(mui_container_t *c, int16_t dx, int16_t dy)
{
    if (c == NULL) {
        return;
    }
    mui_container_set_scroll(c, (int16_t)(c->sx + dx), (int16_t)(c->sy + dy));
}

void mui_container_set_content(mui_container_t *c, int16_t cw, int16_t ch)
{
    if (c == NULL) {
        return;
    }
    c->cw = cw;
    c->ch = ch;
    mui_container_set_scroll(c, c->sx, c->sy);   /* 按新内容尺寸重新钳制 */
}

/* ======================= 布局游标 ======================= */

void mui_layout_init(mui_layout_t *l, int16_t x, int16_t y, int16_t w, int16_t h,
                     uint8_t dir, int16_t gap)
{
    if (l == NULL) {
        return;
    }
    l->x = x;
    l->y = y;
    l->w = w;
    l->h = h;
    l->cx = x;
    l->cy = y;
    l->line_h = 0;
    l->gap = (gap < 0) ? 0 : gap;
    l->dir = (uint8_t)(dir ? MUI_LAYOUT_COL : MUI_LAYOUT_ROW);
}

/** @brief 内部：构造矩形 */
static mui_rect_t mk_rect(int32_t x, int32_t y, int32_t x2, int32_t y2)
{
    mui_rect_t r;

    if (x2 < x) { x2 = x; }
    if (y2 < y) { y2 = y; }
    r.x = (int16_t)x;
    r.y = (int16_t)y;
    r.x2 = (int16_t)x2;
    r.y2 = (int16_t)y2;
    return r;
}

mui_rect_t mui_layout_next(mui_layout_t *l, int16_t w, int16_t h)
{
    if (l == NULL) {
        return mk_rect(0, 0, 0, 0);
    }

    if (l->dir == MUI_LAYOUT_ROW) {
        if (h <= 0) {
            h = l->h;                         /* 默认占满整行高 */
        }
        if (w <= 0) {
            w = (int16_t)(l->x + l->w - l->cx);   /* 占满本行剩余宽度 */
        }
        if (w <= 0) {
            return mk_rect(0, 0, 0, 0);
        }
        if (l->cx + w > l->x + l->w) {        /* 本行放不下 → 换行 */
            l->cx = l->x;
            l->cy = (int16_t)(l->cy + l->line_h + l->gap);
            l->line_h = 0;
        }
        if (l->cy + h > l->y + l->h || w > l->w) {
            return mk_rect(0, 0, 0, 0);       /* 纵向放不下 */
        }
        {
            mui_rect_t r = mk_rect(l->cx, l->cy, (int32_t)l->cx + w,
                                   (int32_t)l->cy + h);
            l->cx = (int16_t)(l->cx + w + l->gap);
            if (h > l->line_h) {
                l->line_h = h;
            }
            return r;
        }
    }

    /* MUI_LAYOUT_COL */
    if (w <= 0) {
        w = l->w;                             /* 默认占满整列宽 */
    }
    if (h <= 0) {
        h = (int16_t)(l->y + l->h - l->cy);   /* 占满本列剩余高度 */
    }
    if (h <= 0 || w > l->w || l->cy + h > l->y + l->h) {
        return mk_rect(0, 0, 0, 0);
    }
    {
        mui_rect_t r = mk_rect(l->cx, l->cy, (int32_t)l->cx + w, (int32_t)l->cy + h);
        l->cy = (int16_t)(l->cy + h + l->gap);
        if (h > l->line_h) {
            l->line_h = h;
        }
        return r;
    }
}

mui_rect_t mui_layout_rest(const mui_layout_t *l)
{
    if (l == NULL) {
        return mk_rect(0, 0, 0, 0);
    }
    if (l->dir == MUI_LAYOUT_ROW) {
        int16_t top = (int16_t)(l->cy + ((l->line_h > 0) ? (l->line_h + l->gap) : 0));
        return mk_rect(l->x, top, (int32_t)l->x + l->w, (int32_t)l->y + l->h);
    }
    return mk_rect(l->cx, l->y, (int32_t)l->x + l->w, (int32_t)l->y + l->h);
}

/** @brief 内部：把 span 按 total 等分，返回第 index 段的起止（末段吃余数） */
static void split_span(int32_t start, int32_t span, int32_t gap,
                       uint8_t total, uint8_t index, int32_t *s, int32_t *e)
{
    int32_t cell = (span - gap * (int32_t)(total - 1)) / (int32_t)total;
    int32_t cs;

    if (cell < 0) {
        cell = 0;
    }
    cs = start + (int32_t)index * (cell + gap);
    *s = cs;
    *e = (index == (uint8_t)(total - 1)) ? (start + span) : (cs + cell);
    if (*e < *s) {
        *e = *s;
    }
}

mui_rect_t mui_layout_cols(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint8_t total, uint8_t index, int16_t gap)
{
    int32_t s, e;

    if (total < 1 || index >= total || w <= 0 || h <= 0) {
        return mk_rect(0, 0, 0, 0);
    }
    split_span(x, w, (gap < 0) ? 0 : gap, total, index, &s, &e);
    return mk_rect(s, y, e, (int32_t)y + h);
}

mui_rect_t mui_layout_rows(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint8_t total, uint8_t index, int16_t gap)
{
    int32_t s, e;

    if (total < 1 || index >= total || w <= 0 || h <= 0) {
        return mk_rect(0, 0, 0, 0);
    }
    split_span(y, h, (gap < 0) ? 0 : gap, total, index, &s, &e);
    return mk_rect(x, s, (int32_t)x + w, e);
}

mui_rect_t mui_layout_grid(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint8_t rows, uint8_t cols,
                           uint8_t row, uint8_t col, int16_t gap)
{
    int32_t ys, ye, xs, xe;

    if (rows < 1 || cols < 1 || row >= rows || col >= cols || w <= 0 || h <= 0) {
        return mk_rect(0, 0, 0, 0);
    }
    split_span(y, h, (gap < 0) ? 0 : gap, rows, row, &ys, &ye);
    split_span(x, w, (gap < 0) ? 0 : gap, cols, col, &xs, &xe);
    return mk_rect(xs, ys, xe, ye);
}

/* ======================= 矩形工具 ======================= */

mui_rect_t mui_rect_pad(mui_rect_t r, int16_t pad)
{
    r.x = (int16_t)(r.x + pad);
    r.y = (int16_t)(r.y + pad);
    r.x2 = (int16_t)(r.x2 - pad);
    r.y2 = (int16_t)(r.y2 - pad);
    if (r.x2 < r.x) { r.x2 = r.x; }
    if (r.y2 < r.y) { r.y2 = r.y; }
    return r;
}

mui_rect_t mui_rect_align(mui_rect_t area, int16_t w, int16_t h, uint8_t align)
{
    int16_t aw = (int16_t)(area.x2 - area.x);
    int16_t ah = (int16_t)(area.y2 - area.y);
    int16_t ax = area.x;
    int16_t ay;

    if (w < 0) { w = 0; }
    if (h < 0) { h = 0; }
    if (w <= aw) {
        if (align == MUI_ALIGN_CENTER) {
            ax = (int16_t)(area.x + (aw - w) / 2);
        } else if (align == MUI_ALIGN_RIGHT) {
            ax = (int16_t)(area.x2 - w);
        }
    }
    ay = (h <= ah) ? (int16_t)(area.y + (ah - h) / 2) : area.y;
    return mk_rect(ax, ay, (int32_t)ax + w, (int32_t)ay + h);
}

uint8_t mui_rect_hit(mui_rect_t r, int16_t px, int16_t py)
{
    return (uint8_t)(px >= r.x && px < r.x2 && py >= r.y && py < r.y2);
}
