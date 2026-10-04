/**
 * @file mui_toggle.c
 * @brief 开关 / 复选 / 单选 控件实现（保留模式 · 触摸切换 · 可选抗锯齿）
 */

#include "mui_toggle.h"

/* -------- 默认样式 -------- */

const mui_switch_style_t mui_switch_style_default = {
    .off = MUI_RGB565(0x66, 0x6E, 0x7A), .on = MUI_RGB565(0x04, 0xAA, 0xF4),
    .knob = MUI_WHITE, .knob_border = MUI_WHITE,
    .text = MUI_BLACK, .screen_bg = MUI_WHITE, .aa = 1, .gap = 6,
};

const mui_checkbox_style_t mui_checkbox_style_default = {
    .bg = MUI_WHITE, .on = MUI_RGB565(0x04, 0xAA, 0xF4),
    .border = MUI_RGB565(0x8A, 0x93, 0xA6), .mark = MUI_WHITE,
    .text = MUI_BLACK, .screen_bg = MUI_WHITE, .size = 0, .gap = 6, .aa = 1,
};

const mui_radio_style_t mui_radio_style_default = {
    .ring = MUI_RGB565(0x8A, 0x93, 0xA6), .on = MUI_RGB565(0x04, 0xAA, 0xF4),
    .inner = MUI_WHITE, .text = MUI_BLACK, .screen_bg = MUI_WHITE,
    .size = 0, .gap = 6, .aa = 1,
};

/* -------- 内部工具 -------- */

/** @brief 画一条"粗线"（按行偏移叠加，避免为小勾新增图元） */
static void tog_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     int16_t th, uint16_t c)
{
    int16_t k;
    for (k = 0; k < th; k++) {
        mui_line_draw(x0, (int16_t)(y0 + k), x1, (int16_t)(y1 + k), c);
    }
}

/** @brief 描边圆角矩形：AA 时用"外壳色铺满 + 内缩 1px 填底色"两次填充（避免暗环） */
static void tog_panel(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                      uint16_t bg, uint16_t border, uint16_t screen_bg, uint8_t aa)
{
    if (!aa) {
        mui_round_rect_fill(x, y, w, h, r, bg);
        mui_round_rect_draw(x, y, w, h, r, border);
        return;
    }
    if (border == bg || w < 3 || h < 3) {
        mui_round_rect_fill_aa(x, y, w, h, r, bg, screen_bg);
        return;
    }
    mui_round_rect_fill_aa(x, y, w, h, r, border, screen_bg);
    mui_round_rect_fill_aa((int16_t)(x + 1), (int16_t)(y + 1),
                           (int16_t)(w - 2), (int16_t)(h - 2),
                           (int16_t)(r > 1 ? r - 1 : 0), bg, border);
}

/* ================= 开关 ================= */

void mui_switch_init(mui_switch_t *sw, int16_t x, int16_t y, int16_t w, int16_t h,
                     uint8_t on, const mui_switch_style_t *style)
{
    if (sw == NULL) { return; }
    sw->x = x; sw->y = y; sw->w = w; sw->h = h;
    sw->style = style;
    sw->text = NULL;
    sw->font = NULL;
    sw->on = on ? 1 : 0;
    sw->enabled = 1;
    sw->visible = 1;
}

void mui_switch_set_on(mui_switch_t *sw, uint8_t on)
{
    if (sw) { sw->on = on ? 1 : 0; }
}

uint8_t mui_switch_get_on(const mui_switch_t *sw)
{
    return sw ? sw->on : 0;
}

void mui_switch_set_text(mui_switch_t *sw, const char *text)
{
    if (sw) { sw->text = text; }
}

void mui_switch_set_font(mui_switch_t *sw, const mui_font_t *font)
{
    if (sw) { sw->font = font; }
}

void mui_switch_set_style(mui_switch_t *sw, const mui_switch_style_t *style)
{
    if (sw) { sw->style = style; }
}

void mui_switch_set_pos(mui_switch_t *sw, int16_t x, int16_t y)
{
    if (sw) { sw->x = x; sw->y = y; }
}

void mui_switch_set_visible(mui_switch_t *sw, uint8_t visible)
{
    if (sw) { sw->visible = visible ? 1 : 0; }
}

