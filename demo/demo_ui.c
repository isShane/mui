/**
 * @file demo_ui.c
 * @brief MUI 控件总览 demo（1024x600，深色主题）—— 库的"效果看板"
 *
 * 定位：每改一次库，跑一次这个 demo 就能一眼看到全部控件。
 * 布局：顶部状态栏 + 5 列 x 2 行 = 10 个面板，每块演示一类能力（坐标为 (列,行)）：
 *   第 0 行：(0,0) Primitives  (1,0) Anti-aliasing  (2,0) Ring/Arc  (3,0) Text&Align  (4,0) Button/Image
 *   第 1 行：(0,1) Progress    (1,1) Toggle/Check   (2,1) Gauge/List (3,1) Dropdown    (4,1) Hints
 *
 * 动态：实时时钟每秒走字；仪表盘用 mui_anim 缓动来回扫。
 * 交互：滑块拖拽、开关/复选/单选切换、按钮锁存、列表滚动选中、下拉展开选择、模态弹窗。
 *
 * 分辨率在 simulator/sim_config.h 改（SIM_W/SIM_H）；面板栅格按屏幕尺寸自动算。
 */

#include "demo_ui.h"
#include "mui.h"
#include "mui_anim.h"
#include "mui_button.h"
#include "mui_label.h"
#include "mui_progressbar.h"
#include "mui_slider.h"
#include "mui_toggle.h"
#include "mui_gauge.h"
#include "mui_list.h"
#include "mui_dropdown.h"
#include "mui_popup.h"
#include "mui_font.h"
#include "bsp_lcd.h"
#include "bsp_system.h"
#include "mui_font_harmony_os_10.h"
#include "mui_font_harmony_os_black_14.h"
#include "mui_font_harmony_os_light_20.h"
#include "img_g12x12_1.h"
#include "img_g12x12_2.h"
#include "img_logo.h"
#include <string.h>
#include <stdio.h>

/* -------- 主题（深色） -------- */
#define PANEL_BG  MUI_RGB565(0x12, 0x18, 0x22)   /* 屏幕底色 */
#define HDR_BG    MUI_RGB565(0x1A, 0x22, 0x30)   /* 顶栏 */
#define CARD_BG   MUI_RGB565(0x1E, 0x27, 0x35)   /* 面板卡片 */
#define TRACK     MUI_RGB565(0x33, 0x3D, 0x4E)   /* 轨道 / 次要底色 */
#define ACCENT    MUI_RGB565(0x04, 0xAA, 0xF4)   /* 主题蓝 */
#define ACCENT2   MUI_RGB565(0x2E, 0xCC, 0x71)   /* 绿 */
#define WARN      MUI_RGB565(0xF5, 0xA6, 0x23)   /* 橙 */
#define FG        MUI_WHITE
#define FG_DIM    MUI_RGB565(0x8A, 0x93, 0xA6)

#define I16(v)   ((int16_t)(v))                  /* 坐标统一转换，省掉满屏 cast */

/* -------- 栅格几何（按屏幕自动算：5 列 x 2 行） -------- */
#define HDR_H    32
#define MARGIN   8
#define GAP      8
#define COLS     5
#define PANEL_W  I16((BSP_LCD_WIDTH  - 2 * MARGIN - (COLS - 1) * GAP) / COLS)
#define PANEL_H  I16((BSP_LCD_HEIGHT - HDR_H - MARGIN - GAP) / 2)

