/**
 * @file mui_font.h
 * @brief MUI 字体渲染（LVGL 转换字体，8bpp 灰度抗锯齿）
 *
 * 字体资源由 tools/lvgl_font_conv.py 从 LVGL 导出的 .c 转换生成；
 * 本文件只依赖图形 API 的调用方，平台无关。
 *
 * 文字装饰（加粗 / 下划线 / 删除线）由 mui_text_draw_ex / mui_text_draw_cell_ex
 * 的 flags 参数控制（见 MUI_TEXT_* 宏），不带 _ex 的旧函数等价于 flags = 0。
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

/* -------- 文字装饰标志（mui_text_*_ex 的 flags 位，可或组合） --------
 * 加粗 = "软件加粗"（字形形态学膨胀，零资源、无需额外字库）；
 *        字号越小笔画占比越大，越容易糊，≥16px 效果较好。
 *        要更好的效果请用同族粗体字重另导一套字库，两者可叠加。
 * 下划线 / 删除线 = 纯装饰线（直线，轴对齐，无需抗锯齿，不占 Flash），
 *        画在整串宽度上，空格与字间间隙都连得上；
 *        **颜色与字形同色（fg）**，没有独立的装饰线颜色 —— 换字色时它一起变。
 */
#define MUI_TEXT_BOLD        0x01   /**< 加粗：字形膨胀 1px（步进与包围盒不变） */
#define MUI_TEXT_UNDERLINE   0x02   /**< 下划线：基线下方画线（可能落在行框外） */
#define MUI_TEXT_STRIKE      0x04   /**< 删除线：行顶与基线中点画线（在行框内） */

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
 * @param s      字符串（字符无字形时跳过：不绘制、也不步进，即宽度按 0 计）
 * @param f      字体描述
 * @param fg     前景色
 * @param bg     背景色
 * @param scale  整数放大倍数
 */
void mui_text_draw(int16_t x, int16_t y, const char *s, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale);

/**
 * @brief 绘制字符串（带装饰：加粗 / 下划线 / 删除线）
 *
 * 与 mui_text_draw 逐像素等价（flags = 0 时同一份实现），差别只在 flags：
 *   - MUI_TEXT_BOLD：字形按"取邻域最大值"膨胀 1px（左右各 1px，裁到字形框内），
 *     步进宽度 adv_w 与字形成像范围都不变 → 串内位置与不加粗完全一致；
 *     代价是 `alpha != 0` 的像素变多（写屏像素数增加）。
 *   - MUI_TEXT_UNDERLINE：基线下方 1px（随 scale 放大、线宽 = scale）画一条
 *     贯穿整串的直线，**可能落在行框外**（base_line 小的字体就是），需要的行数
 *     用 mui_text_height 查询；控件擦除区域要按它算。
 *   - MUI_TEXT_STRIKE：行顶与基线中点画线，始终落在行框内。
 * 装饰线的颜色与字形同色（fg），无抗锯齿需求（轴对齐直线覆盖率恒为 1）。
 * @param x      行框左上角 x
 * @param y      行框左上角 y
 * @param s      字符串
 * @param f      字体描述
 * @param fg     前景色（字形与装饰线）
 * @param bg     背景色（alpha 混合用）
 * @param scale  整数放大倍数
 * @param flags  MUI_TEXT_* 位或，0 = 无装饰
 */
void mui_text_draw_ex(int16_t x, int16_t y, const char *s, const mui_font_t *f,
                           uint16_t fg, uint16_t bg, int16_t scale, uint8_t flags);

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
 * @brief 绘制字符串"整格覆盖"版（带装饰：加粗 / 下划线 / 删除线）
 *
 * 每格覆盖矩形 = 步进宽 × mui_text_height(f, scale, flags)
 * （含行框外的下划线行，故旧内容被就地替换、不会残留上一条装饰线），
 * 装饰线与 mui_text_draw_ex 结果逐像素一致 —— 两条路径可自由互换。
 * @note  参数含义与 mui_text_draw_ex 相同，差别仅在"就地覆盖、无擦白"
 */
void mui_text_draw_cell_ex(int16_t x, int16_t y, const char *s,
                               const mui_font_t *f,
                               uint16_t fg, uint16_t bg, int16_t scale, uint8_t flags);

/**
 * @brief 计算 LVGL 字体字符串宽度
 *
 * 字体是比例字体（每个字形自己的步进宽 `adv_w`，1/16 像素定点），所以：
 *   宽度 = Σ(四舍五入到整像素的 adv_w) × scale，即"排版步进总宽"，
 *   与下划线/删除线画出来的线宽完全一致（装饰线就铺这么宽）。
 * 无字形的字符（含空格：本库字库不裁 U+0020）**不占宽度**；scale < 1 返回 0。
 * @param s      字符串
 * @param f      字体描述
 * @param scale  整数放大倍数
 * @return       宽度（像素）
 */
int16_t mui_text_width(const char *s, const mui_font_t *f, int16_t scale);

/**
 * @brief 计算文字（含装饰）占用的纵向行数
 *
 * 与 mui_text_width 成对：无装饰时即 line_height * scale（行框高）；
 * 带 MUI_TEXT_UNDERLINE 时下划线可能画在行框下方，返回值取到包括它
 * （删除线在行框内，不改变结果）。控件/调用方按它分配擦除或重绘范围。
 * @param f      字体描述（NULL 返回 0）
 * @param scale  整数放大倍数（< 1 返回 0）
 * @param flags  MUI_TEXT_* 位或，0 = 只算行框
 * @return       从行框顶部起需要覆盖的行数（像素）
 */
int16_t mui_text_height(const mui_font_t *f, int16_t scale, uint8_t flags);

#endif /* MUI_FONT_H */
