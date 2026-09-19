/**
 * @file demo_ui.c
 * @brief 界面层：红光治疗设备 UI（纯 MUI API，平台无关）
 *
 * 按钮采用 OO 保留模式：静态创建对象，一次 init，之后只需 set_state + draw。
 */

#include "mui.h"
#include "mui_button.h"
#include "mui_label.h"
#include "mui_font.h"
#include "img_logo.h"
#include "demo_ui.h"
#include "img_g12x12_1.h"
#include "mui_font_harmony_os_10.h"
#include "mui_font_harmony_os_32.h"

/* -------- 自定义按钮样式 -------- */

/* 蓝色圆角（默认按钮） */
static const mui_button_style_t style_blue = {
    MUI_RGB565(0x71, 0xb0, 0xff),     /* bg */
    MUI_RGB565(0x00, 0x4e, 0xff),     /* bg_press */
    MUI_WHITE, MUI_WHITE,
    MUI_BTN_SHAPE_ROUND, 3
};

/* -------- 按钮对象（静态创建 · 保留模式） -------- */

#define BTN_COUNT 4
static mui_button_t s_buttons[BTN_COUNT];
static mui_button_t s_mode;

/**
 * @brief 一次性初始化所有按钮对象
 */
static void ui_init_buttons(void)
{
    int16_t sw = mui_get_width();
    int16_t sh = mui_get_height();
    int16_t margin = 2;
    int16_t gap = 1;
    int16_t bw = (int16_t)((sw - 2 * margin - 3 * gap) / BTN_COUNT);
    int16_t bh = 15;
    int16_t by = (int16_t)(sh - margin - bh);
    int16_t x;
    int i;

    // 模式
    mui_button_init(&s_mode, (128-70)/2, 28, 70, bh,
                            &style_blue, NULL, NULL);
    mui_button_set_text(&s_mode, "Mode:RED");
    mui_button_set_font(&s_mode, &harmony_os_10);
    mui_button_set_latch(&s_mode, 1);   /* 开关：按下保持，再按弹起 */
    for (i = 0; i < BTN_COUNT; i++) {
        x = (int16_t)(margin + i * (bw + gap));

        if (i == 0) {
            /* 按钮 0：带图标（保留模式，一次创建） */
            mui_button_init(&s_buttons[i], x, by, bw, bh,
                            &style_blue, NULL, &g12x12_1);
            mui_button_set_latch(&s_buttons[i], 1);   /* init 之后再设锁存 */
        } else {
            /* 其余三个：蓝色圆角按钮 */
            mui_button_init(&s_buttons[i], x, by, bw, bh,
                            &style_blue, NULL, NULL);
        }
    }

}

/**
 * @brief 屏幕中部 32px 大号时间（HH:MM），label 对象自动擦旧画新
 */
static uint8_t s_minutes = 30;
static uint8_t s_mode_red = 1;   /* Mode 当前显示：1=RED 0=OFF */
static mui_label_t s_clock;      /* 中部大时钟 */
static mui_label_t s_lbl_pct;    /* 静态：60% */
static mui_label_t s_lbl_hz;     /* 静态：100Hz */
static mui_label_t s_lbl_off;    /* 静态：OFF */

/** @brief 更新时钟文本（坐标/擦除由 label 对象管理） */
static void update_clock_text(void)
{
    char txt[6];

    txt[0] = '1';
    txt[1] = '2';
    txt[2] = ':';
    txt[3] = (char)('0' + s_minutes / 10);
    txt[4] = (char)('0' + s_minutes % 10);
    txt[5] = '\0';
    mui_label_set_text(&s_clock, txt);
}

#define LINE_H  (int16_t)(mui_get_height() - 25)

