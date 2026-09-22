/**
 * @file mui_font.h
 * @brief MUI 字体渲染（LVGL 转换字体，8bpp 灰度抗锯齿）
 *
 * 字体资源由 tools/lvgl_font_conv.py 从 LVGL 导出的 .c 转换生成；
 * 本文件只依赖图形 API 的调用方，平台无关。
 */

#ifndef MUI_FONT_H
#define MUI_FONT_H

#include <stdint.h>

/* -------- LVGL 转换字体（8bpp 灰度抗锯齿） -------- */

/** @brief 单个字形描述（与 LVGL glyph_dsc 字段对应） */
typedef struct {
    uint16_t bitmap_index;  /**< 位图起始偏移（字节） */
    uint16_t adv_w;         /**< 步进宽度（1/16 像素） */
    uint8_t box_w;          /**< 字形位图宽（像素） */
    uint8_t box_h;          /**< 字形位图高（像素） */
    int8_t ofs_x;           /**< 水平偏移 */
    int8_t ofs_y;           /**< 相对基线的垂直偏移 */
} mui_glyph_dsc_t;

/** @brief 字符映射段（对应 LVGL fmt_txt cmap，字符集可含多个非连续段，如数字+字母） */
typedef struct {
    uint16_t first_char;    /**< 本段起始字符 ASCII/Unicode */
    uint16_t last_char;     /**< 本段结束字符（含，长度=last-first+1） */
    uint16_t glyph_id;      /**< 本段第一个字符在 glyphs[] 中的下标 */
} mui_font_cmap_t;

/** @brief LVGL 转换字体描述 */
typedef struct {
    const uint8_t *bitmap;        /**< 8bpp 灰度位图数据 */
    const mui_glyph_dsc_t *glyphs;/**< 字形描述数组（按段顺序、段内连续排列） */
    const mui_font_cmap_t *cmaps; /**< 字符映射段表；NULL 时退化为单段(first_char~last_char) */
    uint16_t cmap_count;          /**< cmaps 段数（0=单段，用 first_char/last_char） */
    uint8_t first_char;           /**< 单段模式首字符 ASCII（cmap_count=0 时使用） */
    uint8_t last_char;            /**< 单段模式末字符 ASCII（cmap_count=0 时使用） */
    uint8_t line_height;          /**< 行高（像素） */
    int8_t base_line;             /**< 基线（从行底向上） */
} mui_font_t;

/**
 * @brief 绘制单个 LVGL 字体字符（8bpp alpha 混合，需纯色背景）
 * @param x      字符行框左上角 x
 * @param y      字符行框左上角 y
 * @param c      字符（超出字体范围则跳过）
 * @param f      字体描述
 * @param fg     前景色
 * @param bg     背景色（alpha 混合用）
 * @param scale  整数放大倍数
 */
void mui_text_draw_char(int16_t x, int16_t y, char c, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale);

/**
 * @brief 绘制 LVGL 字体字符串
 * @param x      行框左上角 x
 * @param y      行框左上角 y
 * @param s      字符串（无字形时该字符跳过并按空格步进）
 * @param f      字体描述
 * @param fg     前景色
 * @param bg     背景色
 * @param scale  整数放大倍数
 */
void mui_text_draw(int16_t x, int16_t y, const char *s, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale);

/**
 * @brief 绘制单字符"整格覆盖"版（就地替换，无先擦后画 → 无闪烁）
 *
 * 覆盖矩形 = 该字符的步进宽（adv_w） × 行高（line_height）：
 * 横向整格、纵向整行框；格内先铺背景色再叠字形像素，每行一次开窗连续写，
 * 旧内容被直接替换而不是"擦白再重画"。
 *
 * 适用：等宽/定长文本原地更新（数字读数、时钟等）；要求覆盖矩形下方为纯 bg 色
 * （格内其它内容会被涂掉）。无字形或放大后超出内部行缓冲时不绘制。
 * @param x      该字符格左上角 x（= 行框 x + 前面字符步进累计）
 * @param y      行框顶部 y
 * @param c      字符
 * @param f      字体描述
 * @param fg     前景色
 * @param bg     背景色（格内非字形像素写它）
 * @param scale  整数放大倍数
 */
void mui_text_draw_char_cell(int16_t x, int16_t y, char c,
                                const mui_font_t *f,
                                uint16_t fg, uint16_t bg, int16_t scale);

/**
 * @brief 绘制字符串"整格覆盖"版（逐字符调用 draw_char_cell，步进与 draw_text 一致）
 * @note  与 mui_text_draw 参数含义相同，差别仅在"就地覆盖、无擦白"
 */
void mui_text_draw_cell(int16_t x, int16_t y, const char *s,
                                const mui_font_t *f,
                                uint16_t fg, uint16_t bg, int16_t scale);

/**
 * @brief 计算 LVGL 字体字符串宽度
 * @param s      字符串
 * @param f      字体描述
 * @param scale  整数放大倍数
 * @return       宽度（像素）
 */
int16_t mui_text_width(const char *s, const mui_font_t *f, int16_t scale);

#endif /* MUI_FONT_H */
