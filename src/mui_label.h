/**
 * @file mui_label.h
 * @brief MUI 文本标签控件（保留模式 · 增量擦旧画新）
 *
 * 依赖 mui.h（图形原语）与 mui_font.h（字体渲染），平台无关。
 */

#ifndef MUI_LABEL_H
#define MUI_LABEL_H

#include <stdint.h>
#include "mui_font.h"   /* mui_font_t */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 标签内部文本缓冲上限（字节，含结束符） */
#define MUI_LABEL_MAX 32

/**
 * 增量更新安全模式：置 1 时重画起点向前回退一个字符。
 * 用于字形右边缘越过步进间隙的字体（如 harmony_os_10 的 & * Y _ k v x y），
 * 避免溢出列被擦除后不再重画而残留；代价是每帧多重画一个字符。
 * 项目在用的 harmony_os_10 / harmony_os_32 实测无字形右溢，故默认 0。
 * 可用编译选项 -DMUI_LABEL_SAFE_SPAN=1 覆盖。
 */
#ifndef MUI_LABEL_SAFE_SPAN
#define MUI_LABEL_SAFE_SPAN 0
#endif

/** @brief 文本标签对象：静态分配，一次 init 后 set_text 自动擦旧画新 */
typedef struct {
    int16_t x;                  /**< 左上角横坐标 */
    int16_t y;                  /**< 行框左上角纵坐标 */
    const mui_font_t *font;     /**< 文字字体（必传） */
    uint16_t fg;                /**< 前景色 */
    uint16_t bg;                /**< 背景色（擦除用，需与标签下方屏幕底色一致） */
    int16_t scale;              /**< 整数放大倍数 */
    char buf[MUI_LABEL_MAX];    /**< 当前文本（内部拷贝，更新无生命周期问题） */
    int16_t last_w;             /**< 上次绘制包围盒宽（擦除用） */
    int16_t last_h;             /**< 上次绘制包围盒高（擦除用） */
    uint8_t drawn;              /**< 是否已绘制过（首次 set_text 不擦除） */
} mui_label_t;

/**
 * @brief 初始化标签对象（静态分配后调用一次）
 * @param lbl   标签对象
 * @param x     行框左上角 x
 * @param y     行框左上角 y
 * @param font  文字字体（mui_font_t*，不可为 NULL）
 * @param fg    前景色
 * @param bg    背景色（需与标签下方屏幕底色一致，擦除依赖它）
 * @param scale 放大倍数
 */
void mui_label_init(mui_label_t *lbl, int16_t x, int16_t y,
                    const mui_font_t *font, uint16_t fg, uint16_t bg, int16_t scale);

/**
 * @brief 更新文本：从与上次文本的差异起点擦除并重画到行尾（无需记忆坐标）
 *
 * 先定位新旧文本的公共前缀（按字节比较，并回退到 UTF-8 字符边界），
 * 左边界再回退一个字符以防前缀末字符字形溢出；从该点擦除旧内容尾部
 * 并重画新内容尾部，公共前缀的像素不被触碰，显著减少写屏量。
 * 文本完全相同时不擦不画；整屏被清后需恢复请用 mui_label_draw 或
 * mui_label_set_pos。
 * @param lbl   标签对象
 * @param text  新文本（超长自动截断到 MUI_LABEL_MAX-1）
 */
void mui_label_set_text(mui_label_t *lbl, const char *text);

/**
 * @brief 移动标签位置：自动擦旧画新
 * @param lbl  标签对象
 * @param x    新左上角 x
 * @param y    新左上角 y
 */
void mui_label_set_pos(mui_label_t *lbl, int16_t x, int16_t y);

/**
 * @brief 无条件重绘当前文本（整屏被清后恢复用；常规更新用 set_text）
 * @param lbl 标签对象
 */
void mui_label_draw(const mui_label_t *lbl);

#ifdef __cplusplus
}
#endif

#endif /* MUI_LABEL_H */
