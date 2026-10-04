/**
 * @file mui_popup.c
 * @brief MUI 模态弹窗实现
 */

#include "mui_popup.h"
#include "mui_layout.h"   /* mui_layout_cols / mui_rect_hit / mui_rect_pad */

#define POPUP_BTN_GAP   8

/** @brief 内部：面板矩形 */
static mui_rect_t popup_panel(const mui_popup_t *p)
{
    mui_rect_t r;

    r.x = p->x;
    r.y = p->y;
    r.x2 = (int16_t)(p->x + p->w);
    r.y2 = (int16_t)(p->y + p->h);
    return r;
}

/** @brief 内部：按当前按钮数重排底部按钮（等分一行） */
static void popup_layout_buttons(mui_popup_t *p)
{
    uint8_t i;

    if (p->nbtn == 0) {
        return;
    }
    for (i = 0; i < p->nbtn; i++) {
        mui_rect_t r = mui_layout_cols((int16_t)(p->x + p->pad),
                                       (int16_t)(p->y + p->h - p->pad - p->btn_h),
                                       (int16_t)(p->w - 2 * p->pad), p->btn_h,
                                       p->nbtn, i, POPUP_BTN_GAP);
        mui_button_init(&p->btn[i], r.x, r.y, (int16_t)(r.x2 - r.x), p->btn_h,
                        p->btn_style[i], p->btn_text[i], NULL);
        mui_button_set_latch(&p->btn[i], 0);
    }
}

void mui_popup_init(mui_popup_t *p, int16_t x, int16_t y, int16_t w, int16_t h,
                    const char *title, const char *msg, const mui_font_t *font)
{
    uint8_t i;

    if (p == NULL) {
        return;
    }
    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;
    p->title = title;
    p->msg = msg;
    p->font = font;
    p->bg = MUI_WHITE;
    p->border = MUI_BLACK;
    p->fg = MUI_BLACK;
    p->title_fg = MUI_BLACK;
    p->pad = 10;
    p->btn_h = 24;
    p->radius = 8;
    p->nbtn = 0;
    p->open = 0;
    p->needs_panel = 1;
    for (i = 0; i < MUI_POPUP_MAX_BTN; i++) {
        p->btn_text[i] = NULL;
        p->btn_style[i] = NULL;
    }
}

void mui_popup_set_colors(mui_popup_t *p, uint16_t bg, uint16_t border,
                          uint16_t fg, uint16_t title_fg)
{
    if (p == NULL) {
        return;
    }
    p->bg = bg;
    p->border = border;
    p->fg = fg;
    p->title_fg = title_fg;
    p->needs_panel = 1;
}

void mui_popup_set_text(mui_popup_t *p, const char *title, const char *msg)
{
    if (p == NULL) {
        return;
    }
    p->title = title;
    p->msg = msg;
    p->needs_panel = 1;
}

int8_t mui_popup_add_button(mui_popup_t *p, const char *text,
                            const mui_button_style_t *style)
{
    uint8_t idx;

    if (p == NULL || p->nbtn >= MUI_POPUP_MAX_BTN) {
        return -1;
    }
    idx = p->nbtn++;
    p->btn_text[idx] = text;
    p->btn_style[idx] = style;
    popup_layout_buttons(p);          /* 数量变了：整行重排 */
    return (int8_t)idx;
}

void mui_popup_open(mui_popup_t *p)
{
    if (p == NULL) {
        return;
    }
    p->open = 1;
    p->needs_panel = 1;
}

void mui_popup_close(mui_popup_t *p)
{
    if (p != NULL) {
        p->open = 0;
    }
}

uint8_t mui_popup_is_open(const mui_popup_t *p)
{
    return (uint8_t)((p != NULL && p->open) ? 1 : 0);
}

void mui_popup_draw(mui_popup_t *p)
{
    uint8_t i;

    if (p == NULL || !p->open) {
        return;
    }

#if MUI_CFG_IS_STRIP
    /* 条带后端每帧按带重放：面板必须每带都重画（屏上没有旧面板可复用） */
    p->needs_panel = 1;
#endif

    if (p->needs_panel) {
        int16_t cy = (int16_t)(p->y + p->pad);
        int16_t inner_w = (int16_t)(p->w - 2 * p->pad);

        mui_round_rect_fill(p->x, p->y, p->w, p->h, p->radius, p->bg);
        if (p->border != p->bg) {
            mui_round_rect_draw(p->x, p->y, p->w, p->h, p->radius, p->border);
        }
        if (p->title != NULL && p->font != NULL) {
            mui_text_draw_rect((int16_t)(p->x + p->pad), cy, inner_w, 20,
                               p->title, p->font, p->title_fg, p->bg, 1,
                               MUI_ALIGN_CENTER);
            cy = (int16_t)(cy + 24);
        }
        if (p->msg != NULL && p->font != NULL) {
            int16_t msg_h = (int16_t)(p->y + p->h - p->pad - p->btn_h -
                                      (p->nbtn ? 8 : 0) - cy);
            if (msg_h > 0) {
                mui_text_draw_rect((int16_t)(p->x + p->pad), cy, inner_w, msg_h,
                                   p->msg, p->font, p->fg, p->bg, 1,
                                   MUI_ALIGN_CENTER);
            }
        }
        p->needs_panel = 0;
    }

    for (i = 0; i < p->nbtn; i++) {
        mui_button_set_font(&p->btn[i], p->font);
        mui_button_draw(&p->btn[i]);   /* 内部有状态缓存，未变不重画 */
    }
}

int8_t mui_popup_touch(mui_popup_t *p, mui_touch_event_t ev)
{
    uint8_t i;
    int16_t x, y;

    if (p == NULL || !p->open || ev == MUI_TOUCH_NONE) {
        return MUI_POPUP_NONE;
    }

    /* 按钮优先 */
    for (i = 0; i < p->nbtn; i++) {
        if (mui_button_touch(&p->btn[i], ev)) {
            mui_popup_close(p);
            return (int8_t)i;
        }
    }

    /* 点击面板外 → 取消关闭 */
    if (ev == MUI_TOUCH_CLICK) {
        mui_touch_get_xy(&x, &y);
        if (!mui_rect_hit(popup_panel(p), x, y)) {
            mui_popup_close(p);
            return MUI_POPUP_CLOSED;
        }
    }
    return MUI_POPUP_NONE;
}
