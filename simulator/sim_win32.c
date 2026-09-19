/**
 * @file sim_win32.c
 * @brief MUI Win32 模拟器宿主（开发者无需修改本文件）
 *
 * 职责只有两件事：每 33ms 调一次界面层注册的 frame 回调，
 * 再把帧缓冲 blit 到窗口。本文件不感知任何具体界面，
 * 界面经 sim_host.h 注入（见装配文件 sim_app_bind.c）。
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
#include "sim_host.h"

#define SCALE   2   /**< 窗口放大倍数（改为 2 可放大观察像素细节） */
#define FPS_MS  33   /**< 刷新间隔（毫秒） */

static unsigned char *rgb_buf = NULL;           /**< RGB888 转换缓冲 */
static const sim_app_ops_t *s_app_ops = NULL;   /**< 注入的界面回调 */

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
        /* Windows DIB 32bpp 为 BGRA 顺序 */
        rgb_buf[i * 4 + 0] = (unsigned char)((c & 0x1F) * 255 / 31);          /* B */
        rgb_buf[i * 4 + 1] = (unsigned char)(((c >> 5) & 0x3F) * 255 / 63);   /* G */
        rgb_buf[i * 4 + 2] = (unsigned char)(((c >> 11) & 0x1F) * 255 / 31);  /* R */
        rgb_buf[i * 4 + 3] = 255;                                              /* A = 不透明 */
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

/**
 * @brief 把物理按键转发给界面层（未注入 key 回调则忽略）
 * @param key_id 键号（0~3 = 底部按钮，4 = Mode 键）
 * @param down   1 = 按下，0 = 抬起
 */
static void sim_key_dispatch(uint8_t key_id, uint8_t down)
{
    if (s_app_ops != NULL && s_app_ops->key != NULL) {
        s_app_ops->key(key_id, down);
    }
}

/** @brief 窗口过程 */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        if (s_app_ops != NULL && s_app_ops->frame != NULL) {
            s_app_ops->frame();
        }
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
        } else if (wp >= '1' && wp <= '4') {
            /* 物理键 1~4 = 底部四个按钮（忽略按住不放的自动重复） */
            if (!(lp & (1u << 30))) {
                sim_key_dispatch((uint8_t)(wp - '1'), 1);
            }
        } else if (wp == 'M' || wp == 'm') {
            if (!(lp & (1u << 30))) {
                sim_key_dispatch(4, 1);
            }
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
    case WM_LBUTTONDOWN:
        /* 鼠标左键 = 触摸按下（捕获保证拖出窗口也能收到抬起） */
        SetCapture(hwnd);
        mui_touch_update((int16_t)((short)LOWORD(lp) / SCALE),
                         (int16_t)((short)HIWORD(lp) / SCALE), 1);
        return 0;
    case WM_MOUSEMOVE:
        /* 按住状态下的移动 = 触摸拖动 */
        if (wp & MK_LBUTTON) {
            mui_touch_update((int16_t)((short)LOWORD(lp) / SCALE),
                             (int16_t)((short)HIWORD(lp) / SCALE), 1);
        }
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        mui_touch_update((int16_t)((short)LOWORD(lp) / SCALE),
                         (int16_t)((short)HIWORD(lp) / SCALE), 0);
        return 0;
    case WM_KEYUP:
        if (wp >= '1' && wp <= '4') {
            sim_key_dispatch((uint8_t)(wp - '1'), 0);
        } else if (wp == 'M' || wp == 'm') {
            sim_key_dispatch(4, 0);
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

/**
 * @brief 启动模拟器宿主：创建窗口 + 定时刷新消息循环
 * @param ops 界面回调集合
 * @return 进程退出码
 */
int sim_host_run(const sim_app_ops_t *ops)
{
    static const char cls_name[] = "MUI_SIM";
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    RECT rc;
    HINSTANCE hInst = GetModuleHandleA(NULL);

    s_app_ops = ops;

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
    ShowWindow(hwnd, SW_SHOWDEFAULT);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    free(rgb_buf);
    return (int)msg.wParam;
}
