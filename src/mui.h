/**
 * @file mui.h
 * @brief MUI - 单片机裸机简易图形库（Mini UI）
 *
 * 特性：
 *  - 零动态内存；输出后端可配（直绘/全屏单缓冲/全屏双缓冲，见 mui_conf.h）：
 *    直绘后端零 RAM 直接写屏，缓冲后端用静态缓冲 + 帧末推屏
 *  - RGB565 颜色，所有图元颜色可调
 *  - 图元：点/直线/矩形(含圆角)/三角形/圆/椭圆（空心+实心）
 *  - 所有绘制函数自带边界裁剪，任意越界参数都安全
 *
 * 命名约定：mui_<对象>_<动作>[_<修饰>]
 *   例：mui_rect_fill（实心矩形）/ mui_circle_draw_aa（抗锯齿圆），详见 docs/naming.md
 *
 * 使用步骤：
 *  1. 实现 mui_port.h 中的 4 个移植函数
 *  2. 调用 mui_init(屏宽, 屏高) 完成初始化
 *  3. 调用各图元 API 绘制（对象在前、动作在后，全部自带裁剪）
 */

#ifndef MUI_H
#define MUI_H

#include <stdint.h>
#include <stddef.h>   /* NULL / size_t —— 裸机 -ffreestanding 下 stdint.h 不保证提供 */
#include "mui_conf.h"  /* 编译期配置：输出后端、缓冲尺寸等 */
#include "mui_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------- 版本 -------- */
#define MUI_VERSION_MAJOR 0
#define MUI_VERSION_MINOR 1

/* -------- 颜色定义（RGB565） -------- */

/** @brief 888颜色转RGB565（r/g/b 取值 0~255） */
#define MUI_RGB565(r, g, b) \
    ((uint16_t)((((uint16_t)(r) & 0xF8) << 8) | \
                (((uint16_t)(g) & 0xFC) << 3) | \
                ((uint16_t)(b) >> 3)))

#define MUI_BLACK       0x0000  /**< 黑 */
#define MUI_NAVY        0x000F  /**< 藏青 */
#define MUI_DARKGREEN   0x03E0  /**< 深绿 */
#define MUI_DARKCYAN    0x03EF  /**< 深青 */
#define MUI_MAROON      0x7800  /**< 栗色 */
#define MUI_PURPLE      0x780F  /**< 紫 */
#define MUI_OLIVE       0x7BE0  /**< 橄榄 */
#define MUI_LIGHTGREY   0xD69A  /**< 亮灰 */
#define MUI_DARKGREY    0x7BEF  /**< 暗灰 */
#define MUI_BLUE        0x001F  /**< 蓝 */
#define MUI_GREEN       0x07E0  /**< 绿 */
#define MUI_CYAN        0x07FF  /**< 青 */
#define MUI_RED         0xF800  /**< 红 */
#define MUI_MAGENTA     0xF81F  /**< 品红 */
#define MUI_YELLOW      0xFFE0  /**< 黄 */
#define MUI_WHITE       0xFFFF  /**< 白 */
#define MUI_ORANGE      0xFD20  /**< 橙 */
#define MUI_GREENYELLOW 0xB7E0  /**< 绿黄 */
#define MUI_PINK        0xFE19  /**< 粉 */

/* -------- 初始化 -------- */

/**
 * @brief 初始化图形库（内部调用 mui_port_init）
 * @param screen_w 屏幕宽度（像素）
 * @param screen_h 屏幕高度（像素）
 */
void mui_init(int16_t screen_w, int16_t screen_h);

/** @brief 获取屏幕宽度 */
int16_t mui_screen_get_width(void);

/** @brief 获取屏幕高度 */
int16_t mui_screen_get_height(void);

/* -------- 输出提交 -------- */

#if MUI_CFG_HAS_BUFFER
/**
 * @brief 把本帧绘制推送到屏幕
 *
 * 缓冲后端（单缓冲/双缓冲）下绘制只改内存，须在"一帧的全部绘制完成"后
 * 调用本函数；直绘后端下它被定义为空操作，应用代码可无条件调用。
 * 后端选择见 src/mui_conf.h 的 MUI_CFG_OUTPUT_MODE。
 */
void mui_screen_flush(void);
#else
#define mui_screen_flush()  ((void)0)
#endif

/* -------- 基础图元 -------- */

/**
 * @brief 画单个像素（越界自动忽略）
 * @param x     横坐标
 * @param y     纵坐标
 * @param color RGB565 颜色
 */
