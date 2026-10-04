/**
 * @file demo_shot_main.c
 * @brief demo 无头截图入口：不开窗口，把 demo 界面渲染成 PNG
 *
 * 用途：在没有 GUI（或不想开窗口）的环境下核对 demo 的布局与配色。
 * 与窗口版 demo 共用同一份 demo_ui.c，因此看到的画面完全一致。
 *
 * 产物：当前目录下 demo_shot.png
 * 运行：build\mui_demo_shot.exe
 */

#include <stdio.h>
#include "mui.h"
#include "demo_ui.h"
#include "sim_helper.h"

/** @brief 产物文件名：条带后端目标通过它另存，便于与直绘截图对比 */
#ifndef MUI_DEMO_SHOT_NAME
#define MUI_DEMO_SHOT_NAME  "demo_shot.png"
#endif

int main(void)
{
    mui_init(SIM_W, SIM_H);
    demo_ui_init();           /* 首屏（内部走 mui_screen_frame，条带后端自动按带重放） */
    demo_ui_frame();          /* 再走一帧动态逻辑（时钟 / 仪表盘动画） */
    mui_screen_flush();

    sim_export_png(MUI_DEMO_SHOT_NAME);
    printf("demo shot exported: %s (SIM %dx%d, mui %dx%d, violations %d)\n",
           MUI_DEMO_SHOT_NAME,
           SIM_W, SIM_H, mui_screen_get_width(), mui_screen_get_height(),
           sim_violations());
    return 0;
}
