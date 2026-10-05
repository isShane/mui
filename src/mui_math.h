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

/**
 * @brief 整数转字符串（可选单字符后缀，如 '%'），不用 stdio
 * @param buf    输出缓冲，容量必须 >= 8（"-32768" + 后缀 + NUL）
 * @param v      要格式化的值
 * @param suffix 追加字符；'\0' = 不追加
 * @return 写入长度（不含结尾 NUL）
 * @note  控件里显示数值（滑块 / 表盘）共用这一份，避免把 printf 链进固件
 */
int16_t mui_num16_fmt(char *buf, int16_t v, char suffix);

/**
 * @brief 抗锯齿落笔（库内公共）：按覆盖率 a 把 fg 混到"底色"上
 * @param x,y 像素坐标（调用方已裁剪到屏幕内）
 * @param fg  前景色
 * @param bg  底色兜底值（直绘后端 / 无图案回调 / 不能回读时使用）
 * @param a   覆盖率 0~255（0 = 不落笔，255 = 纯 fg）
 * @note  底色取值优先级：① mui_aa_base_set() 注册的图案取样回调；② 缓冲/
 *        条带后端回读当前屏上像素；③ 参数 bg。
 *        ② 仅在"读到的像素确实还是底色"时成立：整帧重绘成立；对同一形状
 *        反复增量重绘会累积加墨 ⇒ 那种场景请注册 ① 或传准 bg。
 */
void mui_pixel_draw_aa(int16_t x, int16_t y, uint16_t fg, uint16_t bg, uint8_t a);

/**
 * @brief 取抗锯齿底色（不回读版）：① 图案取样回调 → ② fallback
 * @note  供控件边界像素使用：控件增量重绘时不能回读（会把上一帧自己的墨迹
 *        当成底色、越描越实），但注册了图案回调时仍能取到真实底色。
 */
uint16_t mui_aa_base_at(int16_t x, int16_t y, uint16_t fallback);

/**
 * @brief 取抗锯齿底色（完整版）：① 图案回调 → ② 回读屏幕 → ③ fallback
 * @note  供"逐像素决定底色"的绘制路径（文字、图元边界）使用。
 */
uint16_t mui_aa_base_get(int16_t x, int16_t y, uint16_t fallback);

/** @brief mui_aa_base_mode() 的返回值 */
#define MUI_AA_BASE_UNIFORM  0   /**< 底色恒为传入的 bg（直绘后端且未注册图案回调） */
#define MUI_AA_BASE_PATTERN  1   /**< 注册了图案取样回调：底色必须逐像素解析 */
#define MUI_AA_BASE_SCREEN   2   /**< 无回调但能回读：底色由屏上内容决定 */

/**
 * @brief 查询当前底色模式（见上面三个宏）
 * @return MUI_AA_BASE_UNIFORM / MUI_AA_BASE_PATTERN / MUI_AA_BASE_SCREEN
 * @note  模式为 UNIFORM 时，调用方可以完全按"底色 = bg"绘制，零额外开销；
 *        其余模式需要逐像素调 mui_aa_base_get() 取真实底色。
 */
uint8_t mui_aa_base_mode(void);

#ifdef __cplusplus
}
#endif

#endif /* MUI_MATH_H */
