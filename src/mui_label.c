/**
 * @file mui_label.c
 * @brief MUI 文本标签控件实现（保留模式 · 增量擦旧画新）
 */

#include "mui_label.h"
#include "mui.h"
#include "mui_font.h"

/** @brief 5x7 点阵字距（须与绘制时的传参一致） */
#define LABEL_SPACING 1

/**
 * @brief 覆盖式刷新开关：1 = 等长同宽文本走"整格覆盖"（无擦白阶段，防闪烁）
 * @note  覆盖绘制会在格内写背景色，要求文本下方为纯 bg；
 *        非等宽 / 变长文本自动回退到"擦旧画新"路径
 */
#ifndef MUI_LABEL_OPA_COVER
#define MUI_LABEL_OPA_COVER   1
#endif

/**
 * @brief 内部：按字体类型测量文本宽高
 */
static void label_measure(const mui_label_t *lbl, const char *s,
                          int16_t *w, int16_t *h)
{
    if (lbl->font != NULL) {
        const mui_lv_font_t *f = (const mui_lv_font_t *)lbl->font;
        *w = mui_lv_font_text_width(s, f, lbl->scale);
        *h = (int16_t)(f->line_height * lbl->scale);
    } else {
        *w = mui_font_text_width(s, lbl->scale, LABEL_SPACING);
        *h = (int16_t)(7 * lbl->scale);
    }
}

/**
 * @brief 内部：从指定横坐标开始按字体类型绘制文本
 * @param lbl 标签对象（纵向位置固定取 lbl->y）
 * @param x   起始横坐标
 * @param s   字符串
 */
static void label_paint_at(const mui_label_t *lbl, int16_t x, const char *s)
{
    if (lbl->font != NULL) {
        mui_lv_font_draw_text(x, lbl->y, s, (const mui_lv_font_t *)lbl->font,
                              lbl->fg, lbl->bg, lbl->scale);
    } else {
        mui_font_draw_text(x, lbl->y, s, lbl->fg, lbl->scale, LABEL_SPACING);
    }
}

/**
 * @brief 内部：绘制整串文本
 */
static void label_paint(const mui_label_t *lbl)
{
    label_paint_at(lbl, lbl->x, lbl->buf);
}

/**
 * @brief 内部：计算前 n 个字符占用的步进宽度
 *
 * 必须与绘制步进严格一致，否则"从中间起画"会错位：
 *  - LVGL 字体：adv_w 累加，与 mui_lv_font_draw_text 一致
 *  - 5x7 点阵：绘制步进恒为 (5+spacing)*scale，不能用 mui_font_text_width
 *    （后者少算末尾一个 spacing，仅用于包围盒测量）
 * @param lbl 标签对象
 * @param s   字符串
 * @param n   字符数（<= 0 返回 0）
 * @return 宽度（像素）
 */
static int16_t label_prefix_width(const mui_label_t *lbl, const char *s, int16_t n)
{
    char tmp[MUI_LABEL_MAX];
    int16_t i;

    if (n <= 0) {
        return 0;
    }
    if (lbl->font == NULL) {
        return (int16_t)(n * (5 + LABEL_SPACING) * lbl->scale);
    }
    for (i = 0; i < n && i < MUI_LABEL_MAX - 1 && s[i] != '\0'; i++) {
        tmp[i] = s[i];
    }
    tmp[i] = '\0';
    return mui_lv_font_text_width(tmp, (const mui_lv_font_t *)lbl->font, lbl->scale);
}

/**
 * @brief 内部：擦除上次绘制区域（bg 填充，四周留 2px 防残影）
 */
static void label_erase(const mui_label_t *lbl)
{
    mui_fill_rect((int16_t)(lbl->x - 2), (int16_t)(lbl->y - 2),
                  (int16_t)(lbl->last_w + 4), (int16_t)(lbl->last_h + 4),
                  lbl->bg);
}

void mui_label_init(mui_label_t *lbl, int16_t x, int16_t y,
                    const void *font, uint16_t fg, uint16_t bg, int16_t scale)
{
    lbl->x = x;
    lbl->y = y;
    lbl->font = font;
    lbl->fg = fg;
    lbl->bg = bg;
    lbl->scale = scale > 0 ? scale : 1;
    lbl->buf[0] = '\0';
    lbl->last_w = 0;
    lbl->last_h = 0;
    lbl->drawn = 0;
}

