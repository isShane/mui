/**
 * @file mui_toggle.h
 * @brief MUI 状态类控件：开关 Switch / 复选 Checkbox / 单选 Radio
 *        （保留模式 · 触摸切换 · 可选抗锯齿）
 *
 * 三者都在本模块内，风格与 mui_button 一致：init 一次 → set_* → 每帧 draw，
 * 触摸事件喂给对应的 *_touch（返回 1 表示状态发生了变化）。
 */

#ifndef MUI_TOGGLE_H
#define MUI_TOGGLE_H

#include "mui.h"
#include "mui_font.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 开关 Switch ================= */

/** @brief 开关样式（胶囊轨道 + 圆钮） */
typedef struct {
    uint16_t off;         /**< 关态轨道色 */
    uint16_t on;          /**< 开态轨道色 */
    uint16_t knob;        /**< 圆钮色 */
    uint16_t knob_border; /**< 圆钮边框色（与 knob 同色则无边框） */
    uint16_t text;        /**< 标签文字色 */
    uint16_t screen_bg;   /**< 抗锯齿混色底色（aa=1 时必填） */
    uint8_t  aa;          /**< 圆角抗锯齿 */
    int16_t  gap;         /**< 文字与开关的间距 */
} mui_switch_style_t;

extern const mui_switch_style_t mui_switch_style_default;

/** @brief 开关对象（x,y,w,h 为整行矩形：控件在左、标签在右；本体尺寸由 h 决定） */
typedef struct {
    int16_t x, y, w, h;
    const mui_switch_style_t *style;
    const char *text;
    const mui_font_t *font;
    uint8_t on;
    uint8_t enabled, visible;
} mui_switch_t;

void mui_switch_init(mui_switch_t *sw, int16_t x, int16_t y, int16_t w, int16_t h,
                     uint8_t on, const mui_switch_style_t *style);
void mui_switch_set_on(mui_switch_t *sw, uint8_t on);
uint8_t mui_switch_get_on(const mui_switch_t *sw);
void mui_switch_set_text(mui_switch_t *sw, const char *text);
void mui_switch_set_font(mui_switch_t *sw, const mui_font_t *font);
void mui_switch_set_style(mui_switch_t *sw, const mui_switch_style_t *style);
void mui_switch_set_pos(mui_switch_t *sw, int16_t x, int16_t y);
void mui_switch_set_visible(mui_switch_t *sw, uint8_t visible);
void mui_switch_draw(const mui_switch_t *sw);
uint8_t mui_switch_touch(mui_switch_t *sw, mui_touch_event_t ev);

/* ================= 复选 Checkbox ================= */

/** @brief 复选框样式（方框 + 勾） */
typedef struct {
    uint16_t bg;          /**< 未选中盒底色 */
    uint16_t on;          /**< 选中盒底色 */
    uint16_t border;      /**< 盒边框色 */
    uint16_t mark;        /**< 勾颜色 */
    uint16_t text;        /**< 标签文字色 */
    uint16_t screen_bg;   /**< 抗锯齿混色底色 */
    int16_t  size;        /**< 盒边长（<=0 自动：h） */
    int16_t  gap;         /**< 文字与盒的间距 */
    uint8_t  aa;
} mui_checkbox_style_t;

extern const mui_checkbox_style_t mui_checkbox_style_default;

/** @brief 复选框对象 */
typedef struct {
    int16_t x, y, w, h;
    const mui_checkbox_style_t *style;
    const char *text;
    const mui_font_t *font;
    uint8_t checked;
    uint8_t enabled, visible;
} mui_checkbox_t;

void mui_checkbox_init(mui_checkbox_t *cb, int16_t x, int16_t y, int16_t w, int16_t h,
                       uint8_t checked, const mui_checkbox_style_t *style);
void mui_checkbox_set_checked(mui_checkbox_t *cb, uint8_t checked);
uint8_t mui_checkbox_get_checked(const mui_checkbox_t *cb);
void mui_checkbox_set_text(mui_checkbox_t *cb, const char *text);
void mui_checkbox_set_font(mui_checkbox_t *cb, const mui_font_t *font);
void mui_checkbox_set_style(mui_checkbox_t *cb, const mui_checkbox_style_t *style);
void mui_checkbox_set_pos(mui_checkbox_t *cb, int16_t x, int16_t y);
void mui_checkbox_set_visible(mui_checkbox_t *cb, uint8_t visible);
void mui_checkbox_draw(const mui_checkbox_t *cb);
uint8_t mui_checkbox_touch(mui_checkbox_t *cb, mui_touch_event_t ev);

/* ================= 单选 Radio ================= */

/** @brief 单选框样式（圆环 + 圆点） */
typedef struct {
    uint16_t ring;        /**< 外环色 */
    uint16_t on;          /**< 选中的圆点色 */
    uint16_t inner;       /**< 内芯色（未选中时的空心） */
    uint16_t text;        /**< 标签文字色 */
    uint16_t screen_bg;   /**< 抗锯齿混色底色 */
    int16_t  size;        /**< 圆直径（<=0 自动：h） */
    int16_t  gap;         /**< 文字与圆的间距 */
    uint8_t  aa;
} mui_radio_style_t;

extern const mui_radio_style_t mui_radio_style_default;

/** @brief 单选框对象 */
typedef struct mui_radio_t {
    int16_t x, y, w, h;
    const mui_radio_style_t *style;
    const char *text;
    const mui_font_t *font;
    uint8_t selected;
    uint8_t enabled, visible;
    struct mui_radio_t **group;   /**< 互斥组：以 NULL 结尾的指针数组（可 NULL） */
} mui_radio_t;

void mui_radio_init(mui_radio_t *rd, int16_t x, int16_t y, int16_t w, int16_t h,
                    uint8_t selected, const mui_radio_style_t *style);
void mui_radio_set_selected(mui_radio_t *rd, uint8_t selected);
uint8_t mui_radio_get_selected(const mui_radio_t *rd);
void mui_radio_set_text(mui_radio_t *rd, const char *text);
void mui_radio_set_font(mui_radio_t *rd, const mui_font_t *font);
void mui_radio_set_style(mui_radio_t *rd, const mui_radio_style_t *style);
void mui_radio_set_pos(mui_radio_t *rd, int16_t x, int16_t y);
void mui_radio_set_visible(mui_radio_t *rd, uint8_t visible);
/** @brief 设置互斥组（以 NULL 结尾的 mui_radio_t* 数组；选中某项时自动清除同组其它项） */
void mui_radio_set_group(mui_radio_t *rd, mui_radio_t **group);
void mui_radio_draw(const mui_radio_t *rd);
uint8_t mui_radio_touch(mui_radio_t *rd, mui_touch_event_t ev);

#ifdef __cplusplus
}
#endif

#endif /* MUI_TOGGLE_H */
