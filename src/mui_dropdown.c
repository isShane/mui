/**
 * @file mui_dropdown.c
 * @brief MUI 下拉框实现（按钮 + 覆盖式列表）
 */

#include "mui_dropdown.h"
#include "mui_layout.h"   /* mui_rect_hit */

/** @brief 内部：按钮矩形 */
static mui_rect_t drop_btn_rect(const mui_dropdown_t *d)
{
    mui_rect_t r;

    r.x = d->btn.x;
    r.y = d->btn.y;
    r.x2 = (int16_t)(d->btn.x + d->btn.w);
    r.y2 = (int16_t)(d->btn.y + d->btn.h);
    return r;
}

/** @brief 内部：展开列表显示的行数（受 max_visible 限制） */
static int16_t drop_vis_rows(const mui_dropdown_t *d)
{
    int16_t v = (int16_t)d->n;

    if (v > (int16_t)d->max_visible) {
        v = (int16_t)d->max_visible;
    }
    if (v < 1) {
        v = 1;
    }
    return v;
}

/** @brief 内部：按当前行数/行高等重排展开列表 */
static void drop_layout(mui_dropdown_t *d)
{
    int16_t vis = drop_vis_rows(d);
    int16_t lh = (int16_t)((int32_t)vis * (d->row_h + d->gap) - d->gap);

    if (lh < d->row_h) {
        lh = d->row_h;
    }
    mui_list_init(&d->list, d->btn.x, (int16_t)(d->btn.y + d->btn.h),
                  d->btn.w, lh, d->row_h, d->gap);
    mui_list_set_rows(&d->list, d->n);
    mui_list_set_sel(&d->list, d->sel);
}

void mui_dropdown_init(mui_dropdown_t *d, int16_t x, int16_t y, int16_t w, int16_t h,
                       const char *const *items, uint8_t n, int16_t row_h,
                       const mui_font_t *font, const mui_button_style_t *btn_style)
{
    if (d == NULL) {
        return;
    }
    d->items = items;
    d->n = (uint8_t)((n > 0) ? n : 1);
    d->sel = 0;
    d->open = 0;
    d->row_h = (row_h > 0) ? row_h : 16;
    d->gap = 0;
    d->max_visible = 4;
    d->font = font;
    d->btn_style = btn_style;
    d->bg = MUI_WHITE;
    d->bg_sel = MUI_RGB565(0x04, 0xAA, 0xF4);
    d->fg = MUI_BLACK;
    d->fg_sel = MUI_WHITE;
    d->border = MUI_BLACK;

    mui_button_init(&d->btn, x, y, w, h, btn_style,
                    (items != NULL && items[0] != NULL) ? items[0] : "", NULL);
    mui_button_set_font(&d->btn, font);
    drop_layout(d);
}

void mui_dropdown_set_colors(mui_dropdown_t *d, uint16_t bg, uint16_t bg_sel,
                             uint16_t fg, uint16_t fg_sel, uint16_t border)
{
    if (d == NULL) {
        return;
    }
    d->bg = bg;
    d->bg_sel = bg_sel;
    d->fg = fg;
    d->fg_sel = fg_sel;
    d->border = border;
}

void mui_dropdown_set_max_visible(mui_dropdown_t *d, uint8_t rows)
{
    if (d == NULL || rows == 0) {
        return;
    }
    d->max_visible = rows;
    drop_layout(d);
}

uint8_t mui_dropdown_selected(const mui_dropdown_t *d)
{
    return (d != NULL) ? d->sel : 0;
}

void mui_dropdown_set_selected(mui_dropdown_t *d, uint8_t sel)
{
    if (d == NULL) {
        return;
    }
    if (sel >= d->n) {
        sel = (uint8_t)(d->n - 1);
    }
    d->sel = sel;
    if (d->items != NULL) {
        mui_button_set_text(&d->btn, d->items[sel]);
    }
    mui_list_set_sel(&d->list, sel);
}

uint8_t mui_dropdown_is_open(const mui_dropdown_t *d)
{
    return (uint8_t)((d != NULL && d->open) ? 1 : 0);
}

void mui_dropdown_open(mui_dropdown_t *d)
{
    int16_t y;

    if (d == NULL) {
        return;
    }
    d->open = 1;
    mui_list_set_sel(&d->list, d->sel);

    /* 把选中项滚入可见区 */
    y = (int16_t)((int32_t)d->sel * (d->row_h + d->gap));
    if (y < mui_list_get_scroll(&d->list)) {
        mui_list_set_scroll(&d->list, y);
    } else if (y + d->row_h > mui_list_get_scroll(&d->list) + d->list.box.h) {
        mui_list_set_scroll(&d->list,
                            (int16_t)(y + d->row_h - d->list.box.h));
    }
}

