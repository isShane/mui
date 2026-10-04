#ifndef __DEMO_UI_H__
#define __DEMO_UI_H__

#include <stdint.h>

/* MUI 控制面板 demo（480x480，深色主题）：环境/设备控制面板
 * 与 app_ui.c 同构的三层接口：模拟器宿主与单片机主循环都这样调：
 *   demo_ui_init();             - 上电调一次
 *   while (1) demo_ui_frame();  - 周期调用刷新
 */

/** @brief 初始化图形库并完成首屏静态绘制（启动时调用一次） */
void demo_ui_init(void);

/** @brief 周期刷新：消费触摸事件、推进动画（放在主循环里） */
void demo_ui_frame(void);

/** @brief 物理按键回调（模拟器宿主 / 单片机按键扫描调用，按下沿动作） */
void demo_ui_key(uint8_t key_id, uint8_t down);

#endif /* __DEMO_UI_H__ */