/* -------- 样式 -------- */
static const mui_button_style_t s_btn_primary = {
    .bg = ACCENT, .bg_press = MUI_RGB565(0x02, 0x74, 0xB8), .fg = MUI_WHITE,
    .border = ACCENT, .shape = MUI_BUTTON_SHAPE_ROUND, .radius = 6,
    .aa = 1, .screen_bg = CARD_BG,
};
static const mui_button_style_t s_btn_ok = {
    .bg = ACCENT2, .bg_press = MUI_RGB565(0x1E, 0x8E, 0x4E), .fg = MUI_WHITE,
    .border = ACCENT2, .shape = MUI_BUTTON_SHAPE_PILL, .radius = 0,
    .aa = 1, .screen_bg = CARD_BG,
};
static const mui_button_style_t s_btn_ghost = {
    .bg = TRACK, .bg_press = ACCENT, .fg = FG,
    .border = TRACK, .shape = MUI_BUTTON_SHAPE_ROUND, .radius = 6,
    .aa = 1, .screen_bg = CARD_BG,
};
static const mui_progressbar_style_t s_pb_h = {
    .bg = TRACK, .fg = ACCENT, .border = TRACK, .radius = 7,
    .dir = MUI_PROGRESSBAR_HORIZONTAL, .aa = 1, .screen_bg = CARD_BG,
};
static const mui_progressbar_style_t s_pb_v = {
    .bg = TRACK, .fg = ACCENT2, .border = TRACK, .radius = 6,
    .dir = MUI_PROGRESSBAR_VERTICAL, .aa = 1, .screen_bg = CARD_BG,
};
static const mui_slider_style_t s_slider_h = {
    .track = TRACK, .fill = ACCENT, .knob = FG, .knob_border = ACCENT,
    .screen_bg = CARD_BG, .thickness = 10, .knob_r = 13,
    .dir = MUI_SLIDER_HORIZONTAL, .aa = 1,
};
static const mui_slider_style_t s_slider_v = {
    .track = TRACK, .fill = ACCENT2, .knob = FG, .knob_border = ACCENT2,
    .screen_bg = CARD_BG, .thickness = 10, .knob_r = 13,
    .dir = MUI_SLIDER_VERTICAL, .aa = 1,
};
static const mui_switch_style_t s_switch = {
    .off = TRACK, .on = ACCENT, .knob = FG, .knob_border = FG,
    .text = FG, .screen_bg = CARD_BG, .aa = 1, .gap = 8,
};
static const mui_checkbox_style_t s_check = {
    .bg = CARD_BG, .on = ACCENT, .border = FG_DIM, .mark = FG,
    .text = FG, .screen_bg = CARD_BG, .size = 0, .gap = 8, .aa = 1,
};
static const mui_radio_style_t s_radio = {
    .ring = FG_DIM, .on = ACCENT, .inner = CARD_BG, .text = FG,
    .screen_bg = CARD_BG, .size = 0, .gap = 8, .aa = 1,
};
static const mui_gauge_style_t s_gauge_style = {
    .track = TRACK, .fill = ACCENT, .text = FG, .bg = CARD_BG,
    .thick = 15, .a0 = 135, .a1 = 405, .aa = 1,
    .show_value = 1, .percent = 1,
};

/* -------- 控件对象 -------- */
static mui_button_t     s_btn_normal, s_btn_latch, s_btn_disabled, s_btn_icon;
static mui_button_t     s_btn_tex, s_btn_popup;
static mui_progressbar_t s_prog1, s_prog2, s_prog_v;
static mui_slider_t     s_slider, s_vslider;
static mui_switch_t     s_sw[2];
static mui_checkbox_t   s_cb[2];
static mui_radio_t      s_rd[3], *s_rd_grp[4];
static mui_gauge_t      s_gauge;
static mui_list_t       s_list;
static mui_dropdown_t   s_dd;
static mui_popup_t      s_pp;
static mui_label_t      s_clock;

static mui_anim_t  s_gauge_anim;
static uint32_t    s_clock_ms;
static char        s_clock_buf[6] = { 0 };
static uint16_t    s_tex_data[150 * 30];
static mui_image_t s_tex_img;
static const char *const s_dd_items[4] = { "Auto", "Manual", "Timer", "Off" };

/* -------- 模式差异：帧内"即时绘制" --------
 * 条带后端（MUI_OUTPUT_STRIP）要求所有写屏都发生在 mui_screen_frame 的绘制
 * 回调里（帧外写屏会写进条带缓冲，被下一次推带带出去 → 花屏），因此这里的
 * "改状态后立刻重画一下"在条带模式下必须变成空操作：每帧结尾的整屏重绘会覆盖。
 */
#if MUI_CFG_IS_STRIP
#define UI_PAINT(stmt)      do { } while (0)
#else
#define UI_PAINT(stmt)      do { stmt; } while (0)
#endif

/* -------- 几何小工具 -------- */
static int16_t ui_col(uint8_t c) { return I16(MARGIN + c * (PANEL_W + GAP)); }
static int16_t ui_row(uint8_t r) { return I16(HDR_H + r * (PANEL_H + GAP)); }

/** @brief 面板内容区矩形（不绘制） */
static mui_rect_t ui_body(uint8_t col, uint8_t row)
{
    mui_rect_t r;

    r.x = I16(ui_col(col) + 12);
    r.y = I16(ui_row(row) + 32);
    r.x2 = I16(ui_col(col) + PANEL_W - 12);
    r.y2 = I16(ui_row(row) + PANEL_H - 10);
    return r;
}

/** @brief 画面板底 + 标题，返回内容区 */
static mui_rect_t ui_panel(uint8_t col, uint8_t row, const char *title)
{
    int16_t x = ui_col(col), y = ui_row(row);

    mui_round_rect_fill(x, y, PANEL_W, PANEL_H, 10, CARD_BG);
    mui_text_draw(I16(x + 12), I16(y + 7), title, &harmony_os_10, ACCENT, CARD_BG, 1);
    mui_hline_draw(I16(x + 12), I16(y + 24), I16(PANEL_W - 24), TRACK);
    return ui_body(col, row);
}

/* ================= 面板绘制 ================= */

