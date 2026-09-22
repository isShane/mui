/**
 * @file sim_app_bind.c
 * @brief 模拟器装配入口：把界面层（app/）接入通用宿主
 *
 * 这是模拟器与界面之间唯一的连接点：更换或新增界面只需替换本文件，
 * 宿主 sim_win32.c 与具体界面互不感知。
 *
 * 当前绑定的是单片机工程的 app/app_ui.c —— 该文件原样复用、未作任何修改，
 * 它依赖的 BSP_LCD_WIDTH/HEIGHT 与 bsp_system_tick_ms() 由
 * simulator/bsp_lcd.h、simulator/bsp_system.h 两个兼容桩吸收，
 * 因此模拟器所见即单片机所显示。
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "sim_host.h"
#include "app_ui.h"

static uint8_t s_inited = 0;

/**
 * @brief 界面帧回调：首帧做一次初始化，之后周期刷新
 * @note  初始化必须发生在宿主 mui_init() 之后，否则帧缓冲重建会擦掉画面
 */
static void sim_frame(void)
{
    if (!s_inited) {
        s_inited = 1;
        app_ui_init();      /* 内部完成全部静态绘制，并调用一次 app_ui_frame */
        return;
    }
    app_ui_frame();
}

static const sim_app_ops_t s_app_ops = {
    sim_frame,
    app_ui_key,             /* 物理按键 → 界面（按键合成触摸，与鼠标同一链路） */
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