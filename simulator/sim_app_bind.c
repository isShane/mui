/**
 * @file sim_app_bind.c
 * @brief 模拟器装配入口：把界面层（app/）接入通用宿主
 *
 * 这是模拟器与界面之间唯一的连接点：更换或新增界面只需替换本文件，
 * 宿主 sim_win32.c 与界面 demo_ui.c 互不感知。
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "sim_host.h"
#include "demo_ui.h"

static const sim_app_ops_t s_app_ops = {
    demo_ui_frame,
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