/* (0,0) 基础图元 */
static void ui_p_primitives(mui_rect_t a)
{
    int16_t x = a.x, y = a.y;

    mui_rect_fill(x, y, 52, 26, ACCENT);
    mui_round_rect_fill(I16(x + 62), y, 52, 26, 9, ACCENT2);
    mui_rect_draw(x, I16(y + 36), 52, 26, FG);
    mui_round_rect_draw(I16(x + 62), I16(y + 36), 52, 26, 9, FG);

    mui_circle_fill(I16(x + 26), I16(y + 92), 18, WARN);
    mui_circle_draw(I16(x + 92), I16(y + 92), 18, FG);
    mui_ellipse_fill(I16(x + 140), I16(y + 92), 26, 17, MUI_PINK);

    mui_triangle_fill(I16(x + 8), I16(y + 168), I16(x + 88), I16(y + 168),
                      I16(x + 48), I16(y + 118), MUI_ORANGE);
    mui_triangle_draw(I16(x + 98), I16(y + 168), I16(x + 162), I16(y + 168),
                      I16(x + 130), I16(y + 128), FG);

    mui_line_draw(x, I16(y + 200), I16(x + 166), I16(y + 180), ACCENT);
    mui_hline_draw(x, I16(y + 218), 166, FG);
    mui_pixel_draw(I16(x + 4), I16(y + 228), MUI_RED);
    mui_pixel_draw(I16(x + 12), I16(y + 228), ACCENT2);
    mui_pixel_draw(I16(x + 20), I16(y + 228), WARN);
}

/* (0,1) 抗锯齿 */
static void ui_p_aa(mui_rect_t a)
{
    int16_t x = a.x, y = a.y;

    mui_line_draw_aa(x, I16(y + 8), I16(x + 166), I16(y + 36), ACCENT, CARD_BG);

    mui_circle_draw_aa(I16(x + 40), I16(y + 80), 22, ACCENT2, CARD_BG);
    mui_circle_fill_aa(I16(x + 126), I16(y + 80), 22, MUI_PINK, CARD_BG);

    mui_round_rect_fill_aa(x, I16(y + 114), 76, 46, 14, WARN, CARD_BG);
    mui_round_rect_draw_aa(I16(x + 90), I16(y + 114), 76, 46, 14, FG, CARD_BG);
    mui_round_rect_fill_aa(x, I16(y + 172), 76, 46, 23, ACCENT, CARD_BG);
    mui_round_rect_draw_aa(I16(x + 90), I16(y + 172), 76, 46, 23, ACCENT2, CARD_BG);
}

/* (0,2) 圆环 / 圆弧 */
static void ui_p_ring(mui_rect_t a)
{
    int16_t x = a.x, y = a.y;

    /* 整环轨道 + 270° 进度（直角填充） */
    mui_ring_fill_aa(I16(x + 44), I16(y + 44), 22, 36, 0, 360, TRACK, CARD_BG);
    mui_ring_fill_aa(I16(x + 44), I16(y + 44), 22, 36, 135, 405, ACCENT, CARD_BG);

    /* 实心扇形（rin = 0） */
    mui_ring_fill_aa(I16(x + 126), I16(y + 44), 0, 36, 0, 120, ACCENT2, CARD_BG);

    mui_text_draw(x, I16(y + 92), "ring progress / pie", &harmony_os_10, FG_DIM, CARD_BG, 1);

    /* 硬边 vs 抗锯齿 */
    mui_ring_fill(I16(x + 40), I16(y + 146), 14, 26, 0, 270, WARN);
    mui_ring_fill_aa(I16(x + 122), I16(y + 146), 14, 26, 0, 270, WARN, CARD_BG);

    mui_text_draw(x, I16(y + 186), "hard edge / AA", &harmony_os_10, FG_DIM, CARD_BG, 1);

    /* 描边圆弧（圆头端帽） */
    mui_arc_draw_aa(I16(x + 84), I16(y + 208), 16, 0, 250, 8, MUI_PINK, CARD_BG);
}

/* (0,3) 文字与对齐 */
static void ui_p_text(mui_rect_t a)
{
    int16_t x = a.x, y = a.y;
    int16_t bw = I16(a.x2 - a.x);

    mui_round_rect_fill(x, I16(y + 2), bw, 26, 4, TRACK);
    mui_text_draw_rect(x, I16(y + 2), bw, 26, "LEFT", &harmony_os_10, FG, TRACK, 1, MUI_ALIGN_LEFT);
    mui_round_rect_fill(x, I16(y + 32), bw, 26, 4, TRACK);
    mui_text_draw_rect(x, I16(y + 32), bw, 26, "CENTER", &harmony_os_10, ACCENT, TRACK, 1,
                       MUI_ALIGN_CENTER);
    mui_round_rect_fill(x, I16(y + 62), bw, 26, 4, TRACK);
    mui_text_draw_rect(x, I16(y + 62), bw, 26, "RIGHT", &harmony_os_10, ACCENT2, TRACK, 1,
                       MUI_ALIGN_RIGHT);

    mui_text_draw_ex(x, I16(y + 100), "Bold", &harmony_os_10, FG, CARD_BG, 1, MUI_TEXT_BOLD);
    mui_text_draw_ex(x, I16(y + 124), "Underline", &harmony_os_10, FG, CARD_BG, 1,
                     MUI_TEXT_UNDERLINE);
    mui_text_draw_ex(x, I16(y + 148), "Strike", &harmony_os_10, FG, CARD_BG, 1,
                     MUI_TEXT_STRIKE);

    mui_text_draw(x, I16(y + 180), "Scale x2", &harmony_os_10, FG, CARD_BG, 2);
    mui_text_draw(x, I16(y + 208), "Scale x1 (small)", &harmony_os_10, FG_DIM, CARD_BG, 1);
}

