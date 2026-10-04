/**
 * @file mui_button.h
 * @brief MUI 按钮控件（保留模式对象 · OO 风格）
 *
 * 依赖 mui.h（图形原语/触摸）与 mui_font.h（文字渲染），平台无关。
 * 用法：mui_button_init 一次 → 改状态用 set_* → 每帧 mui_button_draw。
 */

#ifndef MUI_BUTTON_H
#define MUI_BUTTON_H

#include "mui.h"
#include "mui_font.h"   /* mui_font_t：文字渲染 */

#ifdef __cplusplus
extern "C" {
#endif

/* -------- 按钮底板形状与配色 -------- */

/** @brief 按钮底板形状 */
typedef enum {
    MUI_BUTTON_SHAPE_ROUND = 0,  /**< 圆角（半径取 radius，<=0 时自动 h/4 且限 8） */
    MUI_BUTTON_SHAPE_RECT,       /**< 直角 */
    MUI_BUTTON_SHAPE_PILL,       /**< 胶囊（半径=高/2） */
} mui_button_shape_t;

/** @brief 按钮配色与形状方案（画之前配置好，绘制时传入） */
typedef struct {
    uint16_t bg;              /**< 弹起底色 */
    uint16_t bg_press;        /**< 按下底色 */
    uint16_t fg;              /**< 文字/图标色 */
    uint16_t border;          /**< 边框色（与 bg 同色则视觉无边框） */
    uint8_t shape;            /**< 底板形状（mui_button_shape_t） */
    int16_t radius;           /**< 圆角半径：<=0 自动（h/4 限 8），>0 指定（自动限 h/2） */
    uint8_t aa;               /**< 底板圆角抗锯齿：1=开，0=关；同时受全局 MUI_CFG_AA 约束 */
    uint16_t screen_bg;       /**< 抗锯齿混色用的页面底色（aa=1 时必填；为 0 视为未设置，自动退化硬边） */
} mui_button_style_t;

/** @brief 默认按钮配色（蓝底白字） */
extern const mui_button_style_t mui_button_style_default;

/* -------- 按钮对象（保留模式 · OO 风格） -------- */

/** @brief 按钮对象：静态分配，一次 init 后只需更新状态，每帧 draw */
typedef struct {
    int16_t x;                         /**< 左上角横坐标 */
    int16_t y;                         /**< 左上角纵坐标 */
    int16_t w;                         /**< 宽度 */
    int16_t h;                         /**< 高度 */
    const mui_button_style_t *style;   /**< 样式（指针，NULL 用默认） */
    const char *text;                  /**< 文字（指针，由调用者管理存储） */
    const mui_image_mask_t *icon;     /**< alpha 蒙版图标（前景，NULL 则不画） */
    const mui_image_t *icon_img;       /**< 全彩图标（优先于蒙版图标；NULL 则用 icon） */
    const mui_image_t *bg_img;         /**< 底板贴图（非 NULL 时替代纯色底板，按自身 w/h 绘制） */
    const mui_image_t *bg_img_press;   /**< 按下态底板贴图（NULL 则沿用 bg_img） */
    uint16_t bg_key;                   /**< 底板贴图色键（bg_key_on=1 时透明） */
    uint16_t icon_key;                 /**< 全彩图标色键（icon_key_on=1 时透明） */
    int16_t  bg_l, bg_t, bg_r, bg_b;   /**< 九宫格四边边框（bg_nine=1 时用） */
    uint8_t  bg_nine;                  /**< 1=底板贴图走九宫格拉伸到按钮尺寸 */
    uint8_t  bg_key_on;                /**< 1=底板贴图启用色键透明 */
    uint8_t  icon_key_on;              /**< 1=全彩图标启用色键透明 */
    const mui_font_t *font;            /**< 文字字体（NULL 则不画文字） */
    uint8_t pressed;                   /**< 0=弹起，非 0=按下 */
    uint8_t enabled;                   /**< 0=禁用（灰色遮罩），非 0=可用 */
    uint8_t visible;                   /**< 0=隐藏，非 0=显示 */
    uint8_t latch;                     /**< 1=锁存（开关）：点击切换并保持按下，再次点击弹起；0=瞬态（默认） */

    /* ---- 绘制缓存（按需重绘）----
     * draw() 先把下列字段与上次绘制时的快照比对，完全一致则直接返回、不写屏。
     * 所有 set_* 接口在状态真正变化时自动作废缓存；触摸也在状态变化时作废。
     * 直接改结构体字段、或改写 text 指向的同一块缓冲区时，需显式调
     * mui_button_invalidate()。 */
    uint8_t  cache_valid;              /**< 缓存是否有效（0 = 下次 draw 必重绘） */
    uint8_t  c_pressed;                /**< 上次绘制时的按下状态 */
    uint8_t  c_enabled;                /**< 上次绘制时的使能状态 */
    uint8_t  c_visible;                /**< 上次绘制时的可见状态 */
    int16_t  c_x;                      /**< 上次绘制时的横坐标 */
    int16_t  c_y;                      /**< 上次绘制时的纵坐标 */
    const char *c_text;                /**< 上次绘制时的文字指针 */
    const mui_image_mask_t *c_icon;    /**< 上次绘制时的图标指针 */
    const void *c_font;                /**< 上次绘制时的字体指针 */
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
                     const mui_image_mask_t *icon);

/** @brief 设置按下状态 */
void mui_button_set_pressed(mui_button_t *btn, uint8_t pressed);

/** @brief 设置使能（禁用时绘制灰色遮罩，不响应交互） */
void mui_button_set_enabled(mui_button_t *btn, uint8_t enabled);

/** @brief 设置可见性 */
void mui_button_set_visible(mui_button_t *btn, uint8_t visible);

/** @brief 设置文字（传入指针，调用者负责存储生命周期） */
void mui_button_set_text(mui_button_t *btn, const char *text);

/** @brief 设置图标（alpha 蒙版，前景色绘制） */
void mui_button_set_icon(mui_button_t *btn, const mui_image_mask_t *icon);

/**
 * @brief 设置底板贴图（全彩 RGB565）
 *
 * 非 NULL 时底板改用它（按贴图自身 w/h 从按钮左上角绘制，故贴图尺寸应与按钮一致），
 * 不再画纯色圆角底板；文字/图标仍会叠在贴图上。
 * @param btn 按钮对象
 * @param img 贴图（NULL 恢复纯色底板）
 */
void mui_button_set_bg_image(mui_button_t *btn, const mui_image_t *img);

/** @brief 设置按下态底板贴图（NULL 则按下仍用 bg_img 那张） */
void mui_button_set_bg_image_press(mui_button_t *btn, const mui_image_t *img);

/** @brief 设置底板贴图 + 色键透明（源像素等于 key 处透出下层） */
void mui_button_set_bg_image_key(mui_button_t *btn, const mui_image_t *img,
                                 uint16_t key);

/**
 * @brief 让底板贴图按九宫格拉伸到按钮尺寸（圆角/边框不变形）
 * @param l,t,r,b 四边不缩放的边框宽度；与 set_bg_image 配合
 */
void mui_button_set_bg_nine(mui_button_t *btn,
                            int16_t l, int16_t t, int16_t r, int16_t b);

/** @brief 设置全彩图标（优先于蒙版图标；按原尺寸居中绘制） */
void mui_button_set_icon_image(mui_button_t *btn, const mui_image_t *img);

/** @brief 设置全彩图标 + 色键透明 */
void mui_button_set_icon_image_key(mui_button_t *btn, const mui_image_t *img,
                                   uint16_t key);

/** @brief 设置文字字体（NULL 则不画文字） */
void mui_button_set_font(mui_button_t *btn, const mui_font_t *font);

/**
 * @brief 换配色样式（NULL 恢复默认 mui_button_style_default）
 *
 * 按钮的颜色（含文字色 fg）全部由样式决定，**没有逐色 setter** ——
 * 要单独给某个按钮换色，就另定义一份 style 再切过来，避免"两个真值来源"。
 * 本函数只改状态，改完由调用者决定何时 mui_button_draw（与其它 set_* 一致）。
 * @param btn   按钮对象
 * @param style 新样式（可为 NULL）
 */
void mui_button_set_style(mui_button_t *btn, const mui_button_style_t *style);

/** @brief 移动按钮位置 */
void mui_button_set_pos(mui_button_t *btn, int16_t x, int16_t y);

/** @brief 设置锁存模式（1=开关/锁存，0=瞬态；瞬态为默认） */
void mui_button_set_latch(mui_button_t *btn, uint8_t latch);

/**
 * @brief 每帧调用：按当前状态绘制按钮（状态未变时直接返回，不写屏）
 *
 * 内部缓存上次绘制时的状态快照，完全一致则跳过重绘 —— 因此可以放心地每帧
 * 无条件调用，实际写屏量只取决于真正发生变化的按钮。
 * 所有 set_* 接口与触摸处理都会在状态变化时自动作废缓存。
 * @param btn 按钮对象（非 const：draw 会写缓存）
 */
void mui_button_draw(mui_button_t *btn);

/**
 * @brief 作废绘制缓存，强制下次 mui_button_draw 重绘
 *
 * 适用场景：
 *   - 屏幕内容被外部破坏（如整屏 mui_screen_clear）后需要恢复按钮；
 *   - 改写了 text/图标所指向的同一块缓冲区内容（指针没变，缓存感知不到）。
 * @param btn 按钮对象
 */
void mui_button_invalidate(mui_button_t *btn);

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
