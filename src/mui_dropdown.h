/**
 * @file mui_dropdown.h
 * @brief MUI 下拉框（收起态是按钮，展开态是覆盖在下方的列表）
 *
 * 使用方式（下拉的展开列表必须画在最后、触摸最先收到事件）：
 *     ui_draw_page();
 *     mui_dropdown_draw(&dd);                       // 展开时叠在最上层
 *     uint8_t r = mui_dropdown_touch(&dd, ev);
 *     if (r == MUI_DROPDOWN_CLOSED) { ui_draw_page(); }   // 收起后重绘底层
 *
 * 限制同弹窗：无图层，展开的列表会覆盖其下像素，收起后需应用重绘底层。
 */

#ifndef MUI_DROPDOWN_H
#define MUI_DROPDOWN_H

#include "mui.h"
#include "mui_font.h"
#include "mui_button.h"
#include "mui_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUI_DROPDOWN_NONE     0   /**< 无事发生 */
#define MUI_DROPDOWN_CHANGED  1   /**< 下拉自身需要重绘（展开/滚动） */
#define MUI_DROPDOWN_CLOSED   2   /**< 已收起（选中或点外部）→ 需重绘底层 */

/** @brief 下拉框对象（静态分配） */
typedef struct {
    mui_button_t btn;                  /**< 收起态按钮（文字 = 当前选项） */
    mui_list_t   list;                 /**< 展开态列表 */
    const char *const *items;          /**< 选项文字数组 */
    uint8_t n;                         /**< 选项数 */
    uint8_t sel;                       /**< 当前选中项 */
    uint8_t open;                      /**< 是否展开 */
    int16_t row_h;                     /**< 行高 */
    int16_t gap;                       /**< 行间距 */
    uint8_t max_visible;               /**< 展开时最多显示行数 */
    const mui_font_t *font;
    const mui_button_style_t *btn_style;
    uint16_t bg, bg_sel;               /**< 选项底 / 选中底 */
    uint16_t fg, fg_sel;               /**< 选项字 / 选中字 */
    uint16_t border;                   /**< 展开列表边框 */
} mui_dropdown_t;

/**
 * @brief 初始化下拉框
 * @param x,y,w,h 收起态按钮矩形
 * @param items   选项文字数组（须长期有效）
 * @param n       选项数（<=0 视为 1）
 * @param row_h   展开列表行高
 * @param font    文字字体（可为 NULL）
 * @param btn_style 按钮样式（可为 NULL 用默认）
 */
void mui_dropdown_init(mui_dropdown_t *d, int16_t x, int16_t y, int16_t w, int16_t h,
                       const char *const *items, uint8_t n, int16_t row_h,
                       const mui_font_t *font, const mui_button_style_t *btn_style);

/** @brief 设置展开列表的配色 */
void mui_dropdown_set_colors(mui_dropdown_t *d, uint16_t bg, uint16_t bg_sel,
                             uint16_t fg, uint16_t fg_sel, uint16_t border);

/** @brief 设置展开时最多显示行数（默认 4） */
void mui_dropdown_set_max_visible(mui_dropdown_t *d, uint8_t rows);

/** @brief 取当前选中项下标 */
uint8_t mui_dropdown_selected(const mui_dropdown_t *d);

/** @brief 设置当前选中项（不重绘，只改状态） */
void mui_dropdown_set_selected(mui_dropdown_t *d, uint8_t sel);

/** @brief 是否展开 */
uint8_t mui_dropdown_is_open(const mui_dropdown_t *d);

/** @brief 展开（自动把选中项滚入可见区） */
void mui_dropdown_open(mui_dropdown_t *d);

/** @brief 收起（不重绘；应用负责重绘底层） */
void mui_dropdown_close(mui_dropdown_t *d);

/** @brief 展开列表的屏幕矩形（未展开时返回 0） */
uint8_t mui_dropdown_popup_rect(const mui_dropdown_t *d, mui_rect_t *r);

/** @brief 每帧绘制（展开时含覆盖在下方的列表） */
void mui_dropdown_draw(mui_dropdown_t *d);

/**
 * @brief 触摸处理
 * @return MUI_DROPDOWN_CLOSED 已收起（需重绘底层）
 *         MUI_DROPDOWN_CHANGED 展开/滚动变化（只需重绘下拉）
 *         MUI_DROPDOWN_NONE 无事发生
 */
uint8_t mui_dropdown_touch(mui_dropdown_t *d, mui_touch_event_t ev);

#ifdef __cplusplus
}
#endif

#endif /* MUI_DROPDOWN_H */
