/**
 * @file mui_anim.h
 * @brief MUI 动画支撑：毫秒时间基 + 缓动函数 + 补间（tween）对象
 *
 * 纯整数实现（无浮点），平台无关，不依赖 RTOS。
 *
 * 典型用法（裸机主循环）：
 *     mui_tick_update(bsp_system_tick_ms());   // 每帧喂一次毫秒时基
 *     uint16_t dt = mui_tick_delta();          // 距上次的毫秒差（抗 32 位回绕）
 *     mui_anim_update(&a, dt);                 // 推进补间
 *     int32_t v = mui_anim_value(&a);          // 取当前动画值，用于绘制
 *
 * 只用缓动函数也可以（不建补间对象）：
 *     int16_t e = mui_ease(MUI_EASE_OUT_CUBIC, t256);   // t256: 0~256
 *
 * 说明：本头文件只提供声明，定义在 mui_gfx.c 里（与 mui_math.h 同策略：
 * 保持 src/ 顶层 .c 文件数不变，避免移植时在 Keil 工程里手工登记）。
 */

#ifndef MUI_ANIM_H
#define MUI_ANIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================= 毫秒时间基 ================= */

/**
 * @brief 喂入当前毫秒时刻（主循环每帧调一次）
 * @param now_ms 单调递增的毫秒计数（如 SysTick 毫秒），内部自动处理 32 位回绕
 */
void mui_tick_update(uint32_t now_ms);

/** @brief 最近一次喂入的时刻（毫秒） */
uint32_t mui_time_ms(void);

/**
 * @brief 距上次 mui_tick_update 的毫秒差
 * @return 毫秒差；首次调用为"距 0 的差"；超过 65535 时截断为 65535
 */
uint16_t mui_tick_delta(void);

/* ================= 缓动函数 ================= */

/** @brief 缓动曲线 */
typedef enum {
    MUI_EASE_LINEAR = 0,   /**< 线性：匀速 */
    MUI_EASE_IN_QUAD,      /**< 二次缓入：慢起 */
    MUI_EASE_OUT_QUAD,     /**< 二次缓出：慢停 */
    MUI_EASE_IN_OUT_QUAD,  /**< 二次缓入缓出 */
    MUI_EASE_IN_CUBIC,     /**< 三次缓入：更慢起 */
    MUI_EASE_OUT_CUBIC,    /**< 三次缓出：更慢停 */
    MUI_EASE_IN_OUT_CUBIC, /**< 三次缓入缓出 */
    MUI_EASE_OUT_BACK,     /**< 缓出回弹：终点前过冲再回落 */
} mui_ease_t;

/**
 * @brief 缓动映射：把线性进度映射为缓动后的进度
 * @param type 缓动曲线（mui_ease_t）
 * @param t256 线性进度，0~256（256 = 1.0；越界自动钳制到 0~256）
 * @return 缓动后进度，8.8 定点：常规曲线为 0~256；
 *         MUI_EASE_OUT_BACK 会略超 256（过冲），这是回弹效果的来源
 */
int16_t mui_ease(uint8_t type, int16_t t256);

/* ================= 补间（tween） ================= */

/** @brief 补间对象：把 from→to 在 dur 毫秒内按缓动曲线推进 */
typedef struct {
    int32_t  from;      /**< 起点值 */
    int32_t  to;        /**< 终点值 */
    int32_t  value;     /**< 当前值（缓动后） */
    uint16_t dur;       /**< 时长（毫秒；0 = 立即到位） */
    uint16_t t;         /**< 已过时间（毫秒） */
    uint8_t  ease;      /**< 缓动曲线（mui_ease_t） */
    uint8_t  done;      /**< 1 = 已完成 */
} mui_anim_t;

/**
 * @brief 初始化补间：立即停在 from，并设定缓动曲线
 * @param a    补间对象
 * @param from 起始值
 * @param ease 缓动曲线（mui_ease_t）
 */
void mui_anim_init(mui_anim_t *a, int32_t from, uint8_t ease);

/**
 * @brief 从 from 到 to 重新开始一段动画（复用已设定的缓动曲线）
 * @param dur_ms 时长（毫秒；0 = 立即到达 to）
 */
void mui_anim_start(mui_anim_t *a, int32_t from, int32_t to, uint16_t dur_ms);

/** @brief 从当前值出发奔向 to（动画途中改目标用） */
void mui_anim_to(mui_anim_t *a, int32_t to, uint16_t dur_ms);

/** @brief 立即停在当前值（取消动画） */
void mui_anim_stop(mui_anim_t *a);

/** @brief 直接跳到指定值（不做动画） */
void mui_anim_jump(mui_anim_t *a, int32_t value);

/** @brief 推进 dt 毫秒（dt 通常取自 mui_tick_delta()；完成的补间直接返回） */
void mui_anim_update(mui_anim_t *a, uint16_t dt_ms);

/** @brief 当前动画值 */
int32_t mui_anim_value(const mui_anim_t *a);

/** @brief 是否已完成（未初始化/已停止时返回 1） */
uint8_t mui_anim_done(const mui_anim_t *a);

#ifdef __cplusplus
}
#endif

#endif /* MUI_ANIM_H */
