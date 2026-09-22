/**
 * @file mui_button.c
 * @brief MUI 按钮控件实现（保留模式对象）
 */

#include "mui_button.h"
#include "mui_font.h"

/* -------- 默认配色 -------- */

const mui_button_style_t mui_button_style_default = {
    MUI_RGB565(0x3F, 0x77, 0xB8),   /* bg：参数框蓝 */
    MUI_RGB565(0x2A, 0x52, 0x80),   /* bg_press：深蓝 */
    MUI_WHITE,                      /* fg：白字 */
    MUI_BLACK,                      /* border：黑边 */
    MUI_BUTTON_SHAPE_ROUND,            /* shape：圆角 */
    0                               /* radius：自动（h/4 限 8） */
};

/**
 * @brief 按 style 的形状配置计算底板圆角半径
 */
static int16_t button_radius(int16_t h, const mui_button_style_t *s)
{
    int16_t r;

    switch (s->shape) {
    case MUI_BUTTON_SHAPE_RECT:
        return 0;
    case MUI_BUTTON_SHAPE_PILL:
        return (int16_t)(h / 2);
    default:    /* MUI_BUTTON_SHAPE_ROUND */
        r = s->radius > 0 ? s->radius : (int16_t)(h / 4);
        if (s->radius <= 0 && r > 8) {
            r = 8;
        }
        return r;
    }
}

/**
 * @brief 绘制按钮底板（按 style 形状）
 */
static void button_base(int16_t x, int16_t y, int16_t w, int16_t h,
                        const mui_button_style_t *s, uint16_t bg)
{
    int16_t r = button_radius(h, s);

    mui_round_rect_fill(x, y, w, h, r, bg);
    mui_round_rect_draw(x, y, w, h, r, s->border);
}

/* -------- OO 按钮：对象接口 -------- */

void mui_button_init(mui_button_t *btn, int16_t x, int16_t y, int16_t w, int16_t h,
                     const mui_button_style_t *style, const char *text,
                     const mui_image_mask_t *icon)
{
    if (btn == NULL) {
        return;
    }
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;
    btn->style = style;       /* NULL 在 draw 时 fallback 默认 */
    btn->text = text;
    btn->icon = icon;
    btn->font = NULL;         /* 默认不画文字 */
    btn->pressed = 0;
    btn->enabled = 1;
    btn->visible = 1;
    btn->latch = 0;   /* 默认瞬态 */
}

void mui_button_set_pressed(mui_button_t *btn, uint8_t pressed)
{
    if (btn) { btn->pressed = pressed ? 1 : 0; }
}

void mui_button_set_enabled(mui_button_t *btn, uint8_t enabled)
{
    if (btn) { btn->enabled = enabled ? 1 : 0; }
}

void mui_button_set_visible(mui_button_t *btn, uint8_t visible)
{
    if (btn) { btn->visible = visible ? 1 : 0; }
}

void mui_button_set_text(mui_button_t *btn, const char *text)
{
    if (btn) { btn->text = text; }
}

void mui_button_set_icon(mui_button_t *btn, const mui_image_mask_t *icon)
{
    if (btn) { btn->icon = icon; }
}

void mui_button_set_font(mui_button_t *btn, const mui_font_t *font)
{
    if (btn) { btn->font = font; }
}

void mui_button_set_pos(mui_button_t *btn, int16_t x, int16_t y)
{
    if (btn) { btn->x = x; btn->y = y; }
}

void mui_button_set_latch(mui_button_t *btn, uint8_t latch)
{
    if (btn) { btn->latch = latch ? 1 : 0; }
}

/**
 * @brief 绘制禁用遮罩（覆盖底板与内容，视觉为半透明灰色）
 */
