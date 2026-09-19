/**
 * @file mui_button.h
 * @brief MUI 按钮控件（立即模式绘制 + 保留模式对象）
 *
 * 依赖 mui.h（图形原语/触摸）与 mui_font.h（文字渲染），平台无关。
 */

#ifndef MUI_BUTTON_H
#define MUI_BUTTON_H

#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------- 立即模式按钮（无状态，每帧调用） -------- */

/** @brief 按钮底板形状 */
typedef enum {
    MUI_BTN_SHAPE_ROUND = 0,  /**< 圆角（半径取 radius，<=0 时自动 h/4 且限 8） */
    MUI_BTN_SHAPE_RECT,       /**< 直角 */
    MUI_BTN_SHAPE_PILL,       /**< 胶囊（半径=高/2） */
} mui_button_shape_t;

/** @brief 按钮配色与形状方案（画之前配置好，绘制时传入） */
typedef struct {
    uint16_t bg;              /**< 弹起底色 */
    uint16_t bg_press;        /**< 按下底色 */
    uint16_t fg;              /**< 文字/图标色 */
    uint16_t border;          /**< 边框色（与 bg 同色则视觉无边框） */
    uint8_t shape;            /**< 底板形状（mui_button_shape_t） */
    int16_t radius;           /**< 圆角半径：<=0 自动（h/4 限 8），>0 指定（自动限 h/2） */
} mui_button_style_t;

/** @brief 默认按钮配色（蓝底白字） */
extern const mui_button_style_t mui_button_style_default;

/**
 * @brief 绘制按钮（圆角底板 + 居中点阵文字 + 按下视觉反馈）
 *
 * 立即模式控件：无内部状态，每帧调用，按下状态由外部传入。
 * 按下时底色切换为 bg_press，文字下移 1 像素。
 * @param x        左上角横坐标
 * @param y        左上角纵坐标
 * @param w        宽度
 * @param h        高度
 * @param text     按钮文字（5x7 点阵字体，NULL 画纯色块）
 * @param pressed  0=弹起，非 0=按下
 * @param style    配色（NULL 使用 mui_button_style_default）
 */
void mui_button(int16_t x, int16_t y, int16_t w, int16_t h,
                const char *text, uint8_t pressed,
                const mui_button_style_t *style);

/**
 * @brief 绘制图标按钮（8bpp alpha 蒙版图标[+文字]自动居中 + 按下视觉反馈）
 *
 * 图标前景色取 style->fg，与按钮底色逐像素混合，边缘平滑无锯齿。
 * 图标与文字组合整体水平居中：图标在左、文字在右，间距 4 像素；
 * 只传 icon 或只传 text 时单独居中。按下时内容整体下移 1 像素。
 * 彩色图标（多色位图）请用 mui_button + mui_draw_bitmap_key 组合实现。
 * @param x        左上角横坐标
 * @param y        左上角纵坐标
 * @param w        宽度
 * @param h        高度（图标建议不大于 h-6）
 * @param icon     alpha 蒙版资源（mui_image_alpha_t*，NULL 则不画图标）
 * @param text     按钮文字（NULL 则不画文字）
 * @param pressed  0=弹起，非 0=按下
 * @param style    配色（NULL 使用 mui_button_style_default）
 */
void mui_button_icon(int16_t x, int16_t y, int16_t w, int16_t h,
                     const mui_image_alpha_t *icon, const char *text,
                     uint8_t pressed, const mui_button_style_t *style);

/* -------- 按钮对象（保留模式 · OO 风格） -------- */

/** @brief 按钮对象：静态分配，一次 init 后只需更新状态，每帧 draw */
typedef struct {
    int16_t x;                         /**< 左上角横坐标 */
    int16_t y;                         /**< 左上角纵坐标 */
    int16_t w;                         /**< 宽度 */
    int16_t h;                         /**< 高度 */
    const mui_button_style_t *style;   /**< 样式（指针，NULL 用默认） */
    const char *text;                  /**< 文字（指针，由调用者管理存储） */
    const mui_image_alpha_t *icon;     /**< alpha 蒙版图标（NULL 则不画） */
    const void *font;                  /**< 抗锯齿字体（将 mui_lv_font_t* 传入，NULL 用 5x7 点阵） */
    uint8_t pressed;                   /**< 0=弹起，非 0=按下 */
    uint8_t enabled;                   /**< 0=禁用（灰色遮罩），非 0=可用 */
    uint8_t visible;                   /**< 0=隐藏，非 0=显示 */
    uint8_t latch;                     /**< 1=锁存（开关）：点击切换并保持按下，再次点击弹起；0=瞬态（默认） */
} mui_button_t;

/**
 * @brief 初始化按钮对象（静态分配后调用一次）
 * @param btn    按钮对象指针（由调用者静态/栈上分配）
 * @param x      左上角横坐标
 * @param y      左上角纵坐标
 * @param w      宽度
 * @param h      高度
 * @param style  配色样式（NULL 使用 mui_button_style_default）
 * @param text   文字（NULL 则不画文字）
 * @param icon   alpha 蒙版图标（NULL 则不画图标）
 */
void mui_button_init(mui_button_t *btn, int16_t x, int16_t y, int16_t w, int16_t h,
                     const mui_button_style_t *style, const char *text,
                     const mui_image_alpha_t *icon);

/** @brief 设置按下状态 */
void mui_button_set_pressed(mui_button_t *btn, uint8_t pressed);

/** @brief 设置使能（禁用时绘制灰色遮罩，不响应交互） */
void mui_button_set_enabled(mui_button_t *btn, uint8_t enabled);

/** @brief 设置可见性 */
void mui_button_set_visible(mui_button_t *btn, uint8_t visible);

/** @brief 设置文字（传入指针，调用者负责存储生命周期） */
void mui_button_set_text(mui_button_t *btn, const char *text);

/** @brief 设置图标 */
void mui_button_set_icon(mui_button_t *btn, const mui_image_alpha_t *icon);

/** @brief 设置抗锯齿字体（传入 mui_lv_font_t*，NULL 恢复 5x7 点阵） */
void mui_button_set_font(mui_button_t *btn, const void *font);

/** @brief 移动按钮位置 */
void mui_button_set_pos(mui_button_t *btn, int16_t x, int16_t y);

/** @brief 设置锁存模式（1=开关/锁存，0=瞬态；瞬态为默认） */
void mui_button_set_latch(mui_button_t *btn, uint8_t latch);

/**
 * @brief 每帧调用：按当前状态绘制按钮
 * @param btn 按钮对象
 */
void mui_button_draw(const mui_button_t *btn);

/**
 * @brief 按钮触摸处理：喂入触摸事件，自动维护按下高亮
 *
 * 配合 mui_touch_poll 使用：DOWN 命中则按钮按下高亮；
 * 抬起时自动弹起，若为完整点击且仍在按钮区域内则返回 1。
 * @param btn  按钮对象
 * @param ev   当前触摸事件（MUI_TOUCH_NONE 直接返回 0）
 * @return 1 = 本次事件构成对该按钮的一次点击
 */
uint8_t mui_button_touch(mui_button_t *btn, mui_touch_event_t ev);

#ifdef __cplusplus
}
#endif

#endif /* MUI_BUTTON_H */
