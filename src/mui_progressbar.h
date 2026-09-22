/**
 * @file mui_progressbar.h
 * @brief MUI 进度条控件（保留模式对象 · 只重画变化区间）
 *
 * 支持水平（自左向右）与垂直（自下向上）两种方向。
 * 依赖 mui.h（图形原语），平台无关。
 */

#ifndef MUI_PROGRESSBAR_H
#define MUI_PROGRESSBAR_H

#include "mui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------- 方向与配色 -------- */

/** @brief 进度条方向 */
typedef enum {
    MUI_PROGRESSBAR_HORIZONTAL = 0,  /**< 水平：自左向右填充 */
    MUI_PROGRESSBAR_VERTICAL,        /**< 垂直：自下向上填充 */
} mui_progressbar_dir_t;

/** @brief 进度条配色与形状方案（画之前配置好，绘制时传入） */
typedef struct {
    uint16_t bg;        /**< 未填充底色 */
    uint16_t fg;        /**< 已填充色 */
    uint16_t border;    /**< 边框色（与 bg 同色则视觉无边框） */
    int16_t radius;     /**< 圆角半径：<=0 自动（h/4），>0 指定（自动限 min(w,h)/2） */
    uint8_t dir;        /**< 方向（mui_progressbar_dir_t） */
} mui_progressbar_style_t;

/** @brief 默认进度条配色（浅灰空槽 + 蓝底填充 + 灰边） */
extern const mui_progressbar_style_t mui_progressbar_style_default;

/* -------- 进度条对象（保留模式 · OO 风格） -------- */

/** @brief 进度条对象：静态分配，一次 init 后 set_value 只重画变化区间 */
typedef struct {
    int16_t x;                         /**< 左上角横坐标 */
    int16_t y;                         /**< 左上角纵坐标 */
    int16_t w;                         /**< 宽度 */
    int16_t h;                         /**< 高度 */
    const mui_progressbar_style_t *style;  /**< 样式（指针，NULL 用默认） */
    uint8_t value;                     /**< 当前进度 0~100 */
    uint8_t visible;                   /**< 0=隐藏，非 0=显示 */
    uint8_t drawn;                     /**< 是否已绘制过（首次 set_value 不擦除） */
    int16_t last_fill;                 /**< 上次填充长度（主轴像素，增量刷新用） */
} mui_progressbar_t;

/**
 * @brief 初始化进度条对象（静态分配后调用一次）
 * @param pb     进度条对象指针（由调用者静态/栈上分配）
 * @param x      左上角横坐标
 * @param y      左上角纵坐标
 * @param w      宽度
 * @param h      高度
 * @param style  配色与形状（NULL 使用 mui_progressbar_style_default）
 */
void mui_progressbar_init(mui_progressbar_t *pb, int16_t x, int16_t y,
                          int16_t w, int16_t h,
                          const mui_progressbar_style_t *style);

/**
 * @brief 设置进度：只重画受影响的列/行（值未变时零写屏）
 *
 * 首次调用自动走全量绘制；之后仅擦除新旧填充的差异区间并重画，
 * 已填充且形状未变的像素不被触碰。
 * @param pb     进度条对象
 * @param value  进度 0~100（>100 按 100 处理）
 */
void mui_progressbar_set_value(mui_progressbar_t *pb, uint8_t value);

/**
 * @brief 移动进度条位置：自动擦旧画新
 * @note  擦除使用 style->bg，要求条原本压在纯 bg 底色上
 * @param pb  进度条对象
 * @param x   新左上角 x
 * @param y   新左上角 y
 */
void mui_progressbar_set_pos(mui_progressbar_t *pb, int16_t x, int16_t y);

/** @brief 设置可见性（重新显示时按全量重绘；隐藏不擦除） */
void mui_progressbar_set_visible(mui_progressbar_t *pb, uint8_t visible);

/**
 * @brief 无条件全量重绘当前进度（整屏被清后恢复用；常规更新用 set_value）
 * @param pb 进度条对象
 */
void mui_progressbar_draw(const mui_progressbar_t *pb);

#ifdef __cplusplus
}
#endif

#endif /* MUI_PROGRESSBAR_H */