void mui_pixel_draw(int16_t x, int16_t y, uint16_t color);

/**
 * @brief 清屏（整屏填充指定颜色）
 * @param color RGB565 颜色
 */
void mui_screen_clear(uint16_t color);

/**
 * @brief 画任意直线（Bresenham算法，纯整数运算）
 * @param x0    起点横坐标
 * @param y0    起点纵坐标
 * @param x1    终点横坐标
 * @param y1    终点纵坐标
 * @param color RGB565 颜色
 */
void mui_line_draw(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

/**
 * @brief 画水平线
 * @param x     起始横坐标
 * @param y     纵坐标
 * @param w     线长（像素数，<1 则不绘制）
 * @param color RGB565 颜色
 */
void mui_hline_draw(int16_t x, int16_t y, int16_t w, uint16_t color);

/**
 * @brief 画垂直线
 * @param x     横坐标
 * @param y     起始纵坐标
 * @param h     线长（像素数，<1 则不绘制）
 * @param color RGB565 颜色
 */
void mui_vline_draw(int16_t x, int16_t y, int16_t h, uint16_t color);

/* -------- 矩形 -------- */

/**
 * @brief 实心矩形
 * @param x     左上角横坐标
 * @param y     左上角纵坐标
 * @param w     宽度（<1 则不绘制）
 * @param h     高度（<1 则不绘制）
 * @param color RGB565 颜色
 */
void mui_rect_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief 空心矩形（1像素边框）
 * @param x     左上角横坐标
 * @param y     左上角纵坐标
 * @param w     宽度（<1 则不绘制）
 * @param h     高度（<1 则不绘制）
 * @param color RGB565 颜色
 */
void mui_rect_draw(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

/**
 * @brief 实心圆角矩形
 * @param x     左上角横坐标
 * @param y     左上角纵坐标
 * @param w     宽度（<1 则不绘制）
 * @param h     高度（<1 则不绘制）
 * @param r     圆角半径（自动限制到 w/2 与 h/2，0 为直角）
 * @param color RGB565 颜色
 */
void mui_round_rect_fill(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t r, uint16_t color);

/**
 * @brief 空心圆角矩形（1像素边框）
 * @param x     左上角横坐标
 * @param y     左上角纵坐标
 * @param w     宽度（<1 则不绘制）
 * @param h     高度（<1 则不绘制）
 * @param r     圆角半径（自动限制到 w/2 与 h/2，0 为直角）
 * @param color RGB565 颜色
 */
void mui_round_rect_draw(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t r, uint16_t color);

/* -------- 三角形 -------- */

/**
 * @brief 实心三角形（扫描线填充，纯整数定点运算）
 * @param x0    顶点0横坐标
 * @param y0    顶点0纵坐标
 * @param x1    顶点1横坐标
 * @param y1    顶点1纵坐标
 * @param x2    顶点2横坐标
 * @param y2    顶点2纵坐标
 * @param color RGB565 颜色
 */
void mui_triangle_fill(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color);

/**
 * @brief 空心三角形（三条1像素边）
 * @param x0    顶点0横坐标
 * @param y0    顶点0纵坐标
 * @param x1    顶点1横坐标
 * @param y1    顶点1纵坐标
 * @param x2    顶点2横坐标
 * @param y2    顶点2纵坐标
 * @param color RGB565 颜色
 */
void mui_triangle_draw(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color);

/* -------- 圆与椭圆 -------- */

/**
 * @brief 实心圆
 * @param cx    圆心横坐标
 * @param cy    圆心纵坐标
 * @param r     半径（<0 不绘制）
 * @param color RGB565 颜色
 */
void mui_circle_fill(int16_t cx, int16_t cy, int16_t r, uint16_t color);

/**
 * @brief 空心圆（中点圆算法，1像素边）
 * @param cx    圆心横坐标
 * @param cy    圆心纵坐标
 * @param r     半径（<0 不绘制）
 * @param color RGB565 颜色
 */
void mui_circle_draw(int16_t cx, int16_t cy, int16_t r, uint16_t color);

/**
 * @brief 实心椭圆
 * @param cx    椭圆中心横坐标
 * @param cy    椭圆中心纵坐标
 * @param rx    横向半径（<1 不绘制）
 * @param ry    纵向半径（<1 不绘制）
 * @param color RGB565 颜色
 */
void mui_ellipse_fill(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                      uint16_t color);

/**
 * @brief 空心椭圆（中点椭圆算法，1像素边）
 * @param cx    椭圆中心横坐标
 * @param cy    椭圆中心纵坐标
 * @param rx    横向半径（<1 不绘制）
 * @param ry    纵向半径（<1 不绘制）
 * @param color RGB565 颜色
 */
void mui_ellipse_draw(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                      uint16_t color);

/* -------- 图片（位图资源） -------- */

/** @brief RGB565 位图资源描述 */
typedef struct {
    int16_t w;              /**< 宽度（像素） */
    int16_t h;              /**< 高度（像素） */
    const uint16_t *data;   /**< 像素数据（行优先，w*h 个） */
} mui_image_t;


/**
 * @brief 绘制 RGB565 位图（不透明）
 * @param x     左上角横坐标（越界自动裁剪）
 * @param y     左上角纵坐标（越界自动裁剪）
 * @param w     位图宽（<1 不绘制）
 * @param h     位图高（<1 不绘制）
 * @param data  像素数据（行优先，共 w*h 个 uint16_t）
 */
void mui_image_draw(int16_t x, int16_t y, int16_t w, int16_t h,
                     const uint16_t *data);

/**
 * @brief 绘制带透明色键的 RGB565 位图（精灵/图标）
 *
 * data 中等于 key 的像素不绘制（透出屏幕原有内容），
 * 每行不透明像素自动合并成连续块批量写入。
 * @param x     左上角横坐标（越界自动裁剪）
 * @param y     左上角纵坐标（越界自动裁剪）
 * @param w     位图宽（<1 不绘制）
 * @param h     位图高（<1 不绘制）
 * @param data  像素数据（行优先，共 w*h 个 uint16_t）
 * @param key   透明色键（常用 MUI_MAGENTA）
 */
void mui_image_draw_key(int16_t x, int16_t y, int16_t w, int16_t h,
                         const uint16_t *data, uint16_t key);

/** @brief 8bpp alpha 蒙版资源描述（亮度即透明度） */
typedef struct {
    int16_t w;             /**< 宽度（像素） */
    int16_t h;             /**< 高度（像素） */
    const uint8_t *data;   /**< 蒙版数据（行优先，w*h 个，0=透明 255=不透明） */
} mui_image_mask_t;

/**
 * @brief 绘制 8bpp alpha 蒙版（单色抗锯齿图标/精灵）
 *
 * 每像素按 alpha 与背景混合：a<8 跳过，a>=248 直画前景，其余 color_mix。
 * 同一份蒙版可用任意前景色绘制（单色线条图标推荐此格式）。
 * @param x     左上角横坐标（越界自动裁剪）
 * @param y     左上角纵坐标（越界自动裁剪）
 * @param w     蒙版宽（<1 不绘制）
 * @param h     蒙版高（<1 不绘制）
 * @param data  蒙版数据（行优先，共 w*h 个 uint8_t）
 * @param fg    前景色
 * @param bg    背景色（alpha 混合用）
 */
void mui_image_draw_mask(int16_t x, int16_t y, int16_t w, int16_t h,
                           const uint8_t *data, uint16_t fg, uint16_t bg);

/* -------- 颜色混合与抗锯齿 --------
 * 命名约定：函数名后缀 _aa = anti-aliasing（抗锯齿版），与同名硬边版本成对存在。
 *   例：mui_circle_draw（硬边 1px 圆） / mui_circle_draw_aa（边缘与背景混合过渡）。
 * 直绘模式回读不了屏幕，所以所有 _aa 版本都要多传一个背景色 bg 用于边缘混合 ——
 * 也正因为签名与硬边版不同，才必须靠后缀区分"用哪个"。
 *
 * bg 的两种用法（见 mui_out.h 的 MUI_OUT_CAN_READ_PIXEL）：
 *   缓冲后端 —— 库能回读屏幕，边缘直接与"底下真实的颜色"混合，bg 参数被忽略
 *               （形状可以压在任何背景：图片、渐变都正确）；
 *   直绘后端 —— 回读不了，用调用方传的 bg 混合（背景为纯色时效果最佳；传错会留脏边）。
 *   参数保留是为了让同一份应用代码在两种后端下都能编译。
 */

/**
 * @brief 两色线性混合（用于抗锯齿边缘过渡）
 * @param fg    前景色
 * @param bg    背景色
 * @param alpha 前景强度 0~255（0=纯背景，255=纯前景）
 * @return 混合后的 RGB565 颜色
 */
uint16_t mui_color_mix(uint16_t fg, uint16_t bg, uint8_t alpha);

/**
 * @brief 抗锯齿直线（Xiaolin Wu 算法，边缘双像素过渡）
 *
 * 直绘模式无法回读屏幕，需显式提供背景色；背景为纯色时效果最佳。
 * @param x0    起点横坐标
 * @param y0    起点纵坐标
 * @param x1    终点横坐标
 * @param y1    终点纵坐标
 * @param fg    前景 RGB565 颜色
 * @param bg    背景 RGB565 颜色
 */
void mui_line_draw_aa(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      uint16_t fg, uint16_t bg);

/**
 * @brief 抗锯齿圆（Wu 算法，边缘双像素过渡）
 *
 * 直绘模式无法回读屏幕，需显式提供背景色；背景为纯色时效果最佳。
 * @param cx    圆心横坐标
 * @param cy    圆心纵坐标
 * @param r     半径（<0 不绘制）
 * @param fg    前景 RGB565 颜色
 * @param bg    背景 RGB565 颜色
 */
void mui_circle_draw_aa(int16_t cx, int16_t cy, int16_t r,
                        uint16_t fg, uint16_t bg);

/**
 * @brief 抗锯齿实心圆角矩形（四角边缘与底色混合过渡）
 *
 * 与 mui_round_rect_fill 同构（主体十字 + 四角），区别是四角的边界像素按覆盖率
 * 与底色混合。做法上按行求满覆盖跨度后整行批量填充，只对真正需要过渡的边界像素
 * 逐点混色 —— 逐点写屏量约为"整个角区逐像素"的 1/5~1/9，每行只需常数次整数开方。
 *
 * 底色：缓冲后端自动读屏幕真实底色（可压在图片/渐变上）；直绘后端用 bg
 * （背景为纯色时效果最佳）。
 * @param x     左上角横坐标
 * @param y     左上角纵坐标
 * @param w     宽度（<1 则不绘制）
 * @param h     高度（<1 则不绘制）
 * @param r     圆角半径（自动限制到 w/2 与 h/2，0 为直角 → 退化为 mui_rect_fill）
 * @param fg    前景 RGB565 颜色
 * @param bg    背景 RGB565 颜色（仅直绘后端使用）
 */
void mui_round_rect_fill_aa(int16_t x, int16_t y, int16_t w, int16_t h,
                            int16_t r, uint16_t fg, uint16_t bg);

/* -------- 触摸 / 指针输入 -------- */

/** @brief 触摸事件类型 */
typedef enum {
    MUI_TOUCH_NONE = 0,     /**< 无事件 */
    MUI_TOUCH_DOWN,         /**< 按下 */
    MUI_TOUCH_MOVE,         /**< 按住移动（拖动） */
    MUI_TOUCH_UP,           /**< 抬起（按下与抬起间发生了拖动） */
    MUI_TOUCH_CLICK,        /**< 完整点击（按下与抬起位置接近） */
} mui_touch_event_t;

/**
 * @brief 喂入触摸状态（由驱动/模拟器调用，随时可调）
 *
 * 内部状态机自动生成事件序列（4 深度队列，一帧内按下又抬起不丢事件）。
 * @param x        触点横坐标（屏幕像素）
 * @param y        触点纵坐标（屏幕像素）
 * @param pressed  0=无触摸，非 0=按下
 */
void mui_touch_update(int16_t x, int16_t y, uint8_t pressed);

/**
 * @brief 取走一个触摸事件（每帧循环调用直到返回 NONE）
 * @return 事件类型；MUI_TOUCH_NONE 表示队列空
 */
mui_touch_event_t mui_touch_poll(void);

/**
 * @brief 获取当前触点坐标（配合 poll 使用，返回事件对应坐标）
 * @param x  输出：横坐标
 * @param y  输出：纵坐标
 */
void mui_touch_get_xy(int16_t *x, int16_t *y);

/**
 * @brief 命中测试：点是否落在矩形内（触摸/点击判定的基础函数）
 * @param x  矩形左上角横坐标
 * @param y  矩形左上角纵坐标
 * @param w  矩形宽度
 * @param h  矩形高度
 * @param px 测试点横坐标
 * @param py 测试点纵坐标
 * @return 1 命中，0 未命中
 */
uint8_t mui_rect_contains(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t px, int16_t py);

#ifdef __cplusplus
}
#endif

#endif /* MUI_H */