/* (0,4) 按钮 / 图片资源 */
static void ui_p_buttons(mui_rect_t a)
{
    int16_t x = a.x, y = a.y;

    mui_button_draw(&s_btn_normal);
    mui_button_draw(&s_btn_latch);
    mui_button_draw(&s_btn_icon);
    mui_button_draw(&s_btn_disabled);
    mui_button_draw(&s_btn_tex);

    /* 位图资源：RGB565 + 透明键 */
    mui_image_draw(x, I16(y + 112), logo.w, logo.h, logo.data);
    mui_text_draw(I16(x + logo.w + 8), I16(y + 114), "RGB565", &harmony_os_10, FG_DIM, CARD_BG, 1);

    /* 8bpp 蒙版图标：可用任意前景色染色 */
    mui_image_draw_mask(I16(x + 2), I16(y + 138), g12x12_2.w, g12x12_2.h,
                        g12x12_2.data, ACCENT2, CARD_BG);
    mui_image_draw_mask(I16(x + 24), I16(y + 138), g12x12_2.w, g12x12_2.h,
                        g12x12_2.data, WARN, CARD_BG);
    mui_text_draw(I16(x + 46), I16(y + 141), "mask icon", &harmony_os_10, FG_DIM, CARD_BG, 1);

    mui_text_draw(x, I16(y + 170), "normal / latch / icon", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 190), "disabled / texture", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 216), "* latch stays pressed", &harmony_os_10, FG_DIM, CARD_BG, 1);
}

/* (1,0) 进度条 / 滑块 */
static void ui_p_progress(mui_rect_t a)
{
    int16_t x = a.x, y = a.y;

    mui_progressbar_draw(&s_prog1);
    mui_progressbar_draw(&s_prog2);
    mui_progressbar_draw(&s_prog_v);
    mui_slider_draw(&s_slider);
    mui_slider_draw(&s_vslider);

    mui_text_draw(x, I16(y + 148), "horizontal / vertical bar", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 168), "AA rounded edges", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 194), "slider: drag H / V", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 214), "drag -> bar syncs", &harmony_os_10, FG_DIM, CARD_BG, 1);
}

/* (1,1) 开关 / 复选 / 单选 */
static void ui_p_toggles(mui_rect_t a)
{
    uint8_t i;

    (void)a;
    for (i = 0; i < 2; i++) {
        mui_switch_draw(&s_sw[i]);
    }
    for (i = 0; i < 2; i++) {
        mui_checkbox_draw(&s_cb[i]);
    }
    for (i = 0; i < 3; i++) {
        mui_radio_draw(&s_rd[i]);
    }
}

/* (1,2) 仪表盘 / 列表 */
static void ui_draw_list(void)
{
    uint8_t i, first = 0, count = 0;

    mui_list_begin(&s_list);
    mui_list_visible(&s_list, &first, &count);
    for (i = 0; i < count; i++) {
        uint8_t idx = (uint8_t)(first + i);
        mui_rect_t r = mui_list_row_rect(&s_list, idx);
        uint8_t sel = (uint8_t)(idx == mui_list_get_sel(&s_list));
        uint16_t bg = sel ? ACCENT : CARD_BG;
        uint16_t fg = sel ? MUI_WHITE : FG;
        char b[12];

        snprintf(b, sizeof(b), "Item %d", (int)idx);
        mui_rect_fill(I16(r.x + 1), r.y, I16(r.x2 - r.x - 2), I16(r.y2 - r.y), bg);
        mui_text_draw_rect(I16(r.x + 6), r.y, I16(r.x2 - r.x - 12), I16(r.y2 - r.y),
                           b, &harmony_os_10, fg, bg, 1, MUI_ALIGN_LEFT);
    }
    mui_list_end(&s_list);
}

static void ui_p_gauge_list(mui_rect_t a)
{
    (void)a;
    mui_gauge_draw(&s_gauge);
    mui_list_draw_frame(&s_list, CARD_BG, TRACK);
    ui_draw_list();
}