void mui_switch_draw(const mui_switch_t *sw)
{
    const mui_switch_style_t *st;
    int16_t bw, cx, cy, kr;
    uint16_t tc;
    uint8_t aa;

    if (sw == NULL || !sw->visible) { return; }
    st = sw->style ? sw->style : &mui_switch_style_default;

    bw = (int16_t)(sw->h * 9 / 5);
    if (bw > sw->w) { bw = sw->w; }
    cy = (int16_t)(sw->y + sw->h / 2);
    kr = (int16_t)(sw->h / 2 - 3);
    if (kr < 2) { kr = 2; }
    tc = sw->on ? st->on : st->off;
    cx = sw->on ? (int16_t)(sw->x + bw - sw->h / 2) : (int16_t)(sw->x + sw->h / 2);
    aa = (uint8_t)(st->aa && MUI_CFG_AA);

    /* 轨道 */
    if (aa) {
        mui_round_rect_fill_aa(sw->x, sw->y, bw, sw->h, (int16_t)(sw->h / 2),
                               tc, st->screen_bg);
    } else {
        mui_round_rect_fill(sw->x, sw->y, bw, sw->h, (int16_t)(sw->h / 2), tc);
    }

    /* 圆钮：外圈边色圆盘 + 内芯填充圆盘（AA 底色取轨道色） */
    if (aa) {
        if (st->knob_border != st->knob) {
            mui_circle_fill_aa(cx, cy, kr, st->knob_border, tc);
            mui_circle_fill_aa(cx, cy, (int16_t)(kr - 1), st->knob,
                               st->knob_border);
        } else {
            mui_circle_fill_aa(cx, cy, kr, st->knob, tc);
        }
    } else {
        if (st->knob_border != st->knob) {
            mui_circle_fill(cx, cy, kr, st->knob_border);
            if (kr > 1) { mui_circle_fill(cx, cy, (int16_t)(kr - 1), st->knob); }
        } else {
            mui_circle_fill(cx, cy, kr, st->knob);
        }
    }

    /* 文字 */
    if (sw->text != NULL && sw->font != NULL) {
        mui_text_draw((int16_t)(sw->x + bw + st->gap),
                      (int16_t)(sw->y + (sw->h - sw->font->line_height) / 2),
                      sw->text, sw->font, st->text, st->screen_bg, 1);
    }
}

uint8_t mui_switch_touch(mui_switch_t *sw, mui_touch_event_t ev)
{
    int16_t x, y;

    if (sw == NULL || !sw->enabled || ev != MUI_TOUCH_CLICK) {
        return 0;
    }
    mui_touch_get_xy(&x, &y);
    if (!mui_rect_contains(sw->x, sw->y, sw->w, sw->h, x, y)) {
        return 0;
    }
    sw->on = sw->on ? 0 : 1;
    return 1;
}

/* ================= 复选 ================= */

void mui_checkbox_init(mui_checkbox_t *cb, int16_t x, int16_t y, int16_t w, int16_t h,
                       uint8_t checked, const mui_checkbox_style_t *style)
{
    if (cb == NULL) { return; }
    cb->x = x; cb->y = y; cb->w = w; cb->h = h;
    cb->style = style;
    cb->text = NULL;
    cb->font = NULL;
    cb->checked = checked ? 1 : 0;
    cb->enabled = 1;
    cb->visible = 1;
}

void mui_checkbox_set_checked(mui_checkbox_t *cb, uint8_t checked)
{
    if (cb) { cb->checked = checked ? 1 : 0; }
}

uint8_t mui_checkbox_get_checked(const mui_checkbox_t *cb)
{
    return cb ? cb->checked : 0;
}

void mui_checkbox_set_text(mui_checkbox_t *cb, const char *text)
{
    if (cb) { cb->text = text; }
}

void mui_checkbox_set_font(mui_checkbox_t *cb, const mui_font_t *font)
{
    if (cb) { cb->font = font; }
}

void mui_checkbox_set_style(mui_checkbox_t *cb, const mui_checkbox_style_t *style)
{
    if (cb) { cb->style = style; }
}

void mui_checkbox_set_pos(mui_checkbox_t *cb, int16_t x, int16_t y)
{
    if (cb) { cb->x = x; cb->y = y; }
}

void mui_checkbox_set_visible(mui_checkbox_t *cb, uint8_t visible)
{
    if (cb) { cb->visible = visible ? 1 : 0; }
}

void mui_checkbox_draw(const mui_checkbox_t *cb)
{
    const mui_checkbox_style_t *st;
    int16_t s, bx, by, r, th;
    uint8_t aa;

    if (cb == NULL || !cb->visible) { return; }
    st = cb->style ? cb->style : &mui_checkbox_style_default;

    s = st->size > 0 ? st->size : cb->h;
    if (s > cb->h) { s = cb->h; }
    if (s > cb->w) { s = cb->w; }
    if (s < 4) { s = 4; }
    bx = cb->x;
    by = (int16_t)(cb->y + (cb->h - s) / 2);
    r = (int16_t)(s / 6);
    if (r < 1) { r = 1; }
    aa = (uint8_t)(st->aa && MUI_CFG_AA);

    tog_panel(bx, by, s, s, r, cb->checked ? st->on : st->bg, st->border,
              st->screen_bg, aa);

    if (cb->checked) {
        int16_t x0 = (int16_t)(bx + s / 4);
        int16_t y0 = (int16_t)(by + s / 2);
        int16_t x1 = (int16_t)(bx + s * 2 / 5);
        int16_t y1 = (int16_t)(by + s * 3 / 4);
        int16_t x2 = (int16_t)(bx + s * 4 / 5);
        int16_t y2 = (int16_t)(by + s / 4);

        th = (int16_t)(s / 8);
        if (th < 1) { th = 1; }
        tog_line(x0, y0, x1, y1, th, st->mark);
        tog_line(x1, y1, x2, y2, th, st->mark);
    }

    if (cb->text != NULL && cb->font != NULL) {
        mui_text_draw((int16_t)(bx + s + st->gap),
                      (int16_t)(cb->y + (cb->h - cb->font->line_height) / 2),
                      cb->text, cb->font, st->text, st->screen_bg, 1);
    }
}

