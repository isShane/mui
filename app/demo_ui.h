/**
 * @file demo_ui.h
 * @brief 界面层：红光治疗设备 UI（模拟器与单片机共用）
 *
 * 本文件及 demo_ui.c 只依赖 MUI 图形 API（src/mui.h）和点阵字体
 * （app/mui_font.h），不含任何平台相关代码——
 * 在 PC 模拟器和单片机上的显示效果完全一致。
 *
 * 单片机用法：定时（如每 33ms）调用一次 demo_ui_frame() 即可，
 * 见项目根目录 README 的移植说明。
 */

#ifndef DEMO_UI_H
#define DEMO_UI_H

/**
 * @brief 绘制一帧界面（可在单片机主循环 / 定时器调度中周期调用）
 * @note  内部维护帧计数，每 30 次调用计数 +1（按 30fps 即每秒 +1）
 */
void demo_ui_frame(void);

/**
 * @brief 物理按键输入：映射到屏幕按钮并合成触摸事件
 *
 * 按下 = 合成一次"点击该按钮中心"的触摸（按下高亮/点击动作与触摸完全一致）。
 * 单片机用法：按键扫描得到 (键值, 按下/抬起) 后直接调用本函数。
 * @param key_id  0~3 = 底部四个按钮，4 = Mode 键
 * @param down    1 = 按下，0 = 抬起
 */
void demo_ui_key(uint8_t key_id, uint8_t down);

#endif /* DEMO_UI_H */