/* (1,3) 下拉 / 弹窗 */
static void ui_p_dropdown(mui_rect_t a)
{
    int16_t x = a.x, y = a.y;
    char b[24];

    mui_text_draw(x, I16(y + 2), "selected:", &harmony_os_10, FG_DIM, CARD_BG, 1);
    snprintf(b, sizeof(b), "%s", s_dd_items[mui_dropdown_selected(&s_dd)]);
    mui_text_draw(x, I16(y + 22), b, &harmony_os_10, ACCENT2, CARD_BG, 1);

    mui_button_draw(&s_btn_popup);

    mui_text_draw(x, I16(y + 130), "* dropdown overlays below", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 150), "  pick / outside closes", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 178), "* popup is modal:", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 198), "  eats all touches", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 222), "* close -> full repaint", &harmony_os_10, FG_DIM, CARD_BG, 1);
}

/* (1,4) 交互说明 */
static void ui_p_help(mui_rect_t a)
{
    int16_t x = a.x, y = a.y;

    mui_text_draw(x, I16(y + 4), "Touch", &harmony_os_10, FG, CARD_BG, 1);
    mui_text_draw(x, I16(y + 26), "* drag slider = value", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 46), "* drag list = scroll", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 66), "* tap = toggle state", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 86), "* dropdown / popup overlay", &harmony_os_10, FG_DIM, CARD_BG, 1);

    mui_text_draw(x, I16(y + 116), "Window keys", &harmony_os_10, FG, CARD_BG, 1);
    mui_text_draw(x, I16(y + 138), "* ESC quit", &harmony_os_10, FG_DIM, CARD_BG, 1);
    mui_text_draw(x, I16(y + 158), "* S save png", &harmony_os_10, FG_DIM, CARD_BG, 1);

    mui_text_draw(x, I16(y + 188), "Run this demo after", &harmony_os_10, ACCENT, CARD_BG, 1);
    mui_text_draw(x, I16(y + 206), "each library change.", &harmony_os_10, ACCENT, CARD_BG, 1);
}

/* ================= 顶栏 ================= */
static void ui_draw_header(void)
{
    mui_rect_fill(0, 0, BSP_LCD_WIDTH, HDR_H, HDR_BG);
    mui_text_draw_ex(12, I16((HDR_H - 14) / 2), "MUI Widget Gallery",
                     &harmony_os_black_14, FG, HDR_BG, 1, MUI_TEXT_BOLD);
    mui_text_draw(220, I16((HDR_H - 10) / 2),
                  "tap / drag any control to interact",
                  &harmony_os_10, FG_DIM, HDR_BG, 1);
    mui_circle_fill(I16(BSP_LCD_WIDTH - 132), I16(HDR_H / 2), 3, ACCENT2);
    mui_label_draw(&s_clock);
    mui_hline_draw(0, HDR_H, BSP_LCD_WIDTH, TRACK);
}

/* ================= 全屏重绘 ================= */

/** @brief 作废全部控件缓存（整屏重画前必须调，否则会被"状态未变"跳过） */
static void ui_invalidate_all(void)
{
    uint8_t i;

    mui_button_invalidate(&s_btn_normal);
    mui_button_invalidate(&s_btn_latch);
    mui_button_invalidate(&s_btn_disabled);
    mui_button_invalidate(&s_btn_icon);
    mui_button_invalidate(&s_btn_tex);
    mui_button_invalidate(&s_btn_popup);
    mui_button_invalidate(&s_dd.btn);
    for (i = 0; i < s_pp.nbtn; i++) {
        mui_button_invalidate(&s_pp.btn[i]);
    }
    s_pp.needs_panel = 1;
    mui_gauge_invalidate(&s_gauge);
}

static void ui_draw_all(void)
{
    mui_reset_clip();
    mui_screen_clear(PANEL_BG);
    ui_draw_header();
    ui_invalidate_all();

    ui_p_primitives(ui_panel(0, 0, "Primitives"));
    ui_p_aa(ui_panel(1, 0, "Anti-aliasing"));
    ui_p_ring(ui_panel(2, 0, "Ring / Arc"));
    ui_p_text(ui_panel(3, 0, "Text & Align"));
    ui_p_buttons(ui_panel(4, 0, "Button / Image"));
    ui_p_progress(ui_panel(0, 1, "Progress / Slider"));
    ui_p_toggles(ui_panel(1, 1, "Toggle / Check"));
    ui_p_gauge_list(ui_panel(2, 1, "Gauge / List"));
    ui_p_dropdown(ui_panel(3, 1, "Dropdown / Popup"));
    ui_p_help(ui_panel(4, 1, "Hints"));

    mui_dropdown_draw(&s_dd);   /* 展开时覆盖在面板之上 */
    mui_popup_draw(&s_pp);      /* 模态，最上层 */
}

