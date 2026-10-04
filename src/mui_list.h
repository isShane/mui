/**
 * @file mui_list.h
 * @brief MUI 可滚动列表（视口 + 行矩形 + 触摸滚动/选中，保留模式）
 *
 * 列表本身不关心"每行长什么样"：它只负责行高、滚动、可见范围、命中与选中，
 * 行内容由调用方在 mui_list_begin/end 之间逐行绘制。
 *
 * 典型用法：
 *     mui_list_begin(&lst);                       // 裁剪到视口
 *     mui_rect_fill(lst.box.x, lst.box.y, lst.box.w, lst.box.h, bg);   // 滚动时先清底
 *     for (i = 0; i < rows; i++) {
 *         mui_rect_t r = mui_list_row_rect(&lst, i);
 *         if (r.y2 <= lst.box.y || r.y >= lst.box.y + lst.box.h) continue;  // 视口外跳过
 *         ...画第 i 行...
 *     }
 *     mui_list_end(&lst);
 *
 * 触摸：把事件喂给 mui_list_touch，返回位掩码（选中/滚动变化）提示是否需重绘。
 */

#ifndef MUI_LIST_H
#define MUI_LIST_H

#include "mui.h"
#include "mui_layout.h"   /* mui_container_t */

#ifdef __cplusplus
extern "C" {
#endif

#define MUI_LIST_SEL_NONE        0xFF   /**< 无选中行 */

#define MUI_LIST_CHANGED_SEL     0x01   /**< 选中行发生了变化 */
#define MUI_LIST_CHANGED_SCROLL  0x02   /**< 滚动位置发生了变化 */

/** @brief 列表对象（静态分配） */
typedef struct {
    mui_container_t box;   /**< 视口与滚动（内容尺寸自动按行数算） */
    int16_t row_h;         /**< 行高 */
    int16_t gap;           /**< 行间距 */
    uint8_t rows;          /**< 数据行数 */
    uint8_t sel;           /**< 选中行（MUI_LIST_SEL_NONE = 无） */
    uint8_t dragging;      /**< 是否正在拖动 */
    uint8_t moved;         /**< 本次按下是否已判定为拖动 */
    int16_t drag_y;        /**< 按下时的触点 y */
    int16_t drag_scroll;   /**< 按下时的滚动值 */
} mui_list_t;

/**
 * @brief 初始化列表
 * @param x,y,w,h 视口矩形
 * @param row_h   行高
 * @param gap     行间距
 */
void mui_list_init(mui_list_t *l, int16_t x, int16_t y, int16_t w, int16_t h,
                   int16_t row_h, int16_t gap);

/** @brief 重新设置视口矩形 */
void mui_list_set_bounds(mui_list_t *l, int16_t x, int16_t y, int16_t w, int16_t h);

/** @brief 设置数据行数（会按新内容高度重新钳制滚动） */
void mui_list_set_rows(mui_list_t *l, uint8_t rows);

/** @brief 取数据行数 */
uint8_t mui_list_get_rows(const mui_list_t *l);

/** @brief 设置选中行（自动夹到 [0, rows)；MUI_LIST_SEL_NONE = 取消选中） */
void mui_list_set_sel(mui_list_t *l, uint8_t sel);

/** @brief 取选中行 */
uint8_t mui_list_get_sel(const mui_list_t *l);

/** @brief 最大滚动量（内容不超出视口时为 0） */
int16_t mui_list_max_scroll(const mui_list_t *l);

/** @brief 设置滚动偏移（自动钳制到 [0, max]） */
void mui_list_set_scroll(mui_list_t *l, int16_t scroll);

/** @brief 相对滚动 */
void mui_list_scroll_by(mui_list_t *l, int16_t dy);

/** @brief 取当前滚动偏移 */
int16_t mui_list_get_scroll(const mui_list_t *l);

/**
 * @brief 求当前可见行范围
 * @param first 输出：第一个可见行
 * @param count 输出：可见行数
 * @return 可见行数（0 = 视口内没有整行）
 */
uint8_t mui_list_visible(const mui_list_t *l, uint8_t *first, uint8_t *count);

/** @brief 第 index 行的屏幕矩形（已含滚动偏移；可能在视口外） */
mui_rect_t mui_list_row_rect(const mui_list_t *l, uint8_t index);

/**
 * @brief 命中测试：屏幕点落在哪一行
 * @param index 输出：行号
 * @return 1 = 命中某行，0 = 不在视口内 / 落在行间距上
 */
uint8_t mui_list_row_at(const mui_list_t *l, int16_t px, int16_t py, uint8_t *index);

/**
 * @brief 触摸处理：点按选中、上下拖动滚动
 * @return 位或 MUI_LIST_CHANGED_SEL / MUI_LIST_CHANGED_SCROLL；0 = 无变化
 */
uint8_t mui_list_touch(mui_list_t *l, mui_touch_event_t ev);

/** @brief 进入列表：把裁剪区收窄到视口（与 mui_list_end 成对） */
void mui_list_begin(mui_list_t *l);

/** @brief 退出列表：恢复进入前的裁剪区 */
void mui_list_end(mui_list_t *l);

/** @brief 用 bg 铺满视口并画 1px 边框（滚动/重建时先清底用；border==bg 则无边框） */
void mui_list_draw_frame(const mui_list_t *l, uint16_t bg, uint16_t border);

#ifdef __cplusplus
}
#endif

#endif /* MUI_LIST_H */
