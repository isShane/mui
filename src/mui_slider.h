/**
 * @file mui_slider.h
 * @brief MUI 滑块控件（保留模式 · 水平/垂直 · 触摸拖动 · 可选抗锯齿）
 *
 * 依赖 mui.h（图元/触摸）与 mui_font.h（可选的数值文字），平台无关。
 * 用法：mui_slider_init 一次 → set_value/set_range → 每帧 mui_slider_draw，
 *       触摸事件喂给 mui_slider_touch（返回 1 表示值发生了变化）。
 */

#ifndef MUI_SLIDER_H
#define MUI_SLIDER_H

#include "mui.h"
#include "mui_font.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 滑块方向 */
typedef enum {
    MUI_SLIDER_HORIZONTAL = 0,  /**< 水平：左=min，右=max */
    MUI_SLIDER_VERTICAL,        /**< 垂直：下=min，上=max */
} mui_slider_dir_t;

/** @brief 滑块配色与几何方案 */
typedef struct {
    uint16_t track;        /**< 轨道（未填充）色 */
    uint16_t fill;         /**< 已填充色 */
    uint16_t knob;         /**< 滑块填充色 */
    uint16_t knob_border;  /**< 滑块边框色（与 knob 同色则无边框） */
    uint16_t screen_bg;    /**< 抗锯齿混色用页面底色（aa=1 时必填） */
    int16_t  thickness;    /**< 轨道厚度（<=0 自动：短边/4） */
    int16_t  knob_r;       /**< 滑块半径（<=0 自动：thickness*3/4） */
    uint8_t  dir;          /**< mui_slider_dir_t */
    uint8_t  aa;           /**< 圆角/圆头抗锯齿 */
} mui_slider_style_t;

/** @brief 默认样式（深色轨道 + 主题蓝填充 + 白滑块） */
extern const mui_slider_style_t mui_slider_style_default;

/** @brief 滑块对象（静态分配） */
typedef struct {
    int16_t x, y, w, h;                /**< 外接矩形（含各方向的轨道长度） */
    const mui_slider_style_t *style;   /**< 样式（NULL 用默认） */
    int16_t min, max, value;           /**< 取值范围与当前值 */
    uint8_t dragging;                  /**< 是否正在拖动 */
    uint8_t enabled;                   /**< 0=禁用（不响应触摸） */
    uint8_t visible;                   /**< 0=隐藏 */
    uint8_t show_value;                /**< 1=在滑块旁显示数值（需 font） */
    const mui_font_t *font;            /**< 数值字体（NULL 则不画） */
} mui_slider_t;

/**
 * @brief 初始化滑块
 * @param s      对象
 * @param x,y,w,h 外接矩形
 * @param min,max 取值范围（min<max）
 * @param value   初始值（自动夹到范围）
 * @param style   样式（NULL 用默认）
 */
void mui_slider_init(mui_slider_t *s, int16_t x, int16_t y, int16_t w, int16_t h,
                     int16_t min, int16_t max, int16_t value,
                     const mui_slider_style_t *style);

/** @brief 设置取值范围（当前值自动夹入） */
void mui_slider_set_range(mui_slider_t *s, int16_t min, int16_t max);

/** @brief 设置当前值（夹到范围） */
void mui_slider_set_value(mui_slider_t *s, int16_t value);

/** @brief 取当前值 */
int16_t mui_slider_get_value(const mui_slider_t *s);

/** @brief 换样式（NULL 恢复默认） */
void mui_slider_set_style(mui_slider_t *s, const mui_slider_style_t *style);

/** @brief 移动位置 */
void mui_slider_set_pos(mui_slider_t *s, int16_t x, int16_t y);

/** @brief 设置可见性 */
void mui_slider_set_visible(mui_slider_t *s, uint8_t visible);

/** @brief 设置数值字体；show_value!=0 时在滑块旁显示数值 */
void mui_slider_set_font(mui_slider_t *s, const mui_font_t *font,
                         uint8_t show_value);

/**
 * @brief 每帧绘制
 * @note 绘制前会先把控件区域（含滑块圆头越出轨道的部分）擦成 style->screen_bg，
 *       因此滑块应放在与 screen_bg 同色的底上；拖动时不会留下拖影。
 * @warning 滑块的**实际占位** = 外接矩形向外扩 knob_r（圆头越出轨道），布局时
 *          必须按这个扩过的范围避让相邻控件，否则相邻控件会被擦掉一条。
 *          命中判定同样按扩过的范围。
 */
void mui_slider_draw(const mui_slider_t *s);

/**
 * @brief 取滑块的实际占位矩形（外接矩形向外扩圆头半径，并夹到屏幕内）
 * @param s 滑块对象
 * @param r 输出：占位矩形（开区间，见 mui_rect_t）
 * @return 1=成功，0=参数为空
 */
uint8_t mui_slider_get_bounds(const mui_slider_t *s, mui_rect_t *r);

/**
 * @brief 触摸处理：按下/拖动改变数值
 * @param s  滑块对象
 * @param ev 触摸事件
 * @return 1 = 本次事件改变了数值（可用于即时应用，如调亮度）
 */
uint8_t mui_slider_touch(mui_slider_t *s, mui_touch_event_t ev);

#ifdef __cplusplus
}
#endif

#endif /* MUI_SLIDER_H */