/**
 * @brief 整帧绘制回调：把整屏从头画满
 *
 * 条带后端会按带反复调用它（每次只保留一条带的像素），所以它必须"幂等"：
 * 先铺满底色再画全部内容，不依赖上一次绘制留下的任何东西。
 */
static void ui_frame_draw(void *ctx)
{
    (void)ctx;
    ui_draw_all();
}

/* ================= 初始化 ================= */
void demo_ui_init(void)
{
    int i;

    mui_init(BSP_LCD_WIDTH, BSP_LCD_HEIGHT);

    /* 顶栏时钟（右对齐标签） */
    mui_label_init(&s_clock, I16(BSP_LCD_WIDTH - 16 - 80), I16((HDR_H - 10) / 2),
                   &harmony_os_10, FG, HDR_BG, 1);
    mui_label_set_align(&s_clock, MUI_ALIGN_RIGHT, 80);

    /* ---- (0,4) 按钮 / 图片 ---- */
    {
        mui_rect_t a = ui_body(4, 0);
        int16_t bx = a.x, by = a.y;
        int16_t bw = I16((a.x2 - a.x - 12) / 2);

        mui_button_init(&s_btn_normal, bx, I16(by + 2), bw, 28, &s_btn_primary, "OK", NULL);
        mui_button_set_font(&s_btn_normal, &harmony_os_10);
        mui_button_init(&s_btn_latch, I16(bx + bw + 12), I16(by + 2), bw, 28,
                        &s_btn_ghost, "Latch", NULL);
        mui_button_set_font(&s_btn_latch, &harmony_os_10);
        mui_button_set_latch(&s_btn_latch, 1);
        mui_button_init(&s_btn_icon, bx, I16(by + 38), bw, 28,
                        &s_btn_primary, "Icon", &g12x12_1);
        mui_button_set_font(&s_btn_icon, &harmony_os_10);
        mui_button_init(&s_btn_disabled, I16(bx + bw + 12), I16(by + 38), bw, 28,
                        &s_btn_primary, "Disabled", NULL);
        mui_button_set_font(&s_btn_disabled, &harmony_os_10);
        mui_button_set_enabled(&s_btn_disabled, 0);

        for (i = 0; i < 150 * 30; i++) {
            uint8_t t = (uint8_t)(((i % 150) * 255) / 149);
            s_tex_data[i] = mui_color_mix(MUI_RGB565(0x0A, 0x2A, 0x5A),
                                          MUI_RGB565(0x2E, 0xCC, 0xFF), t);
        }
        s_tex_img.w = 150;
        s_tex_img.h = 30;
        s_tex_img.data = s_tex_data;
        /* 底板贴图按自身尺寸绘制，故按钮宽度与贴图一致（150） */
        mui_button_init(&s_btn_tex, bx, I16(by + 74), 150, 28,
                        &s_btn_primary, "Texture", NULL);
        mui_button_set_font(&s_btn_tex, &harmony_os_10);
        mui_button_set_bg_image(&s_btn_tex, &s_tex_img);
    }

    /* ---- (1,0) 进度条 / 滑块 ---- */
    {
        mui_rect_t a = ui_body(0, 1);
        int16_t px = a.x, py = a.y;

        /* 注意：滑块的"实际占位"= 外接矩形向外扩 knob_r（圆头越出轨道，绘制前还要
         * 把这块擦成 screen_bg），布局时必须按这个扩过的范围避让相邻控件，
         * 否则相邻控件会被擦掉一条。下面把左列控制在 x+16..x+119、垂直进度条放到
         * x+140，两者之间留出 ≥8px，滑块右扩 13px 后仍够不着进度条。 */
        mui_progressbar_init(&s_prog1, I16(px + 16), I16(py + 4), 104, 16, &s_pb_h);
        mui_progressbar_set_value(&s_prog1, 35);
        mui_progressbar_init(&s_prog2, I16(px + 16), I16(py + 28), 104, 16, &s_pb_h);
        mui_progressbar_set_value(&s_prog2, 78);
        mui_progressbar_init(&s_prog_v, I16(px + 140), I16(py + 4), 18, 70, &s_pb_v);
        mui_progressbar_set_value(&s_prog_v, 60);

        mui_slider_init(&s_slider, I16(px + 16), I16(py + 58), 104, 30,
                        0, 100, 65, &s_slider_h);
        mui_slider_init(&s_vslider, I16(px + 134), I16(py + 92), 30, 96,
                        0, 100, 40, &s_slider_v);
    }

    /* ---- (1,1) 开关 / 复选 / 单选 ---- */
    {
        mui_rect_t a = ui_body(1, 1);
        int16_t tx = a.x, ty = a.y;
        int16_t tw = I16(a.x2 - a.x);

        mui_switch_init(&s_sw[0], tx, I16(ty + 4), tw, 28, 1, &s_switch);
        mui_switch_set_text(&s_sw[0], "Lamp");
        mui_switch_set_font(&s_sw[0], &harmony_os_10);
        mui_switch_init(&s_sw[1], tx, I16(ty + 38), tw, 28, 0, &s_switch);
        mui_switch_set_text(&s_sw[1], "Aircon");
        mui_switch_set_font(&s_sw[1], &harmony_os_10);

        mui_checkbox_init(&s_cb[0], tx, I16(ty + 78), tw, 26, 1, &s_check);
        mui_checkbox_set_text(&s_cb[0], "Fresh air");
        mui_checkbox_set_font(&s_cb[0], &harmony_os_10);
        mui_checkbox_init(&s_cb[1], tx, I16(ty + 110), tw, 26, 0, &s_check);
        mui_checkbox_set_text(&s_cb[1], "Curtain");
        mui_checkbox_set_font(&s_cb[1], &harmony_os_10);

        s_rd_grp[0] = &s_rd[0];
        s_rd_grp[1] = &s_rd[1];
        s_rd_grp[2] = &s_rd[2];
        s_rd_grp[3] = NULL;
        mui_radio_init(&s_rd[0], tx, I16(ty + 146), tw, 26, 1, &s_radio);
        mui_radio_set_text(&s_rd[0], "Auto");
        mui_radio_init(&s_rd[1], tx, I16(ty + 176), tw, 26, 0, &s_radio);
        mui_radio_set_text(&s_rd[1], "Manual");
        mui_radio_init(&s_rd[2], tx, I16(ty + 206), tw, 26, 0, &s_radio);
        mui_radio_set_text(&s_rd[2], "Timer");
        for (i = 0; i < 3; i++) {
            mui_radio_set_font(&s_rd[i], &harmony_os_10);
            mui_radio_set_group(&s_rd[i], s_rd_grp);
        }
    }

    /* ---- (1,2) 仪表盘 / 列表 ---- */
    {
        mui_rect_t a = ui_body(2, 1);
        int16_t gx = I16(a.x + (a.x2 - a.x) / 2);
        int16_t gy = I16(a.y + 62);

        mui_gauge_init(&s_gauge, gx, gy, 52, 0, 100, 65, &s_gauge_style);
        mui_gauge_set_font(&s_gauge, &harmony_os_light_20);
        mui_anim_init(&s_gauge_anim, 65, MUI_EASE_IN_OUT_CUBIC);
        mui_anim_to(&s_gauge_anim, 100, 2500);

        mui_list_init(&s_list, I16(a.x + 2), I16(a.y + 124), I16(a.x2 - a.x - 4), 104,
                      22, 2);
        mui_list_set_rows(&s_list, 20);
        mui_list_set_sel(&s_list, 1);
    }

    /* ---- (1,3) 下拉 / 弹窗 ---- */
    {
        mui_rect_t a = ui_body(3, 1);
        int16_t dx = a.x, dy = a.y;

        mui_dropdown_init(&s_dd, dx, I16(dy + 42), I16(a.x2 - a.x), 28,
                          s_dd_items, 4, 26, &harmony_os_10, &s_btn_primary);
        mui_dropdown_set_colors(&s_dd, CARD_BG, ACCENT, FG, MUI_WHITE, TRACK);
        mui_dropdown_set_max_visible(&s_dd, 4);

        mui_button_init(&s_btn_popup, dx, I16(dy + 86), I16(a.x2 - a.x), 32,
                        &s_btn_ok, "Open popup", NULL);
        mui_button_set_font(&s_btn_popup, &harmony_os_10);

        mui_popup_init(&s_pp, I16((BSP_LCD_WIDTH - 420) / 2),
                       I16((BSP_LCD_HEIGHT - 220) / 2), 420, 220,
                       "Modal Popup", "Body text, centered. Buttons at the bottom.",
                       &harmony_os_10);
        mui_popup_set_colors(&s_pp, CARD_BG, ACCENT, FG, FG);
        mui_popup_add_button(&s_pp, "Cancel", &s_btn_ghost);
        mui_popup_add_button(&s_pp, "OK", &s_btn_ok);
    }

    /* ---- 时钟初值 ---- */
    snprintf(s_clock_buf, sizeof(s_clock_buf), "%02d:%02d", 14, 32);
    s_clock_ms = bsp_system_tick_ms();
    mui_label_set_text(&s_clock, s_clock_buf);   /* 置 drawn=1，重绘时 label_draw 才生效 */

    /* 首屏：走整帧绘制通道（条带后端下会按带重放，用户代码无需区分后端） */
    mui_screen_frame(ui_frame_draw, NULL);
}

