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
 * 项目在用的几套字库（10 / 14 / 20 / 38 px）实测无字形右溢，故默认 0。
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
    uint8_t drawn;              /**< 是否已绘制过。置 0 后 set_text 走"整串稀疏绘制、不擦底"
                                 *   的首绘分支 —— **只有屏幕已被清空时才能清 0**；
                                 *   屏幕上有旧内容时清 0 会造成墨迹透出残影（旧字形落在
                                 *   新字形空腔/框外的像素不会被覆盖）。要"擦旧画新"请用
                                 *   mui_label_set_pos(自身坐标) 或 set_colors */
    uint8_t decor;              /**< 文字装饰（MUI_TEXT_* 位或，0 = 无） */
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
 * @brief 设置文字装饰（加粗 / 下划线 / 删除线）
 *
 * 等价于把 mui_text_draw_ex 的 flags 固化到标签上：
 *   - 擦除与重绘范围按 mui_text_height() 计算，故行框外的下划线行也管得到；
 *   - 改装饰后自动"擦旧画新"一次（未绘制过则只记录，下次首绘生效）。
 * @param lbl   标签对象
 * @param decor MUI_TEXT_* 位或（0 = 无装饰）
 */
void mui_label_set_decor(mui_label_t *lbl, uint8_t decor);

/**
 * @brief 改文字颜色（前景色）：整串按新色重画
 *
 * 字形几何不变、只换颜色，所以**不需要先擦除**：新墨恰好落在旧墨的同一批像素上
 * （抗锯齿边缘也是同一批像素、按同一个 bg 重新混色）。装饰线（下划线/删除线）
 * 与字形同色，会一起变。未绘制过则只记录，下次首绘生效。
 * @param lbl 标签对象
 * @param fg  新的前景色（字形与装饰线）
 */
void mui_label_set_fg(mui_label_t *lbl, uint16_t fg);

/**
 * @brief 改文字颜色 + 底色：按**新**底色擦掉旧内容后重画
 *
 * 换 bg 等于告诉标签"我下面的底色变了"，故擦除直接用新 bg 铺回旧区域。
 * 若只是文字换色、底色未变，用 mui_label_set_fg 更省写屏量（不整块铺底）。
 * @param lbl 标签对象
 * @param fg  新的前景色
 * @param bg  新的背景色（须与标签下方真实底色一致：擦除与抗锯齿混色都用它）
 */
void mui_label_set_colors(mui_label_t *lbl, uint16_t fg, uint16_t bg);

/**
 * @brief 无条件重绘当前文本（整屏被清后恢复用；常规更新用 set_text）
 * @param lbl 标签对象
 */
void mui_label_draw(const mui_label_t *lbl);

#ifdef __cplusplus
}
#endif

#endif /* MUI_LABEL_H */