uint8_t mui_checkbox_touch(mui_checkbox_t *cb, mui_touch_event_t ev)
{
    int16_t x, y;

    if (cb == NULL || !cb->enabled || ev != MUI_TOUCH_CLICK) {
        return 0;
    }
    mui_touch_get_xy(&x, &y);
    if (!mui_rect_contains(cb->x, cb->y, cb->w, cb->h, x, y)) {
        return 0;
    }
    cb->checked = cb->checked ? 0 : 1;
    return 1;
}

/* ================= 单选 ================= */

void mui_radio_init(mui_radio_t *rd, int16_t x, int16_t y, int16_t w, int16_t h,
                    uint8_t selected, const mui_radio_style_t *style)
{
    if (rd == NULL) { return; }
    rd->x = x; rd->y = y; rd->w = w; rd->h = h;
    rd->style = style;
    rd->text = NULL;
    rd->font = NULL;
    rd->selected = selected ? 1 : 0;
    rd->enabled = 1;
    rd->visible = 1;
    rd->group = NULL;
}

void mui_radio_set_selected(mui_radio_t *rd, uint8_t selected)
{
    if (rd) { rd->selected = selected ? 1 : 0; }
}

uint8_t mui_radio_get_selected(const mui_radio_t *rd)
{
    return rd ? rd->selected : 0;
}

void mui_radio_set_text(mui_radio_t *rd, const char *text)
{
    if (rd) { rd->text = text; }
}

void mui_radio_set_font(mui_radio_t *rd, const mui_font_t *font)
{
    if (rd) { rd->font = font; }
}

void mui_radio_set_style(mui_radio_t *rd, const mui_radio_style_t *style)
{
    if (rd) { rd->style = style; }
}

void mui_radio_set_pos(mui_radio_t *rd, int16_t x, int16_t y)
{
    if (rd) { rd->x = x; rd->y = y; }
}

void mui_radio_set_visible(mui_radio_t *rd, uint8_t visible)
{
    if (rd) { rd->visible = visible ? 1 : 0; }
}

void mui_radio_set_group(mui_radio_t *rd, mui_radio_t **group)
{
    if (rd) { rd->group = group; }
}

void mui_radio_draw(const mui_radio_t *rd)
{
    const mui_radio_style_t *st;
    int16_t s, R, cx, cy;
    uint8_t aa;

    if (rd == NULL || !rd->visible) { return; }
    st = rd->style ? rd->style : &mui_radio_style_default;

    s = st->size > 0 ? st->size : rd->h;
    if (s > rd->h) { s = rd->h; }
    if (s > rd->w) { s = rd->w; }
    if (s < 4) { s = 4; }
    R = (int16_t)(s / 2 - 1);
    if (R < 2) { R = 2; }
    cx = (int16_t)(rd->x + s / 2);
    cy = (int16_t)(rd->y + (rd->h - s) / 2 + s / 2);
    aa = (uint8_t)(st->aa && MUI_CFG_AA);

    if (aa) {
        mui_circle_fill_aa(cx, cy, R, st->ring, st->screen_bg);
        mui_circle_fill_aa(cx, cy, (int16_t)(R - 2), st->inner, st->ring);
        if (rd->selected && R - 4 > 0) {
            mui_circle_fill_aa(cx, cy, (int16_t)(R - 4), st->on, st->inner);
        }
    } else {
        mui_circle_fill(cx, cy, R, st->ring);
        mui_circle_fill(cx, cy, (int16_t)(R - 2), st->inner);
        if (rd->selected && R - 4 > 0) {
            mui_circle_fill(cx, cy, (int16_t)(R - 4), st->on);
        }
    }

    if (rd->text != NULL && rd->font != NULL) {
        mui_text_draw((int16_t)(rd->x + s + st->gap),
                      (int16_t)(rd->y + (rd->h - rd->font->line_height) / 2),
                      rd->text, rd->font, st->text, st->screen_bg, 1);
    }
}

uint8_t mui_radio_touch(mui_radio_t *rd, mui_touch_event_t ev)
{
    int16_t x, y;
    int i;

    if (rd == NULL || !rd->enabled || ev != MUI_TOUCH_CLICK) {
        return 0;
    }
    mui_touch_get_xy(&x, &y);
    if (!mui_rect_contains(rd->x, rd->y, rd->w, rd->h, x, y)) {
        return 0;
    }
    if (rd->group != NULL) {
        for (i = 0; rd->group[i] != NULL; i++) {
            rd->group[i]->selected = 0;
        }
    }
    rd->selected = 1;
    return 1;
}
