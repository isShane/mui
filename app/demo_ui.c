/**
 * @file demo_ui.c
 * @brief 界面层：红光治疗设备 UI（纯 MUI API，平台无关）
 */

#include "mui.h"
#include "mui_font.h"
#include "img_logo.h"
#include "img_icon1.h"
#include "img_icon2.h"
#include "img_icon3.h"
#include "img_icon4.h"
#include "demo_ui.h"

/* -------- 界面配色 -------- */
#define UI_BG     MUI_RGB565(0xD8, 0xE0, 0xEA)  /* 浅蓝灰背景 */
#define UI_BLUE   MUI_RGB565(0x3F, 0x77, 0xB8)  /* 参数框蓝 */

/* -------- 自定义按钮样式（画之前配置好） -------- */

/* 红色胶囊形（RUN 键） */
static const mui_button_style_t style_run = {
    MUI_RED, MUI_MAROON, MUI_WHITE, MUI_BLACK,
    MUI_BTN_SHAPE_PILL, 0
};

/* 灰色直角形（STOP 键，无边框=边框同底色） */
static const mui_button_style_t style_stop = {
    MUI_DARKGREY, MUI_BLACK, MUI_WHITE, MUI_DARKGREY,
    MUI_BTN_SHAPE_RECT, 0
};

/* 绿色指定圆角半径 4（备用示例） */
static const mui_button_style_t style_soft = {
    MUI_GREEN, MUI_DARKGREEN, MUI_WHITE, MUI_BLACK,
    MUI_BTN_SHAPE_ROUND, 4
};

/**
 * @brief 无符号整数转十进制字符串（避免依赖 stdio，裸机友好）
 * @param v    输入数值
 * @param buf  输出缓冲（至少 11 字节）
 */
static void u32_to_str(uint32_t v, char *buf)
{
    char tmp[10];
    int n = 0;
    int i;

    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (v > 0 && n < 10) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (i = 0; i < n; i++) {
        buf[i] = tmp[n - 1 - i];
    }
    buf[n] = '\0';
}

/**
 * @brief 竖排绘制字符串（每字符一行）
 */
static void draw_text_vertical(int16_t x, int16_t y, const char *s,
                               uint16_t color, int16_t line_h)
{
    while (*s) {
        mui_font_draw_char(x, y, *s, color, 1);
        y = (int16_t)(y + line_h);
        s++;
    }
}

static const mui_button_style_t style_btn = {
    .bg = MUI_OLIVE, 
    .bg_press = MUI_MAROON, 
    .fg = MUI_WHITE, 
    .border = MUI_WHITE,
    .shape = MUI_BTN_SHAPE_ROUND, 
    .radius = 5
};

void demo_ui_frame(void)
{
    static uint32_t t = 0;
    static uint8_t inited = 0;

    /* ---- 静态内容：只在第一帧画（避免每帧清屏导致真机闪烁） ---- */
    if (!inited) {
        inited = 1;
        mui_clear_screen(MUI_WHITE);

        /* 顶部 logo（位图，水平居中） */
        mui_draw_bitmap((int16_t)((128 - logo.w) / 2), 4,
                        logo.w, logo.h, logo.data);
        mui_draw_hline(6, 34, 116, MUI_BLACK);

        /* 静态按钮：SET / RUN / STOP（128 屏改 2x2 网格） */
        mui_button_icon(6, 44, 56, 36, &icon1, "", 01, &style_btn);
        mui_button_icon(6, 88, 56, 36, &icon3, "", 0, &style_btn);
        mui_button_icon(66, 88, 56, 36, &icon4, "", 0, &style_btn);
    }

    /* ---- 动态内容：先擦后画（仅触碰变化区域，其余屏幕不动） ---- */
    mui_fill_rect(66, 44, 56, 36, MUI_WHITE);
    mui_button_icon(66, 44, 56, 36, &icon2, "",
                    (uint8_t)((t / 15) % 2), &style_btn);

    t++;
}
