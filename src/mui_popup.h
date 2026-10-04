/**
 * @file mui_popup.h
 * @brief MUI 模态弹窗（标题 + 正文 + 最多 3 个按钮）
 *
 * 使用方式（弹窗必须画在最后、触摸最先收到事件）：
 *     // 每帧
 *     ui_draw_page();                 // 正常界面
 *     mui_popup_draw(&popup);         // 打开时叠在最上层
 *     // 触摸
 *     int8_t r = mui_popup_touch(&popup, ev);
 *     if (r >= 0 || r == MUI_POPUP_CLOSED) { ui_draw_page(); }   // 关闭后重绘底层
 *
 * 重要限制：本库没有图层/离屏缓冲，弹窗会**覆盖**其下的像素；关闭后必须由
 * 应用重绘底层内容（返回 MUI_POPUP_CLOSED / 按钮索引即提示这一点）。
 * 打开期间弹窗会"吃掉"全部触摸（模态）；点击面板外区域等同取消。
 */

#ifndef MUI_POPUP_H
#define MUI_POPUP_H

#include "mui.h"
#include "mui_font.h"
#include "mui_button.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUI_POPUP_MAX_BTN   3      /**< 最多按钮数 */

#define MUI_POPUP_NONE     (-1)    /**< 弹窗未打开 / 无事发生（无需处理） */
#define MUI_POPUP_CLOSED   (-2)    /**< 已关闭但未点按钮（如点面板外）→ 需重绘底层 */

/** @brief 弹窗对象（静态分配） */
typedef struct {
    int16_t x, y, w, h;                 /**< 面板矩形 */
    const char *title;                  /**< 标题（NULL 不画） */
    const char *msg;                    /**< 正文（NULL 不画） */
    const mui_font_t *font;             /**< 标题/正文字体（NULL 不画文字） */
    uint16_t bg, border;                /**< 面板底色 / 边框色 */
    uint16_t fg, title_fg;              /**< 正文色 / 标题色 */
    int16_t  pad;                       /**< 内容内边距 */
    int16_t  btn_h;                     /**< 按钮高度 */
    int16_t  radius;                    /**< 面板圆角 */
    mui_button_t btn[MUI_POPUP_MAX_BTN];/**< 按钮对象 */
    const char *btn_text[MUI_POPUP_MAX_BTN];               /**< 按钮文字（重排用） */
    const mui_button_style_t *btn_style[MUI_POPUP_MAX_BTN];/**< 按钮样式（重排用） */
    uint8_t  nbtn;                      /**< 按钮数 */
    uint8_t  open;                      /**< 是否打开 */
    uint8_t  needs_panel;               /**< 面板需重画（打开/换文案时置位） */
} mui_popup_t;

/**
 * @brief 初始化弹窗
 * @param x,y,w,h 面板矩形
 * @param title   标题（可为 NULL）
 * @param msg     正文（可为 NULL）
 * @param font    文字字体（可为 NULL，则不画文字）
 */
void mui_popup_init(mui_popup_t *p, int16_t x, int16_t y, int16_t w, int16_t h,
                    const char *title, const char *msg, const mui_font_t *font);

/** @brief 设置配色 */
void mui_popup_set_colors(mui_popup_t *p, uint16_t bg, uint16_t border,
                          uint16_t fg, uint16_t title_fg);

/** @brief 设置标题 / 正文（打开时自动重画面板） */
void mui_popup_set_text(mui_popup_t *p, const char *title, const char *msg);

/**
 * @brief 追加一个按钮（按数量自动等分底部一行）
 * @return 按钮索引；已满返回 -1
 */
int8_t mui_popup_add_button(mui_popup_t *p, const char *text,
                            const mui_button_style_t *style);

/** @brief 打开弹窗（置位面板重画标记） */
void mui_popup_open(mui_popup_t *p);

/** @brief 关闭弹窗（不重绘；应用负责重绘底层） */
void mui_popup_close(mui_popup_t *p);

/** @brief 是否打开 */
uint8_t mui_popup_is_open(const mui_popup_t *p);

/** @brief 每帧绘制（打开时叠在最上层；面板只在需要时重画，按钮走自身缓存） */
void mui_popup_draw(mui_popup_t *p);

/**
 * @brief 触摸处理（模态：打开时吃掉全部事件）
 * @return >= 0：被点按钮索引（弹窗已关闭）
 *         MUI_POPUP_CLOSED：已关闭但无按钮动作
 *         MUI_POPUP_NONE  ：未打开 / 无事发生
 */
int8_t mui_popup_touch(mui_popup_t *p, mui_touch_event_t ev);

#ifdef __cplusplus
}
#endif

#endif /* MUI_POPUP_H */
