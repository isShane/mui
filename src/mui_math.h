/**
 * @file mui_math.h
 * @brief MUI 库内整数数学工具（供库内各模块复用）
 *
 * 这里放的是"库内公共"而不是"对外 API"的东西：图元、控件、抗锯齿都要用，
 * 但不希望用户直接调用（用户面向的是 mui.h）。
 *
 * 说明：本头文件目前**只提供声明**，定义还在 mui_gfx.c 里（保持 src/ 顶层文件数不变，
 * 避免新增 .c 后需要在 Keil 工程里手工登记）。将来若要独立成 mui_math.c，
 * 把两个函数的定义搬过去即可，使用方无需改动。
 */

#ifndef MUI_MATH_H
#define MUI_MATH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 整数平方根（逐位法：只用移位/比较/减法，无除法、无浮点）
 * @param n 输入值（n == 0 时返回 0）
 * @return floor(sqrt(n))
 */
uint32_t mui_isqrt(uint32_t n);

/**
 * @brief sqrt(n) 的 8.8 定点值（即 floor(sqrt(n) * 256)）
 * @param n 输入值
 * @return 8.8 定点平方根；n >= 65536 时自动降为 4 位小数以避开 n << 16 溢出
 * @note  固定点开方是抗锯齿的关键：小数部分即"边界像素的覆盖率"
 */
uint32_t mui_sqrt_fp8(uint32_t n);

/**
 * @brief 圆角在"距角圆心 dy 像素"处的半跨度（抗锯齿边界专用）
 * @param dy  到角圆心的距离（像素，内部取绝对值）
 * @param r   圆角半径
 * @param dxf 输出：满覆盖半跨度 = floor(sqrt(r²-dy²))（传 NULL 可忽略）
 * @param dxp 输出：可能仍有覆盖的半跨度（含 r+0.5 过渡带，传 NULL 可忽略）
 * @note  抗锯齿圆角矩形的"四角逐行填充"与进度条填充的角带都调它，
 *        保证两条绘制路径的角部跨度**永远一致**（否则会出现"填充比底板窄一圈"）。
 */
void mui_corner_span(int16_t dy, int16_t r, int16_t *dxf, int16_t *dxp);

/**
 * @brief 角部像素对圆角的覆盖率
 * @param ux 像素中心相对角圆心的横向距离（**半像素单位**，即 2*(px-cx)）
 * @param uy 同上，纵向
 * @param r  圆角半径（像素）
 * @return 0~255：dist <= r 时 255，dist >= r+0.5 时 0，中间线性过渡
 */
uint8_t mui_corner_alpha(int16_t ux, int16_t uy, int16_t r);

#ifdef __cplusplus
}
#endif

#endif /* MUI_MATH_H */