/* ================= 交互 ================= */
static void ui_on_click_widgets(mui_touch_event_t ev)
{
    uint8_t i;
    uint8_t slider_changed = 0;

    if (mui_slider_touch(&s_slider, ev)) {
        mui_progressbar_set_value(&s_prog1, (uint8_t)mui_slider_get_value(&s_slider));
        slider_changed = 1;
    }
    if (mui_slider_touch(&s_vslider, ev)) {
        mui_progressbar_set_value(&s_prog_v, (uint8_t)mui_slider_get_value(&s_vslider));
        slider_changed = 1;
    }
    if (slider_changed) {
        UI_PAINT(mui_slider_draw(&s_slider));
        UI_PAINT(mui_slider_draw(&s_vslider));
    }

    for (i = 0; i < 2; i++) {
        if (mui_switch_touch(&s_sw[i], ev)) {
            UI_PAINT(mui_switch_draw(&s_sw[i]));
        }
        if (mui_checkbox_touch(&s_cb[i], ev)) {
            UI_PAINT(mui_checkbox_draw(&s_cb[i]));
        }
    }
    for (i = 0; i < 3; i++) {
        if (mui_radio_touch(&s_rd[i], ev)) {
            uint8_t k;
            for (k = 0; k < 3; k++) {
                UI_PAINT(mui_radio_draw(&s_rd[k]));
            }
            break;
        }
    }

    (void)mui_button_touch(&s_btn_normal, ev);
    if (mui_button_touch(&s_btn_latch, ev)) {
        UI_PAINT(mui_button_draw(&s_btn_latch));
    }
    (void)mui_button_touch(&s_btn_icon, ev);

    if (mui_list_touch(&s_list, ev) != 0) {
        UI_PAINT(ui_draw_list());
    }
}

