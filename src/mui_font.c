/**
 * @file mui_font.c
 * @brief MUI 点阵字体实现（5x7，行式字模，bit4..bit0 = 列0..列4）
 */

#include "mui.h"
#include "mui_font.h"

/* -------- LVGL 转换字体渲染（8bpp alpha 混合） -------- */

void mui_lv_font_draw_char(int16_t x, int16_t y, char c, const mui_lv_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale)
{
    const mui_glyph_dsc_t *g;
    int16_t gx, gy;
    int row, col;

    if (c < f->first_char || c > f->last_char) {
        return;
    }
    g = &f->glyphs[c - f->first_char];

    /* 行框顶部到字形顶部的偏移（base_line 从行底向上为正） */
    gx = (int16_t)(x + g->ofs_x * scale);
    gy = (int16_t)(y + (f->line_height - f->base_line - g->box_h - g->ofs_y) * scale);

    for (row = 0; row < g->box_h; row++) {
        for (col = 0; col < g->box_w; col++) {
            uint8_t alpha = f->bitmap[g->bitmap_index + (uint16_t)row * g->box_w + col];
            uint16_t color;
            if (alpha == 0) {
                continue;
            }
            color = (alpha >= 250) ? fg : mui_color_mix(fg, bg, alpha);
            mui_fill_rect((int16_t)(gx + col * scale),
                          (int16_t)(gy + row * scale),
                          scale, scale, color);
        }
    }
}

void mui_lv_font_draw_text(int16_t x, int16_t y, const char *s, const mui_lv_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale)
{
    while (*s) {
        if (*s >= f->first_char && *s <= f->last_char) {
            mui_lv_font_draw_char(x, y, *s, f, fg, bg, scale);
            /* 步进宽度：adv_w 为 1/16 像素，四舍五入 */
            x = (int16_t)(x + ((f->glyphs[*s - f->first_char].adv_w + 8) / 16) * scale);
        }
        s++;
    }
}

int16_t mui_lv_font_text_width(const char *s, const mui_lv_font_t *f, int16_t scale)
{
    int16_t w = 0;
    while (*s) {
        if (*s >= f->first_char && *s <= f->last_char) {
            w = (int16_t)(w + ((f->glyphs[*s - f->first_char].adv_w + 8) / 16) * scale);
        }
        s++;
    }
    return w;
}

/* -------- 5x7 点阵字体 -------- */

/* -------- 5x7 字模表（按需收录，可逐步扩充） -------- */
static const struct {
    char ch;
    uint8_t rows[7];
} font5x7[] = {
    {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
    {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
    {'2', {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
    {'3', {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
    {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
    {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
    {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
    {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    {'%', {0x0C,0x0C,0x01,0x02,0x04,0x06,0x06}},
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'D', {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I', {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
    {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
    {'N', {0x11,0x11,0x13,0x15,0x19,0x11,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'d', {0x01,0x01,0x0F,0x11,0x11,0x11,0x0F}},
    {'e', {0x00,0x00,0x0E,0x11,0x1F,0x10,0x0E}},
    {'o', {0x00,0x00,0x0E,0x11,0x11,0x11,0x0E}},
    {'z', {0x00,0x1F,0x02,0x04,0x08,0x1F,0x00}},
    {':', {0x00,0x04,0x00,0x00,0x00,0x04,0x00}},
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
};

void mui_font_draw_char(int16_t x, int16_t y, char c, uint16_t color, int16_t scale)
{
    size_t i;
    for (i = 0; i < sizeof(font5x7) / sizeof(font5x7[0]); i++) {
        if (font5x7[i].ch == c) {
            int row, col;
            for (row = 0; row < 7; row++) {
                for (col = 0; col < 5; col++) {
                    if (font5x7[i].rows[row] & (0x10 >> col)) {
                        mui_fill_rect((int16_t)(x + col * scale),
                                      (int16_t)(y + row * scale),
                                      scale, scale, color);
                    }
                }
            }
            return;
        }
    }
}

void mui_font_draw_text(int16_t x, int16_t y, const char *s, uint16_t color,
                        int16_t scale, int16_t spacing)
{
    while (*s) {
        mui_font_draw_char(x, y, *s, color, scale);
        x = (int16_t)(x + (5 + spacing) * scale);
        s++;
    }
}

int16_t mui_font_text_width(const char *s, int16_t scale, int16_t spacing)
{
    int16_t n = 0;

    while (*s) {
        n++;
        s++;
    }
    return (int16_t)(n == 0 ? 0 : (n * (5 + spacing) - spacing) * scale);
}
