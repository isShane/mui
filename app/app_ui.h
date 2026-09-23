#ifndef __APP_UI_H__
#define __APP_UI_H__

#include <stdint.h>

/* 应用界面（MUI demo，128x128）：只调 MUI 公共 API + 工程内资源，
 * PC 模拟器与单片机共用同一份代码 */

/** @brief 初始化图形库与 LCD（内部完成屏初始化），启动时调用一次 */
void app_ui_init(void);

/** @brief 周期刷新：消费输入事件并按页刷新动态内容（放在 30fps 左右的主循环里） */
void app_ui_frame(void);

/** @brief 按对象状态重画底部四键（公共区；Mode 按钮属于第 1 页内容） */
void app_ui_draw_buttons(void);

/**
 * @brief 物理按键回调（由平台层 bsp_key / 模拟器宿主调用）
 * @param key_id 键号：0~3 = 底部四键，4 = Mode 键
 * @param down   1 = 按下，0 = 抬起（只在按下沿动作）
 * @note  键号 → 功能见 app_ui.c 的 s_key_fn[]，换键位只改那张表
 */
void app_ui_key(uint8_t key_id, uint8_t down);

#endif /* __APP_UI_H__ */
