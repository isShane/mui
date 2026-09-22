/**
 * @file mui_font.c
 * @brief MUI 字体渲染实现（LVGL 转换字体，8bpp 灰度 alpha 混合）
 */

#include "mui.h"
#include "mui_font.h"

/* -------- LVGL 转换字体渲染（8bpp alpha 混合） -------- */

/**
 * @brief 在字体中查找字符 c 的字形描述
 * @return 字形指针；字符不存在返回 NULL
 */
static const mui_glyph_dsc_t *mui_font_find_glyph(const mui_font_t *f, char c)
{
    uint16_t cp = (uint16_t)(unsigned char)c;

    if (f->cmap_count > 0 && f->cmaps != NULL) {
        uint16_t i;
        for (i = 0; i < f->cmap_count; i++) {
            if (cp >= f->cmaps[i].first_char && cp <= f->cmaps[i].last_char) {
                return &f->glyphs[f->cmaps[i].glyph_id + (uint16_t)(cp - f->cmaps[i].first_char)];
            }
        }
    } else if (c >= f->first_char && c <= f->last_char) {
        return &f->glyphs[cp - f->first_char];
    }
    return NULL;
}

void mui_text_draw_char(int16_t x, int16_t y, char c, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale)
{
    const mui_glyph_dsc_t *g;
    int16_t gx, gy;
    int row, col;

    g = mui_font_find_glyph(f, c);
    if (g == NULL) {
        return;
    }

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
            mui_rect_fill((int16_t)(gx + col * scale),
                          (int16_t)(gy + row * scale),
                          scale, scale, color);
        }
    }
}

void mui_text_draw(int16_t x, int16_t y, const char *s, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale)
{
    while (*s) {
        const mui_glyph_dsc_t *g = mui_font_find_glyph(f, *s);
        if (g != NULL) {
            mui_text_draw_char(x, y, *s, f, fg, bg, scale);
            /* 步进宽度：adv_w 为 1/16 像素，四舍五入 */
            x = (int16_t)(x + ((g->adv_w + 8) / 16) * scale);
        }
        s++;
    }
}

/** @brief 整格覆盖绘制的行缓冲上限（= 最大步进像素宽，防越界） */
#define MUI_FONT_CELL_LINE_MAX   72

void mui_text_draw_char_cell(int16_t x, int16_t y, char c,
                                const mui_font_t *f,
                                uint16_t fg, uint16_t bg, int16_t scale)
{
    const mui_glyph_dsc_t *g;
    uint16_t line[MUI_FONT_CELL_LINE_MAX];
    int16_t step, gy_rel, gr;
    int row, col, i, px;

    g = mui_font_find_glyph(f, c);
    if (g == NULL || scale < 1) {
        return;
    }

    step = (int16_t)(((g->adv_w + 8) / 16) * scale);   /* 格宽 = 步进宽 */
    if (step < 1 || step > MUI_FONT_CELL_LINE_MAX) {
        return;                                        /* 超行缓冲上限：跳过 */
    }
    /* 字形在行框内的纵向起点（与 draw_char 的 gy 计算一致） */
    gy_rel = (int16_t)((f->line_height - f->base_line - g->box_h - g->ofs_y) * scale);

    for (row = 0; row < f->line_height; row++) {
        /* 本行先铺背景色（旧内容被就地覆盖） */
        for (i = 0; i < step; i++) {
            line[i] = bg;
        }
        /* 行落在字形纵向范围内则叠加字形像素 */
        if (row >= gy_rel && row < gy_rel + g->box_h * scale) {
            const uint8_t *bm;

            gr = (int16_t)((row - gy_rel) / scale);    /* 字形内行号 */
            bm = &f->bitmap[g->bitmap_index + (uint16_t)gr * g->box_w];
            for (col = 0; col < g->box_w; col++) {
                uint8_t a = bm[col];
                uint16_t color = (a == 0) ? bg
                                 : ((a >= 250) ? fg : mui_color_mix(fg, bg, a));

                for (i = 0; i < scale; i++) {
                    px = g->ofs_x * scale + col * scale + i;
                    if (px >= 0 && px < step) {
                        line[px] = color;
                    }
                }
            }
        }
        /* 整行（含放大倍数行）一次开窗连续写 */
        mui_image_draw(x, (int16_t)(y + row), step, 1, line);
    }
}

void mui_text_draw_cell(int16_t x, int16_t y, const char *s,
                                const mui_font_t *f,
                                uint16_t fg, uint16_t bg, int16_t scale)
{
    while (*s) {
        const mui_glyph_dsc_t *g = mui_font_find_glyph(f, *s);
        if (g != NULL) {
            mui_text_draw_char_cell(x, y, *s, f, fg, bg, scale);
            x = (int16_t)(x + ((g->adv_w + 8) / 16) * scale);
        }
        s++;
    }
}

int16_t mui_text_width(const char *s, const mui_font_t *f, int16_t scale)
{
    int16_t w = 0;
    while (*s) {
        const mui_glyph_dsc_t *g = mui_font_find_glyph(f, *s);
        if (g != NULL) {
            w = (int16_t)(w + ((g->adv_w + 8) / 16) * scale);
        }
        s++;
    }
    return w;
}