void mui_dropdown_close(mui_dropdown_t *d)
{
    if (d != NULL) {
        d->open = 0;
    }
}

uint8_t mui_dropdown_popup_rect(const mui_dropdown_t *d, mui_rect_t *r)
{
    if (d == NULL || !d->open || r == NULL) {
        return 0;
    }
    r->x = d->list.box.x;
    r->y = d->list.box.y;
    r->x2 = (int16_t)(d->list.box.x + d->list.box.w);
    r->y2 = (int16_t)(d->list.box.y + d->list.box.h);
    return 1;
}

void mui_dropdown_draw(mui_dropdown_t *d)
{
    mui_rect_t frame;
    uint8_t first = 0, count = 0, i;
    int16_t fw, fh;

    if (d == NULL) {
        return;
    }
    mui_button_draw(&d->btn);
    if (!d->open) {
        return;
    }
    if (!mui_dropdown_popup_rect(d, &frame)) {
        return;
    }

    fw = (int16_t)(frame.x2 - frame.x);
    fh = (int16_t)(frame.y2 - frame.y);

    /* 展开列表底板 + 边框（不透明，覆盖其下内容） */
    mui_rect_fill(frame.x, frame.y, fw, fh, d->bg);
    if (d->border != d->bg) {
        mui_rect_draw(frame.x, frame.y, fw, fh, d->border);
    }

    /* 可见行：逐行铺底 + 文字（带裁剪，超出视口自动裁掉） */
    mui_list_begin(&d->list);
    mui_list_visible(&d->list, &first, &count);
    for (i = 0; i < count; i++) {
        uint8_t idx = (uint8_t)(first + i);
        mui_rect_t r = mui_list_row_rect(&d->list, idx);
        uint16_t b = (idx == d->sel) ? d->bg_sel : d->bg;
        uint16_t f = (idx == d->sel) ? d->fg_sel : d->fg;

        mui_rect_fill(r.x, r.y, (int16_t)(r.x2 - r.x), d->row_h, b);
        mui_text_draw_rect((int16_t)(r.x + 6), r.y, (int16_t)(r.x2 - r.x - 12),
                           d->row_h, d->items[idx], d->font, f, b, 1,
                           MUI_ALIGN_LEFT);
    }
    mui_list_end(&d->list);
}

uint8_t mui_dropdown_touch(mui_dropdown_t *d, mui_touch_event_t ev)
{
    int16_t x, y;

    if (d == NULL || ev == MUI_TOUCH_NONE) {
        return MUI_DROPDOWN_NONE;
    }

    /* 收起态：只喂按钮 */
    if (!d->open) {
        if (mui_button_touch(&d->btn, ev)) {
            mui_dropdown_open(d);
            return MUI_DROPDOWN_CHANGED;
        }
        return MUI_DROPDOWN_NONE;
    }

    /* 展开态：先喂列表（点选/滚动） */
    {
        uint8_t r = mui_list_touch(&d->list, ev);

        if (r & MUI_LIST_CHANGED_SEL) {
            mui_dropdown_set_selected(d, mui_list_get_sel(&d->list));
            mui_dropdown_close(d);
            return MUI_DROPDOWN_CLOSED;
        }
        if (r & MUI_LIST_CHANGED_SCROLL) {
            return MUI_DROPDOWN_CHANGED;
        }
    }

    /* 点空白 / 点按钮 → 收起 */
    if (ev == MUI_TOUCH_CLICK) {
        mui_rect_t r;

        mui_touch_get_xy(&x, &y);
        if (mui_rect_hit(drop_btn_rect(d), x, y)) {
            mui_dropdown_close(d);                 /* 再点按钮 = 收起 */
            return MUI_DROPDOWN_CLOSED;
        }
        if (mui_dropdown_popup_rect(d, &r) && mui_rect_hit(r, x, y)) {
            return MUI_DROPDOWN_NONE;              /* 落在行间距上 */
        }
        mui_dropdown_close(d);                     /* 点别处 = 收起 */
        return MUI_DROPDOWN_CLOSED;
    }
    return MUI_DROPDOWN_NONE;
}