void demo_ui_key(uint8_t key_id, uint8_t down)
{
    mui_button_t *btn;

    switch (key_id) {
    case 0: case 1: case 2: case 3: btn = &s_buttons[key_id]; break;
    case 4: btn = &s_mode; break;
    default: return;
    }

    if (down) {
        /* 按下 = 点击该按钮中心（合成触摸事件，高亮/点击逻辑全复用） */
        mui_touch_update((int16_t)(btn->x + btn->w / 2),
                         (int16_t)(btn->y + btn->h / 2), 1);
    } else {
        mui_touch_update(0, 0, 0);
    }
}

void demo_ui_frame(void)
{
    static uint32_t t = 0;
    static uint8_t inited = 0;

    /* ---- 静态初始化：只跑一次 ---- */
    if (!inited) {
        inited = 1;
        mui_clear_screen(MUI_WHITE);

        /* 顶部 logo（位图，水平居中） */
        mui_draw_bitmap((int16_t)((mui_get_width() - logo.w) / 2), 4,
                        logo.w, logo.h, logo.data);
        mui_draw_hline(2, 20, (int16_t)(mui_get_width() - 4), MUI_DARKGREY);

        /* 屏幕中部大数字时钟（32px label，一次 init 位置，更新只调 set_text） */
        {
            int16_t tw = mui_lv_font_text_width("12:30", &harmony_os_32, 1);
            int16_t tx = (int16_t)((mui_get_width() - tw) / 2);
            int16_t ty = (int16_t)(22 + ((mui_get_height() - 2 - 15) - 22
                               - harmony_os_32.line_height) / 2);
            mui_label_init(&s_clock, tx, ty, &harmony_os_32,
                           MUI_BLACK, MUI_WHITE, 1);
        }
        update_clock_text();

        /* 按钮对象初始化（静态创建，之后只需改状态） */
        ui_init_buttons();

        /* 底部信息条文本（静态 label，一次绘制） */
        mui_label_init(&s_lbl_pct, 10, (int16_t)(LINE_H - 12), &harmony_os_10,
                       MUI_RGB565(0xa0, 0xa0, 0xa0), MUI_WHITE, 1);
        mui_label_set_text(&s_lbl_pct, "60%");
        mui_label_init(&s_lbl_hz, 45, (int16_t)(LINE_H - 12), &harmony_os_10,
                       MUI_RGB565(0xa0, 0xa0, 0xa0), MUI_WHITE, 1);
        mui_label_set_text(&s_lbl_hz, "100Hz");
        mui_label_init(&s_lbl_off, 102, (int16_t)(LINE_H - 12), &harmony_os_10,
                       MUI_RGB565(0x00, 0x4e, 0xff), MUI_WHITE, 1);
        mui_label_set_text(&s_lbl_off, "OFF");
        mui_draw_hline(10,LINE_H,25,MUI_RGB565(0xa0, 0xa0, 0xa0));
        mui_draw_hline(45,LINE_H,30,MUI_LIGHTGREY);
        mui_draw_hline(102,LINE_H,20,MUI_BLACK);
    }

    /* ---- 动态：触摸事件分发 + 每帧重绘 ---- */

    /* 触摸事件：点击底部按钮 → 时钟分钟 +1；点击 Mode → RED/OFF 切换 */
    {
        mui_touch_event_t ev;
        while ((ev = mui_touch_poll()) != MUI_TOUCH_NONE) {
            int i;
            for (i = 0; i < BTN_COUNT; i++) {
                if (mui_button_touch(&s_buttons[i], ev)) {
                    s_minutes = (uint8_t)((s_minutes + 1) % 60);
                    update_clock_text();
                }
            }
            if (mui_button_touch(&s_mode, ev)) {
                s_mode_red = !s_mode_red;
                mui_button_set_text(&s_mode, s_mode_red ? "Mode:RED" : "Mode:OFF");
            }
        }
    }

    /* 按钮重绘（状态可能已被触摸改变） */
    for (int i = 0; i < BTN_COUNT; i++) {
        mui_button_draw(&s_buttons[i]);
    }
    mui_button_draw(&s_mode);

    t++;
}
