/**
 * @file mui_widget.c
 * @brief MUI 控件：按钮（立即模式，无内部状态）
 */

#include "mui.h"
#include "mui_font.h"

/* -------- 默认配色 -------- */

const mui_button_style_t mui_button_style_default = {
    MUI_RGB565(0x3F, 0x77, 0xB8),   /* bg：参数框蓝 */
    MUI_RGB565(0x2A, 0x52, 0x80),   /* bg_press：深蓝 */
    MUI_WHITE,                      /* fg：白字 */
    MUI_BLACK,                      /* border：黑边 */
    MUI_BTN_SHAPE_ROUND,            /* shape：圆角 */
    0                               /* radius：自动（h/4 限 8） */
};

/**
 * @brief 按 style 的形状配置计算底板圆角半径
 */
static int16_t button_radius(int16_t h, const mui_button_style_t *s)
{
    int16_t r;

    switch (s->shape) {
    case MUI_BTN_SHAPE_RECT:
        return 0;
    case MUI_BTN_SHAPE_PILL:
        return (int16_t)(h / 2);
    default:    /* MUI_BTN_SHAPE_ROUND */
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

    mui_fill_round_rect(x, y, w, h, r, bg);
    mui_draw_round_rect(x, y, w, h, r, s->border);
}

/**
 * @brief 绘制按钮（圆角底板 + 居中点阵文字 + 按下视觉反馈）
 */
void mui_button(int16_t x, int16_t y, int16_t w, int16_t h,
                const char *text, uint8_t pressed,
                const mui_button_style_t *style)
{
    const mui_button_style_t *s = style ? style : &mui_button_style_default;
    uint16_t bg = pressed ? s->bg_press : s->bg;

    button_base(x, y, w, h, s, bg);

    /* 居中文字（按下时下移 1 像素制造按压感） */
    if (text != NULL && text[0] != '\0') {
        int16_t tw = mui_font_text_width(text, 1, 1);
        int16_t tx = (int16_t)(x + (w - tw) / 2);
        int16_t ty = (int16_t)(y + (h - 7) / 2 + (pressed ? 1 : 0));

        if (tx < x + 2) {
            tx = (int16_t)(x + 2);
        }
        mui_font_draw_text(tx, ty, text, s->fg, 1, 1);
    }
}

/**
 * @brief 绘制图标按钮（8bpp alpha 蒙版图标[+文字]自动居中 + 按下视觉反馈）
 *
 * 图标前景色取 style->fg，与按钮底色逐像素混合，边缘平滑无锯齿。
 */
void mui_button_icon(int16_t x, int16_t y, int16_t w, int16_t h,
                     const mui_image_alpha_t *icon, const char *text,
                     uint8_t pressed, const mui_button_style_t *style)
{
    const mui_button_style_t *s = style ? style : &mui_button_style_default;
    uint16_t bg = pressed ? s->bg_press : s->bg;
    int16_t iw = icon ? icon->w : 0;
    int16_t tw = (text != NULL && text[0] != '\0') ? mui_font_text_width(text, 1, 1) : 0;
    int16_t gap = (iw > 0 && tw > 0) ? 4 : 0;
    int16_t total = (int16_t)(iw + gap + tw);
    int16_t sx = (int16_t)(x + (w - total) / 2);
    int16_t cy = (int16_t)(y + h / 2 + (pressed ? 1 : 0));

    button_base(x, y, w, h, s, bg);

    /* 图标（与按钮底色逐像素混合，边缘平滑） */
    if (icon != NULL) {
        mui_draw_bitmap_alpha(sx, (int16_t)(cy - icon->h / 2),
                              icon->w, icon->h, icon->data, s->fg, bg);
    }

    /* 文字（跟随图标右侧） */
    if (tw > 0) {
        mui_font_draw_text((int16_t)(sx + iw + gap), (int16_t)(cy - 3),
                           text, s->fg, 1, 1);
    }
}