void mui_label_set_text(mui_label_t *lbl, const char *text)
{
    char old[MUI_LABEL_MAX];
    int16_t old_len = 0;
    int16_t new_len = 0;
    int16_t prefix = 0;
    int16_t left;
    int16_t left_x;
    int16_t erase_w;
    int16_t new_w;
    int16_t new_h;

    if (lbl == NULL || text == NULL) {
        return;
    }

    /* -------- 备份旧文本，供差异比对 -------- */
    while (old_len < MUI_LABEL_MAX - 1 && lbl->buf[old_len] != '\0') {
        old[old_len] = lbl->buf[old_len];
        old_len++;
    }
    old[old_len] = '\0';

    /* -------- 写入新文本（截断到缓冲上限） -------- */
    while (new_len < MUI_LABEL_MAX - 1 && text[new_len] != '\0') {
        lbl->buf[new_len] = text[new_len];
        new_len++;
    }
    lbl->buf[new_len] = '\0';

    /* -------- 求公共前缀（按字节） -------- */
    while (prefix < old_len && prefix < new_len &&
           old[prefix] == lbl->buf[prefix]) {
        prefix++;
    }
    /* UTF-8：前缀不得落在多字节字符内部，回退到首字节边界 */
    while (prefix > 0 &&
           ((prefix < old_len && ((uint8_t)old[prefix] & 0xC0) == 0x80) ||
            (prefix < new_len && ((uint8_t)lbl->buf[prefix] & 0xC0) == 0x80))) {
        prefix--;
    }

    /* -------- 文本完全未变：不擦不画 -------- */
    if (lbl->drawn && prefix == old_len && prefix == new_len) {
        return;
    }

    label_measure(lbl, lbl->buf, &new_w, &new_h);

    /* -------- 首次绘制：整串直接画 -------- */
    if (!lbl->drawn) {
        label_paint(lbl);
        lbl->last_w = new_w;
        lbl->last_h = new_h;
        lbl->drawn = 1;
        return;
    }

    /* -------- 增量更新：只擦除差异区间并重画 -------- */
#if MUI_LABEL_SAFE_SPAN
    /* 安全模式：回退一个字符，并向前对齐到完整 UTF-8 字符起点 */
    left = (prefix > 0) ? (int16_t)(prefix - 1) : 0;
    while (left > 0 && ((uint8_t)lbl->buf[left] & 0xC0) == 0x80) {
        left--;
    }
#else
    /* 精确模式：差异起点即前缀长度（prefix 已对齐 UTF-8 字符边界） */
    left = prefix;
#endif
    left_x = label_prefix_width(lbl, lbl->buf, left);

#if MUI_LABEL_OPA_COVER
    /* -------- 覆盖式快路径：等长同宽文本整格就地替换（无擦白阶段 → 不闪烁） -------- */
    if (lbl->font != NULL && old_len == new_len && lbl->last_w == new_w) {
        mui_lv_font_draw_text_cell((int16_t)(lbl->x + left_x), lbl->y,
                                   lbl->buf + left,
                                   (const mui_lv_font_t *)lbl->font,
                                   lbl->fg, lbl->bg, lbl->scale);
        lbl->last_w = new_w;
        lbl->last_h = new_h;
        return;
    }
#endif

    /* 只擦旧内容：更宽的新内容直接覆盖，无旧内容可擦时归零 */
    erase_w = (int16_t)(lbl->last_w - left_x);
    if (erase_w < 0) {
        erase_w = 0;
    }

    if (left == 0) {
        /* 从行首开始：保留原有 2 像素外扩，防字形左溢残影 */
        mui_fill_rect((int16_t)(lbl->x - 2), (int16_t)(lbl->y - 2),
                      (int16_t)(erase_w + 4),
                      (int16_t)((lbl->last_h > new_h ? lbl->last_h : new_h) + 4),
                      lbl->bg);
    } else {
        mui_fill_rect((int16_t)(lbl->x + left_x), (int16_t)(lbl->y - 2),
                      erase_w,
                      (int16_t)((lbl->last_h > new_h ? lbl->last_h : new_h) + 4),
                      lbl->bg);
    }
    label_paint_at(lbl, (int16_t)(lbl->x + left_x), lbl->buf + left);

    lbl->last_w = new_w;
    lbl->last_h = new_h;
}

void mui_label_set_pos(mui_label_t *lbl, int16_t x, int16_t y)
{
    if (lbl == NULL || !lbl->drawn) {
        return;
    }

    label_erase(lbl);
    lbl->x = x;
    lbl->y = y;
    label_paint(lbl);
}

void mui_label_draw(const mui_label_t *lbl)
{
    if (lbl == NULL || !lbl->drawn) {
        return;
    }
    label_paint(lbl);
}
