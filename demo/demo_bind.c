/**
 * @file demo_bind.c
 * @brief demo 装配入口：把 demo/demo_ui.c 接入模拟器通用宿主
 *
 * 与 simulator/sim_app_bind.c 同构（那份绑的是单片机工程 app/app_ui.c）。
 * demo 单独一个装配文件，是为了让 mui_sim（正式界面）与 mui_demo（入门示例）
 * 两个目标互不干扰。
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "sim_host.h"
#include "demo_ui.h"

static uint8_t s_inited = 0;

/**
 * @brief 界面帧回调：首帧做一次初始化，之后周期刷新
 * @note  初始化必须发生在宿主 mui_init() 之后，否则帧缓冲重建会擦掉画面
 */
static void sim_frame(void)
{
    if (!s_inited) {
        s_inited = 1;
        demo_ui_init();
        return;
    }
    demo_ui_frame();
}

static const sim_app_ops_t s_app_ops = {
    sim_frame,
    demo_ui_key,
};

/**
 * @brief 程序入口：注册界面回调后交给通用宿主
 */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow)
{
    (void)hInst;
    (void)hPrev;
    (void)cmd;
    (void)nShow;
    return sim_host_run(&s_app_ops);
}
