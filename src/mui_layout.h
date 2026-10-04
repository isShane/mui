/**
 * @file mui_layout.h
 * @brief MUI 容器与布局：裁剪容器（可滚动）+ 行/列/网格布局游标
 *
 * 容器基于裁剪区实现：begin() 把裁剪区收窄到容器矩形（与父裁剪求交，支持嵌套），
 * end() 恢复。子控件照常绘制，超出容器的部分自动被裁掉；配合 scroll 偏移即可
 * 做"内容大于视口"的滚动列表。
 *
 * 布局游标负责"把一堆控件摆进一个矩形"：按行/列顺序取格，或按权重等分。
 * 两者都不持有子控件，纯几何计算，因此可以按需自由组合。
 *
 * 用法：
 *     mui_container_begin(&box);
 *     mui_layout_t l;
 *     mui_layout_init(&l, box.x, box.y - box.sy, box.w, box.h, MUI_LAYOUT_COL, 6);
 *     mui_rect_t r = mui_layout_next(&l, 0, 24);   // 宽度 0 = 占满整行
 *     mui_button_init(&b, r.x, r.y, r.w, r.h, NULL, "OK", NULL);
 *     mui_container_end(&box);
 */

#ifndef MUI_LAYOUT_H
#define MUI_LAYOUT_H

#include "mui.h"
#include "mui_font.h"   /* mui_align_t / mui_font_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ======================= 裁剪容器 ======================= */

/** @brief 容器：把子内容限制在一个矩形内绘制，可带滚动偏移 */
typedef struct {
    int16_t x, y, w, h;   /**< 容器矩形（屏幕坐标，视口） */
    int16_t sx, sy;       /**< 滚动偏移（内容绘制原点 = x - sx / y - sy） */
    int16_t cw, ch;       /**< 内容尺寸（用于钳制滚动；<=0 视作与视口相同=不可滚） */
    mui_rect_t clip;      /**< begin 时保存的父裁剪区 */
    uint8_t open;         /**< begin/end 配对状态 */
} mui_container_t;

/** @brief 初始化容器（位置/尺寸；滚动与内容尺寸清零） */
void mui_container_init(mui_container_t *c, int16_t x, int16_t y,
                        int16_t w, int16_t h);

/**
 * @brief 进入容器：把裁剪区收窄为"容器矩形 ∩ 当前裁剪区"（支持嵌套）
 * @note  必须与 mui_container_end 成对调用。
 */
void mui_container_begin(mui_container_t *c);

/** @brief 退出容器：恢复进入前的裁剪区 */
void mui_container_end(mui_container_t *c);

/** @brief 内容绘制原点 x（= 容器 x - 滚动偏移 sx） */
int16_t mui_container_ox(const mui_container_t *c);

/** @brief 内容绘制原点 y（= 容器 y - 滚动偏移 sy） */
int16_t mui_container_oy(const mui_container_t *c);

/** @brief 设置内容尺寸（决定可滚动范围；<=0 表示不需要滚动） */
void mui_container_set_content(mui_container_t *c, int16_t cw, int16_t ch);

/** @brief 设置滚动偏移（自动钳制到 [0, 内容-视口]） */
void mui_container_set_scroll(mui_container_t *c, int16_t sx, int16_t sy);

/** @brief 相对滚动（自动钳制） */
void mui_container_scroll_by(mui_container_t *c, int16_t dx, int16_t dy);

/** @brief 水平最大滚动量（内容不超出时返回 0） */
int16_t mui_container_scroll_max_x(const mui_container_t *c);

/** @brief 垂直最大滚动量（内容不超出时返回 0） */
int16_t mui_container_scroll_max_y(const mui_container_t *c);

/* ======================= 布局游标 ======================= */

/** @brief 布局方向（也用于对齐枚举复用：见 mui_rect_align） */
typedef enum {
    MUI_LAYOUT_ROW = 0,   /**< 行：自左向右，摆不下自动换行 */
    MUI_LAYOUT_COL,       /**< 列：自上向下 */
} mui_layout_dir_t;

/** @brief 布局游标 */
typedef struct {
    int16_t x, y, w, h;   /**< 布局区域 */
    int16_t cx, cy;       /**< 当前游标位置 */
    int16_t line_h;       /**< 当前行/列已占用的短边尺寸 */
    int16_t gap;          /**< 项间距 */
    uint8_t dir;          /**< mui_layout_dir_t */
} mui_layout_t;

/**
 * @brief 初始化布局游标
 * @param dir MUI_LAYOUT_ROW（横向）/ MUI_LAYOUT_COL（纵向）
 * @param gap 项间距（像素）
 */
void mui_layout_init(mui_layout_t *l, int16_t x, int16_t y, int16_t w, int16_t h,
                     uint8_t dir, int16_t gap);

/**
 * @brief 取下一格
 * @param w 宽度；<=0 表示占满主轴剩余空间（整行/整列）
 * @param h 高度；<=0 表示取该方向剩余高度
 * @return 格子矩形；放不下时返回 w/h 为 0 的空矩形
 */
mui_rect_t mui_layout_next(mui_layout_t *l, int16_t w, int16_t h);

/**
 * @brief 收尾：返回游标下方（行布局）或右方（列布局）尚未使用的剩余区域
 * @note  配合 mui_layout_next 可以在一个区域里"上若干控件 + 下方填满"
 */
mui_rect_t mui_layout_rest(const mui_layout_t *l);

/**
 * @brief 把区域按列等分，取第 index 列
 * @param gap 列间距
 */
mui_rect_t mui_layout_cols(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint8_t total, uint8_t index, int16_t gap);

/**
 * @brief 把区域按行等分，取第 index 行
 * @param gap 行间距
 */
mui_rect_t mui_layout_rows(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint8_t total, uint8_t index, int16_t gap);

/**
 * @brief 网格单元矩形
 * @param rows,cols 网格行列数
 * @param row,col   目标单元下标
 * @param gap       单元格间距
 */
mui_rect_t mui_layout_grid(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint8_t rows, uint8_t cols,
                           uint8_t row, uint8_t col, int16_t gap);

/* ======================= 矩形工具 ======================= */

/** @brief 矩形内缩 pad 像素（pad<0 则外扩；宽高可能被缩成 <=0） */
mui_rect_t mui_rect_pad(mui_rect_t r, int16_t pad);

/**
 * @brief 在区域内按对齐方式放置一个 w×h 的子矩形
 * @param align 水平对齐：MUI_ALIGN_LEFT / MUI_ALIGN_CENTER / MUI_ALIGN_RIGHT（垂直居中）
 */
mui_rect_t mui_rect_align(mui_rect_t area, int16_t w, int16_t h, uint8_t align);

/** @brief 点是否在矩形内（开区间，与 mui_rect_contains 同义，便于配合 mui_rect_t） */
uint8_t mui_rect_hit(mui_rect_t r, int16_t px, int16_t py);

#ifdef __cplusplus
}
#endif

#endif /* MUI_LAYOUT_H */