static void button_draw_disabled_overlay(const mui_button_t *btn,
                                         const mui_button_style_t *s)
{
    int16_t r = button_radius(btn->h, s);
    uint16_t gray = MUI_RGB565(0xBB, 0xBB, 0xBB);   /* 中灰 */
    uint16_t dim_fg = MUI_RGB565(0x77, 0x77, 0x77); /* 前景变深灰 */

    /* 灰色底板直接覆盖（简化：禁用态不叠底，直接画灰底+深灰边） */
    mui_round_rect_fill(btn->x, btn->y, btn->w, btn->h, r, gray);
    mui_round_rect_draw(btn->x, btn->y, btn->w, btn->h, r, dim_fg);
}

void mui_button_draw(const mui_button_t *btn)
{
    const mui_button_style_t *s;
    uint16_t bg;
    int16_t iw;
    int16_t tw;
    int16_t gap;
    int16_t total;
    int16_t sx;
    int16_t cy;

    if (btn == NULL || !btn->visible) {
        return;
    }

    s = btn->style ? btn->style : &mui_button_style_default;
    bg = btn->pressed ? s->bg_press : s->bg;

    /* 底板 */
    button_base(btn->x, btn->y, btn->w, btn->h, s, bg);

    /* 禁用遮罩（覆盖底板，不再画内容） */
    if (!btn->enabled) {
        button_draw_disabled_overlay(btn, s);
        return;
    }

    /* 文字像素宽（无字体则不画文字） */
    tw = (btn->text != NULL && btn->text[0] != '\0' && btn->font != NULL)
         ? mui_text_width(btn->text, btn->font, 1) : 0;

    /* 计算图标 + 文字组合尺寸（内容始终居中，按下仅变底色） */
    iw = btn->icon ? btn->icon->w : 0;
    gap = (iw > 0 && tw > 0) ? 4 : 0;
    total = (int16_t)(iw + gap + tw);
    sx = (int16_t)(btn->x + (btn->w - total) / 2);
    cy = (int16_t)(btn->y + btn->h / 2);

    /* 图标 */
    if (btn->icon != NULL) {
        mui_image_draw_mask(sx, (int16_t)(cy - btn->icon->h / 2),
                              btn->icon->w, btn->icon->h,
                              btn->icon->data, s->fg, bg);
    }

    /* 文字：行框垂直居中（要求字体 line_height ≤ 按钮高） */
    if (tw > 0) {
        int16_t tx = (int16_t)(sx + iw + gap);
        int16_t ty = (int16_t)(btn->y + (btn->h - btn->font->line_height) / 2);

        mui_text_draw(tx, ty, btn->text, btn->font, s->fg, bg, 1);
    }
}

uint8_t mui_button_touch(mui_button_t *btn, mui_touch_event_t ev)
{
    int16_t x, y;
    uint8_t hit;

    if (btn == NULL || ev == MUI_TOUCH_NONE || !btn->enabled) {
        return 0;
    }

    mui_touch_get_xy(&x, &y);
    hit = mui_rect_contains(btn->x, btn->y, btn->w, btn->h, x, y);

    switch (ev) {
    case MUI_TOUCH_DOWN:
        /* 瞬态：命中即高亮反馈；锁存不在此改状态（切换只在 CLICK 完成） */
        if (hit && !btn->latch) {
            mui_button_set_pressed(btn, 1);
        }
        break;

    case MUI_TOUCH_CLICK:
        if (hit) {
            if (btn->latch) {
                /* 锁存（开关）：点击切换最终状态并保持 */
                mui_button_set_pressed(btn, btn->pressed ? 0 : 1);
            } else {
                /* 瞬态：点击后弹起 */
                mui_button_set_pressed(btn, 0);
            }
            return 1;
        }
        break;

    case MUI_TOUCH_UP:
        /* 瞬态撤销 DOWN 的临时高亮；锁存状态由 CLICK 持久管理，UP 不动它 */
        if (!btn->latch && btn->pressed) {
            mui_button_set_pressed(btn, 0);
        }
        break;

    default:
        break;
    }
    return 0;
}