/* ================= 帧循环 ================= */
static uint8_t s_need_full;

void demo_ui_frame(void)
{
    mui_touch_event_t ev;

    mui_tick_update(bsp_system_tick_ms());

    while ((ev = mui_touch_poll()) != MUI_TOUCH_NONE) {
        /* 1) 弹窗优先（模态，吃掉全部事件） */
        if (mui_popup_is_open(&s_pp)) {
            if (mui_popup_touch(&s_pp, ev) != MUI_POPUP_NONE) {
                s_need_full = 1;                 /* 关闭 → 重绘底层 */
            } else {
                UI_PAINT(mui_popup_draw(&s_pp)); /* 只更新按钮高亮 */
            }
            continue;
        }
        /* 2) 下拉展开时优先 */
        if (mui_dropdown_is_open(&s_dd)) {
            uint8_t r = mui_dropdown_touch(&s_dd, ev);
            if (r == MUI_DROPDOWN_CLOSED) {
                s_need_full = 1;
            } else if (r == MUI_DROPDOWN_CHANGED) {
                UI_PAINT(mui_dropdown_draw(&s_dd));
            }
            continue;
        }
        /* 3) 普通控件 */
        if (mui_dropdown_touch(&s_dd, ev) == MUI_DROPDOWN_CHANGED) {
            UI_PAINT(mui_dropdown_draw(&s_dd));
            continue;
        }
        if (mui_button_touch(&s_btn_popup, ev)) {
            mui_popup_open(&s_pp);
            s_need_full = 1;
            continue;
        }
        ui_on_click_widgets(ev);
    }

    /* 时钟：每满 1 秒走字 */
    {
        uint32_t ms = bsp_system_tick_ms();
        if (ms - s_clock_ms >= 1000) {
            uint32_t tot;
            char nb[6];

            s_clock_ms = ms;
            tot = 14 * 3600u + 32 * 60u + ms / 1000u;
            snprintf(nb, sizeof(nb), "%02d:%02d",
                     (int)((tot / 3600u) % 24u), (int)((tot / 60u) % 60u));
            if (memcmp(nb, s_clock_buf, 5) != 0) {
                memcpy(s_clock_buf, nb, 6);
                mui_label_set_text(&s_clock, s_clock_buf);
            }
        }
    }

    /* 仪表盘：mui_anim 缓动来回扫（弹窗打开时暂停，避免画到弹窗上） */
    if (!mui_popup_is_open(&s_pp)) {
        if (mui_anim_done(&s_gauge_anim)) {
            mui_anim_to(&s_gauge_anim,
                        (mui_anim_value(&s_gauge_anim) >= 100) ? 0 : 100, 2500);
        }
        mui_anim_update(&s_gauge_anim, mui_tick_delta());
        mui_gauge_set_value(&s_gauge, (int16_t)mui_anim_value(&s_gauge_anim));
        UI_PAINT(mui_gauge_draw(&s_gauge));
    }

#if MUI_CFG_IS_STRIP
    /* 条带后端：状态都改完了，整屏重绘一遍（回调会被按带重放 N 次） */
    mui_screen_frame(ui_frame_draw, NULL);
#else
    /* 常规后端：帧内已按需局部重画，这里只兜"需要整屏重绘"的情况 */
    if (s_need_full) {
        s_need_full = 0;
        ui_draw_all();
    }
    mui_screen_flush();
#endif
}

void demo_ui_key(uint8_t key_id, uint8_t down)
{
    (void)key_id;
    (void)down;
}
