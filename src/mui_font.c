/**
 * @file mui_font.c
 * @brief MUI 字体渲染实现（LVGL 转换字体，8bpp 灰度 alpha 混合）
 *
 * 装饰（加粗 / 下划线 / 删除线）由 flags 驱动，内部两条路径：
 *   - text_draw_char_flags()      稀疏绘制（整串 mui_text_draw_ex 逐像素写）
 *   - text_draw_char_cell_flags() 整格覆盖（mui_text_draw_cell_ex 逐行批量写）
 * 装饰线的纵向位置与需要覆盖的行数分别出自 text_line_rel() / mui_text_height()，
 * 两条路径共用同一套公式 —— 否则会出现"整格版少画一行下划线"这类对不齐问题。
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

/**
 * @brief 字符步进宽度（adv_w 为 1/16 像素，四舍五入后乘放大倍数）
 */
static int16_t text_char_step(const mui_glyph_dsc_t *g, int16_t scale)
{
    return (int16_t)(((g->adv_w + 8) / 16) * scale);
}

/**
 * @brief 装饰线宽度（随放大倍数，至少 1 像素）
 */
static int16_t text_line_w(int16_t scale)
{
    return (scale > 1) ? scale : 1;
}

/**
 * @brief 装饰线相对行框顶部的纵向位置
 * @param is_strike 0 = 下划线（基线下方 scale 像素起，可能落在行框外）；
 *                  1 = 删除线（行框顶到基线的中点，始终在行框内）
 * @note  基线相对行顶 = (line_height - base_line) * scale，与字形纵向放置同源
 */
static int16_t text_line_rel(const mui_font_t *f, int16_t scale, int is_strike)
{
    int16_t base = (int16_t)((f->line_height - f->base_line) * scale);

    return is_strike ? (int16_t)(base / 2) : (int16_t)(base + scale);
}

int16_t mui_text_height(const mui_font_t *f, int16_t scale, uint8_t flags)
{
    int16_t h;
    int16_t need;

    if (f == NULL || scale < 1) {
        return 0;
    }
    h = (int16_t)(f->line_height * scale);
    if (flags & MUI_TEXT_UNDERLINE) {
        need = (int16_t)(text_line_rel(f, scale, 0) + text_line_w(scale));
        if (need > h) {
            h = need;                      /* 下划线画在行框外：覆盖范围要延长 */
        }
    }
    return h;
}

/**
 * @brief 画整串装饰线（贯穿整串宽度，空格与字间间隙都连得上）
 * @param w 整串宽度（mui_text_width），<= 0 时不画
 */
static void text_decor_draw(int16_t x, int16_t y, int16_t w,
                            const mui_font_t *f, uint16_t fg, int16_t scale,
                            uint8_t flags)
{
    int16_t th = text_line_w(scale);
    int16_t i;

    if (w <= 0) {
        return;
    }
    if (flags & MUI_TEXT_UNDERLINE) {
        int16_t ly = (int16_t)(y + text_line_rel(f, scale, 0));
        for (i = 0; i < th; i++) {
            mui_hline_draw(x, (int16_t)(ly + i), w, fg);
        }
    }
    if (flags & MUI_TEXT_STRIKE) {
        int16_t ly = (int16_t)(y + text_line_rel(f, scale, 1));
        for (i = 0; i < th; i++) {
            mui_hline_draw(x, (int16_t)(ly + i), w, fg);
        }
    }
}

/**
 * @brief 内部：绘制单个字符（flags 含 MUI_TEXT_BOLD 时字形膨胀 1px）
 * @note  加粗用"取左右邻域最大值"（形态学膨胀）并裁到字形框内：
 *        步进 adv_w 与字形成像范围都不变，故串内位置与不加粗完全一致；
 *        也避免了"重画一遍"在 8bpp 下按错误底色混合导致边缘发脏。
 */
