/**
 * @file mui_gauge.h
 * @brief MUI 仪表盘控件（圆环进度 + 中心数值，保留模式 · 值未变不重绘）
 *
 * 由"整环轨道 + 起止角进度弧"组成，弧度由 min/max/value 线性映射；
 * 中心可选显示数值（或百分比）。依赖 mui.h 的圆环图元与 mui_font.h。
 *
 * 用法：mui_gauge_init 一次 → set_value 改值 → 每帧 mui_gauge_draw
 *       （值未变时 draw 直接返回，不写屏）。
 */

#ifndef MUI_GAUGE_H
#define MUI_GAUGE_H

#include "mui.h"
#include "mui_font.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 仪表盘配色与几何方案 */
typedef struct {
    uint16_t track;       /**< 轨道（未填充）色 */
    uint16_t fill;        /**< 进度弧色 */
    uint16_t text;        /**< 中心数值色 */
    uint16_t bg;          /**< 抗锯齿混色底色（表压在什么底色上就填什么） */
    int16_t  thick;       /**< 环宽（<=0 自动：外半径 / 5） */
    int16_t  a0;          /**< 起点角（度：0 = +x 轴，顺时针为正；默认 135） */
    int16_t  a1;          /**< 终点角（默认 a1 = a0 + 270） */
    uint8_t  aa;          /**< 圆环边缘抗锯齿（受全局 MUI_CFG_AA 约束） */
    uint8_t  show_value;  /**< 1 = 中心显示数值 */
    uint8_t  percent;     /**< 1 = 中心数值按百分比显示（否则显示原值） */
} mui_gauge_style_t;

/** @brief 默认样式（深色轨道 + 主题蓝进度 + 白字，270° 扫角，中心显示数值） */
extern const mui_gauge_style_t mui_gauge_style_default;

/** @brief 仪表盘对象（静态分配） */
typedef struct {
    int16_t cx, cy;                 /**< 圆心 */
    int16_t r;                      /**< 外半径 */
    const mui_gauge_style_t *style; /**< 样式（NULL 用默认） */
    const mui_font_t *font;         /**< 中心数值字体（NULL 则不画数值） */
    int16_t min, max, value;        /**< 取值与当前值 */
    uint8_t visible;                /**< 0 = 隐藏 */
    uint8_t drawn;                  /**< 是否已绘制过 */
    int16_t last_value;             /**< 上次绘制时的值（未变则跳过重绘） */
    mui_rect_t text_rect;           /**< 上次中心数值占的矩形（重绘前按它擦除） */
    uint8_t text_drawn;             /**< 是否画过中心数值 */
} mui_gauge_t;

/**
 * @brief 初始化仪表盘
 * @param cx,cy 圆心
 * @param r     外半径
 * @param min,max 取值范围（min < max）
 * @param value   初始值（自动夹到范围）
 * @param style   样式（NULL 用默认）
 */
void mui_gauge_init(mui_gauge_t *g, int16_t cx, int16_t cy, int16_t r,
                    int16_t min, int16_t max, int16_t value,
                    const mui_gauge_style_t *style);

/** @brief 设置当前值（自动夹到范围；值未变时下次 draw 不重绘） */
void mui_gauge_set_value(mui_gauge_t *g, int16_t value);

/** @brief 取当前值 */
int16_t mui_gauge_get_value(const mui_gauge_t *g);

/** @brief 设置取值范围（当前值自动夹入） */
void mui_gauge_set_range(mui_gauge_t *g, int16_t min, int16_t max);

/** @brief 换样式（NULL 恢复默认）；强制下次重绘 */
void mui_gauge_set_style(mui_gauge_t *g, const mui_gauge_style_t *style);

/** @brief 设置中心数值字体（NULL 则不画数值；是否显示还受 style->show_value 控制） */
void mui_gauge_set_font(mui_gauge_t *g, const mui_font_t *font);

/** @brief 移动圆心（强制下次重绘） */
void mui_gauge_set_pos(mui_gauge_t *g, int16_t cx, int16_t cy);

/** @brief 设置外半径（强制下次重绘） */
void mui_gauge_set_radius(mui_gauge_t *g, int16_t r);

/** @brief 设置可见性（重新显示时按全量重绘；隐藏不擦除） */
void mui_gauge_set_visible(mui_gauge_t *g, uint8_t visible);

/** @brief 每帧绘制（值与几何都没变时直接返回，不写屏） */
void mui_gauge_draw(mui_gauge_t *g);

/** @brief 作废绘制状态，强制下次 mui_gauge_draw 重绘（整屏被清后恢复用） */
void mui_gauge_invalidate(mui_gauge_t *g);

/** @brief 取某值对应的终点角（便于外部同步别的元素） */
int16_t mui_gauge_value_angle(const mui_gauge_t *g, int16_t value);

#ifdef __cplusplus
}
#endif

#endif /* MUI_GAUGE_H */
