#ifndef __APP_UI_H__
#define __APP_UI_H__

#include <stdint.h>

/* =========================================================================
 * 应用界面（MUI demo，128x128 屏适配版）
 *
 * 界面代码只调用 MUI 公共 API（middleware/mui/src/mui.h）+ 工程内资源，
 * 不含任何平台代码；在 PC 模拟器中同一份代码可直接预览。
 * ======================================================================= */

/**
 * @brief 初始化 MUI 图形库与 LCD（内部完成屏初始化），应用启动时调用一次
 */
void app_ui_init(void);

/**
 * @brief 界面周期刷新：消费输入事件，并按当前页刷新动态内容
 * @note  第 1 页时间按秒更新、第 3 页进度条每 50ms 更新，
 *        故应在 30fps 左右的主循环里调用，不要放到秒级任务里
 */
void app_ui_frame(void);

/**
 * @brief 按对象当前状态重画底部四键（公共区）
 * @note 只画底部四键；Mode 按钮属于第 1 页的中部内容，由页面绘制负责
 */
void app_ui_draw_buttons(void);

/**
 * @brief 物理按键回调：由平台层（单片机 bsp_key / 模拟器宿主）调用
 * @param key_id 键号：0~3 = 底部四键，4 = Mode 键
 * @param down   1 = 按下，0 = 抬起
 * @note  只在按下沿触发，长按不重复；"键号 → 功能"的对应关系由
 *        app_ui.c 里的 s_key_fn 表集中配置（默认只有 0 号键循环切页）。
 *        鼠标/触摸屏走库的触摸链路，与本回调互不影响。
 */
void app_ui_key(uint8_t key_id, uint8_t down);

#endif /* __APP_UI_H__ */
