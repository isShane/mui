/**
 * @file sim_win32.c
 * @brief MUI Win32 模拟器运行时（开发者无需修改本文件）
 *
 * 职责只有两件事：每 33ms 调一次界面层的 demo_ui_frame()，
 * 再把帧缓冲 blit 到窗口。界面代码在 app/demo_ui.c（与单片机共用）。
 * 修改 SIM_W/SIM_H（sim_config.h）可模拟不同分辨率屏幕。
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mui.h"
#include "win32_port.h"
#include "png_export.h"
#include "sim_config.h"
#include "demo_ui.h"

#define SCALE   3    /**< 窗口放大倍数（改为 2 可放大观察像素细节） */
#define FPS_MS  33   /**< 刷新间隔（毫秒） */

static unsigned char *rgb_buf = NULL;  /**< RGB888 转换缓冲 */

/**
 * @brief 当前帧导出为 PNG 文件
 * @param path 输出路径
 * @return 0 成功
 */
int win32_save_png(const char *path)
{
    int w, h;
    uint16_t *fb = win32_get_fb(&w, &h);
    if (fb == NULL) {
        return -1;
    }
    return png_export_rgb565(path, fb, w, h);
}

/**
 * @brief 把 RGB565 帧缓冲 blit 到窗口（StretchDIBits 放大 SCALE 倍）
 */
static void blit_frame(HDC hdc)
{
    int w;
    int h;
    uint16_t *fb = win32_get_fb(&w, &h);
    BITMAPINFO bi;
    int i;

    if (fb == NULL || rgb_buf == NULL) {
        return;
    }
    for (i = 0; i < w * h; i++) {
        uint16_t c = fb[i];
        rgb_buf[i * 4 + 0] = (unsigned char)(((c >> 11) & 0x1F) * 255 / 31);
        rgb_buf[i * 4 + 1] = (unsigned char)(((c >> 5) & 0x3F) * 255 / 63);
        rgb_buf[i * 4 + 2] = (unsigned char)((c & 0x1F) * 255 / 31);
    }
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  /* 负值：自顶向下 */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    StretchDIBits(hdc, 0, 0, w * SCALE, h * SCALE, 0, 0, w, h,
                  rgb_buf, &bi, DIB_RGB_COLORS, SRCCOPY);
}

/** @brief 窗口过程 */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        demo_ui_frame();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        blit_frame(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            DestroyWindow(hwnd);
        } else if (wp == 'S') {
            /* S 键：截屏当前帧到 build\screenshot.png */
            static int shot_no = 0;
            char path[64];
            snprintf(path, sizeof(path), "build\\screenshot_%d.png", shot_no++);
            if (win32_save_png(path) == 0) {
                OutputDebugStringA("screenshot saved: ");
                OutputDebugStringA(path);
                OutputDebugStringA("\n");
            }
        }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

/** @brief 程序入口：创建窗口 + 定时刷新消息循环 */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow)
{
    static const char cls_name[] = "MUI_SIM";
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    RECT rc;

    (void)hPrev;
    (void)cmd;

    /* DPI 感知：保证窗口像素 1:1，不被系统显示缩放拉伸 */
    SetProcessDPIAware();

    /* 初始化图形库（触发移植层帧缓冲分配） */
    mui_init(SIM_W, SIM_H);
    rgb_buf = (unsigned char *)malloc((size_t)SIM_W * SIM_H * 4);
    if (rgb_buf == NULL) {
        return 1;
    }

    memset(&wc, 0, sizeof(wc));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = cls_name;
    RegisterClassA(&wc);

    /* 客户区 = 屏幕尺寸 x SCALE（样式必须与创建时一致，否则边框计算不符） */
    {
        const DWORD wnd_style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
        rc.left = 0;
        rc.top = 0;
        rc.right = SIM_W * SCALE;
        rc.bottom = SIM_H * SCALE;
        AdjustWindowRect(&rc, wnd_style, FALSE);

        hwnd = CreateWindowA(cls_name, "MUI Simulator (ESC to quit)",
                             wnd_style,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top,
                             NULL, NULL, hInst, NULL);
    }
    SetTimer(hwnd, 1, FPS_MS, NULL);
    ShowWindow(hwnd, nShow);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    free(rgb_buf);
    return (int)msg.wParam;
}