static void text_draw_char_flags(int16_t x, int16_t y, char c, const mui_font_t *f,
                                 uint16_t fg, uint16_t bg, int16_t scale, uint8_t flags)
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
        const uint8_t *bm = &f->bitmap[g->bitmap_index + (uint16_t)row * g->box_w];
        uint8_t prev = 0;                        /* 左邻原始 alpha（加粗用） */

        for (col = 0; col < g->box_w; col++) {
            uint8_t raw = bm[col];
            uint8_t alpha = raw;
            uint16_t color;

            if (flags & MUI_TEXT_BOLD) {
                uint8_t next = (col + 1 < g->box_w) ? bm[col + 1] : 0;
                if (prev > alpha) {
                    alpha = prev;
                }
                if (next > alpha) {
                    alpha = next;
                }
            }
            prev = raw;
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

void mui_text_draw_char(int16_t x, int16_t y, char c, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale)
{
    if (f == NULL || scale < 1) {
        return;
    }
    text_draw_char_flags(x, y, c, f, fg, bg, scale, 0);
}

void mui_text_draw_ex(int16_t x, int16_t y, const char *s, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale, uint8_t flags)
{
    int16_t x0 = x;
    const char *s0 = s;

    if (f == NULL || s == NULL || scale < 1) {
        return;
    }
    while (*s) {
        const mui_glyph_dsc_t *g = mui_font_find_glyph(f, *s);
        if (g != NULL) {
            text_draw_char_flags(x, y, *s, f, fg, bg, scale, flags);
            /* 步进宽度：adv_w 为 1/16 像素，四舍五入 */
            x = (int16_t)(x + text_char_step(g, scale));
        }
        s++;
    }
    /* 装饰线在字形之后画（整串一条，覆盖字形墨迹） */
    if (flags & (MUI_TEXT_UNDERLINE | MUI_TEXT_STRIKE)) {
        text_decor_draw(x0, y, mui_text_width(s0, f, scale), f, fg, scale, flags);
    }
}

void mui_text_draw(int16_t x, int16_t y, const char *s, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale)
{
    mui_text_draw_ex(x, y, s, f, fg, bg, scale, 0);
}

/* -------- 文本对齐 -------- */

int16_t mui_text_align_x(int16_t x, int16_t w, int16_t tw, uint8_t align)
{
    int16_t dx;

    if (align == MUI_ALIGN_CENTER) {
        dx = (int16_t)((w - tw) / 2);
    } else if (align == MUI_ALIGN_RIGHT) {
        dx = (int16_t)(w - tw);
    } else {
        dx = 0;
    }
    if (dx < 0) {
        dx = 0;                 /* 文本比框宽：退化为左对齐，不向左溢出 */
    }
    return (int16_t)(x + dx);
}

int16_t mui_text_align_y(int16_t y, int16_t h, int16_t th)
{
    int16_t dy = (int16_t)((h - th) / 2);

    if (dy < 0) {
        dy = 0;
    }
    return (int16_t)(y + dy);
}

void mui_text_draw_rect_ex(int16_t x, int16_t y, int16_t w, int16_t h,
                           const char *s, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale,
                           uint8_t align, uint8_t flags)
{
    int16_t tw;
    int16_t th;

    if (f == NULL || s == NULL || scale < 1) {
        return;
    }
    tw = mui_text_width(s, f, scale);
    th = mui_text_height(f, scale, flags);
    mui_text_draw_ex(mui_text_align_x(x, w, tw, align),
                     mui_text_align_y(y, h, th),
                     s, f, fg, bg, scale, flags);
}

void mui_text_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                        const char *s, const mui_font_t *f,
                        uint16_t fg, uint16_t bg, int16_t scale, uint8_t align)
{
    mui_text_draw_rect_ex(x, y, w, h, s, f, fg, bg, scale, align, 0);
}

/** @brief 整格覆盖绘制的行缓冲上限（= 最大步进像素宽，防越界） */
#define MUI_FONT_CELL_LINE_MAX   72

/**
 * @brief 内部：单个字符的"整格覆盖"绘制（就地替换，无先擦后画）
 * @note  覆盖行数 = mui_text_height(f, scale, flags)：行框（含放大倍数）
 *        加上行框外的下划线行；装饰线与整串绘制结果逐像素一致。
 */
static void text_draw_char_cell_flags(int16_t x, int16_t y, char c,
                                      const mui_font_t *f,
                                      uint16_t fg, uint16_t bg, int16_t scale,
                                      uint8_t flags)
{
    const mui_glyph_dsc_t *g;
    uint16_t line[MUI_FONT_CELL_LINE_MAX];
    int16_t step, gy_rel, rows, th, uy, sy;
    int row, col, i, px;

    g = mui_font_find_glyph(f, c);
    if (g == NULL || scale < 1) {
        return;
    }

    step = text_char_step(g, scale);                   /* 格宽 = 步进宽 */
    if (step < 1 || step > MUI_FONT_CELL_LINE_MAX) {
        return;                                        /* 超行缓冲上限：跳过 */
    }
    /* 字形在行框内的纵向起点（与 text_draw_char_flags 的 gy 计算一致） */
    gy_rel = (int16_t)((f->line_height - f->base_line - g->box_h - g->ofs_y) * scale);
    rows   = mui_text_height(f, scale, flags);
    th     = text_line_w(scale);
    uy     = text_line_rel(f, scale, 0);
    sy     = text_line_rel(f, scale, 1);

    for (row = 0; row < rows; row++) {
        /* 本行先铺背景色（旧内容被就地覆盖） */
        for (i = 0; i < step; i++) {
            line[i] = bg;
        }
        /* 行落在字形纵向范围内则叠加字形像素 */
        if (row >= gy_rel && row < gy_rel + g->box_h * scale) {
            const uint8_t *bm;
            uint8_t prev = 0;

            bm = &f->bitmap[g->bitmap_index
                            + (uint16_t)((row - gy_rel) / scale) * g->box_w];
            for (col = 0; col < g->box_w; col++) {
                uint8_t raw = bm[col];
                uint8_t alpha = raw;
                uint16_t color;

                if (flags & MUI_TEXT_BOLD) {
                    uint8_t next = (col + 1 < g->box_w) ? bm[col + 1] : 0;
                    if (prev > alpha) {
                        alpha = prev;
                    }
                    if (next > alpha) {
                        alpha = next;
                    }
                }
                prev = raw;
                color = (alpha == 0) ? bg
                                     : ((alpha >= 250) ? fg : mui_color_mix(fg, bg, alpha));
                for (i = 0; i < scale; i++) {
                    px = g->ofs_x * scale + col * scale + i;
                    if (px >= 0 && px < step) {
                        line[px] = color;
                    }
                }
            }
        }
        /* 装饰线铺满整格宽：与整串绘制一致（空格/间隙连得上、且覆盖字形墨迹） */
        if (((flags & MUI_TEXT_UNDERLINE) && row >= uy && row < uy + th) ||
            ((flags & MUI_TEXT_STRIKE) && row >= sy && row < sy + th)) {
            for (i = 0; i < step; i++) {
                line[i] = fg;
            }
        }
        /* 整行一次开窗连续写 */
        mui_image_draw(x, (int16_t)(y + row), step, 1, line);
    }
}

void mui_text_draw_char_cell(int16_t x, int16_t y, char c,
                                const mui_font_t *f,
                                uint16_t fg, uint16_t bg, int16_t scale)
{
    if (f == NULL || scale < 1) {
        return;
    }
    text_draw_char_cell_flags(x, y, c, f, fg, bg, scale, 0);
}

void mui_text_draw_cell_ex(int16_t x, int16_t y, const char *s,
                                const mui_font_t *f,
                                uint16_t fg, uint16_t bg, int16_t scale, uint8_t flags)
{
    if (f == NULL || s == NULL || scale < 1) {
        return;
    }
    while (*s) {
        const mui_glyph_dsc_t *g = mui_font_find_glyph(f, *s);
        if (g != NULL) {
            text_draw_char_cell_flags(x, y, *s, f, fg, bg, scale, flags);
            x = (int16_t)(x + text_char_step(g, scale));
        }
        s++;
    }
}

void mui_text_draw_cell(int16_t x, int16_t y, const char *s,
                                const mui_font_t *f,
                                uint16_t fg, uint16_t bg, int16_t scale)
{
    mui_text_draw_cell_ex(x, y, s, f, fg, bg, scale, 0);
}

int16_t mui_text_width(const char *s, const mui_font_t *f, int16_t scale)
{
    int16_t w = 0;

    if (f == NULL || s == NULL || scale < 1) {
        return 0;
    }
    while (*s) {
        const mui_glyph_dsc_t *g = mui_font_find_glyph(f, *s);
        if (g != NULL) {
            w = (int16_t)(w + text_char_step(g, scale));
        }
        s++;
    }
    return w;
}
