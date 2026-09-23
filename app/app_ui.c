/**
 * @file app_ui.c
 * @brief 应用界面（128x128）：logo + 4 个页面 + 底部四键
 *
 * 页面：0 首页（倒计时 + 信息条）｜1 模式列表｜2 进度百分比｜3 数值 + 信号条
 * 按键：键号→功能见 s_key_fn[]；+/- 是页内参数键（首页倒计时 / 换模式 / 百分比 / 数值）
 * 刷新：静态部分只在 init 画一次，状态变化按需局部重画，缓冲后端在每处绘制末尾 flush
 */

#include "app_ui.h"
#include "mui.h"
#include "mui_button.h"
#include "mui_label.h"
#include "mui_font.h"
#include "img_logo.h"
#include "bsp_lcd.h"
#include "bsp_system.h"
#include "mui_font_harmony_os_10.h"
#include "mui_font_harmony_os_light_20.h"
#include "mui_font_harmony_os_medium_38.h"
#include "mui_font_harmony_os_black_14.h"
#include "img_g12x12_1.h"
#include "img_g12x12_2.h"
#include "img_g12x12_3.h"
#include "img_g12x12_4.h"
#include "img_pro_0.h"
#include "img_pro_1.h"
#include "img_pro_2.h"
#include "img_pro_3.h"
#include "img_pro_4.h"
#include "img_pro_5.h"
#include "img_pro_6.h"
#include "img_pro_7.h"
#include "img_pro_8.h"
#include "img_pro_9.h"
#include "img_pro_10.h"

/* -------- 主题 --------
 * UI_BG 必须与真实屏幕底色一致：文字抗锯齿混合、label 擦除铺底都依赖它。
 */
#define UI_BG                MUI_WHITE
#define THEME_DEEP_BLUE      MUI_RGB565(0, 109, 249)      /* 主题深蓝 */
#define THEME_BLUE           MUI_RGB565(4, 170, 244)      /* 按钮未按下 */
#define UI_NULL_TXT          "Null"                       /* 无读数占位（第 4 页大字 + 信息条第 2 项） */

/* -------- 底部四键 -------- */
#define UI_BTN_NUM  4
static mui_button_t s_btns[UI_BTN_NUM];

#define BTN_MARGIN  (2)
#define BTN_GAP     (1)
#define BTN_H       (15)
#define BTN_W       ((int16_t)((BSP_LCD_WIDTH - 2 * BTN_MARGIN - 3 * BTN_GAP) / 4))
#define BTN_TOP     ((int16_t)(BSP_LCD_HEIGHT - BTN_MARGIN - BTN_H))

static const mui_button_style_t s_btn_style = {
    .bg = THEME_BLUE,
    .bg_press = THEME_DEEP_BLUE,
    .fg = MUI_WHITE,
    .border = MUI_WHITE,
    .shape = MUI_BUTTON_SHAPE_ROUND,
    .radius = 3
};

/* -------- 页面与内容区 -------- */
#define UI_PAGE_NUM         (4)
#define UI_CONTENT_TOP      (22)                              /* 分隔线之下 */
#define UI_CONTENT_BOTTOM   (BTN_TOP)                         /* 底部四键之上 */
#define UI_CONTENT_H        ((int16_t)(UI_CONTENT_BOTTOM - UI_CONTENT_TOP))
#define INFO_RULE_Y         ((int16_t)(BSP_LCD_HEIGHT - 25))  /* 信息条下划线所在行 */

/* -------- Mode：首页 MODE 按钮与第 2 页列表共用 s_mode_sel -------- */
#define MODE_X      ((int16_t)((BSP_LCD_WIDTH - 60) / 2))
#define MODE_Y      (28)
#define MODE_W      (60)
#define MODE_LIST_NUM   (3)
#define MODE_COLOR_ON   THEME_DEEP_BLUE       /* 选中 */
#define MODE_COLOR_OFF  MUI_BLACK             /* 未选中 */

static const char *const s_mode_names[MODE_LIST_NUM]  = { "ALL", "RED", "NIR" };
static const char *const s_mode_titles[MODE_LIST_NUM] = { "Mode:ALL", "Mode:RED", "Mode:NIR" };
static uint8_t s_mode_sel = 1;                /* 默认 RED */
static mui_button_t s_mode;

/* -------- 键号 → 功能（换键/换功能只改这张表） -------- */
typedef enum {
    KEY_FN_HOME_POWER = 0,    /**< 回首页；已在首页则切换开关（ON/OFF） */
    KEY_FN_PAGE_NEXT,         /**< 循环切页 */
    KEY_FN_MODE_NEXT,         /**< 换下一个模式（当前未分配） */
    KEY_FN_SIGNAL_UP,         /**< 页内参数 +1 */
    KEY_FN_SIGNAL_DOWN,       /**< 页内参数 -1 */
    KEY_FN_NONE               /**< 不响应 */
} app_key_fn_t;

static const uint8_t s_key_fn[] = {
    KEY_FN_NONE,          /* k4：未分配 */
    KEY_FN_PAGE_NEXT,     /* k1：切页 */
    KEY_FN_SIGNAL_UP,     /* k5：加 */
    KEY_FN_HOME_POWER,    /* k3：回首页 / 首页切开关 */
    KEY_FN_SIGNAL_DOWN,   /* k2：减 */
};
#define KEY_FN_NUM  ((uint8_t)(sizeof(s_key_fn) / sizeof(s_key_fn[0])))

/* -------- 首页大号倒计时（mm:ss） --------
 * 上电 10:00、开关为关（不递减、冒号常亮）；开启后从当前值继续递减（不重置）；
 * 到 0 自动关；首页 +/- 每次调 1 分钟。
 */
#define TIMER_DEFAULT_SEC   (10 * 60)
#define TIMER_STEP_SEC      (60)
#define TIMER_MAX_SEC       (99 * 60)
#define UI_CLOCK_TOP        (22)
#define UI_CLOCK_BOTTOM     ((int16_t)(BSP_LCD_HEIGHT - 2 - 15))

static const char UI_CLOCK_REF[] = "88:88";   /* 基准串：定居中位置，防跳动 */
static mui_label_t s_clock;
static uint8_t  s_power_on = 0;
static uint16_t s_remain_s = TIMER_DEFAULT_SEC;
static uint32_t s_timer_tick;

/* -------- 冒号闪烁（只动冒号一格，不碰两侧数字） --------
 * 前提：数字墨迹不越出自己的步进宽（medium_38 实测成立），故冒号格可与数字格分离。
 */
#define CLOCK_COLON_BLINK_MS   (500)

static int16_t  s_clock_colon_x;
static int16_t  s_clock_colon_w;
static uint8_t  s_clock_colon_on = 1;
static uint32_t s_clock_colon_tick;

/** @brief 时间标签的居中布局 + 冒号格坐标 */
static void ui_clock_layout(void)
{
    int16_t tw = mui_text_width(UI_CLOCK_REF, &harmony_os_medium_38, 1);
    int16_t tx = (int16_t)((BSP_LCD_WIDTH - tw) / 2);
    int16_t ty = (int16_t)(UI_CLOCK_TOP
                           + (UI_CLOCK_BOTTOM - UI_CLOCK_TOP
                              - harmony_os_medium_38.line_height) / 2);

    mui_label_init(&s_clock, tx, ty, &harmony_os_medium_38, MUI_BLACK, UI_BG, 1);

    /* 冒号是第 3 个字符：格左 = 文本左 + "88" 宽，格宽 = ':' 的步进宽 */
    s_clock_colon_x = (int16_t)(tx + mui_text_width("88", &harmony_os_medium_38, 1));
    s_clock_colon_w = mui_text_width(":", &harmony_os_medium_38, 1);
}

/** @brief 按当前相位画/抹冒号 */
static void ui_clock_colon_apply(void)
{
    if (s_clock_colon_on) {
        mui_text_draw_char(s_clock_colon_x, s_clock.y, ':',
                           s_clock.font, s_clock.fg, s_clock.bg, s_clock.scale);
    } else {
        mui_rect_fill(s_clock_colon_x, s_clock.y, s_clock_colon_w,
                      s_clock.font->line_height, s_clock.bg);
    }
}

/** @brief 冒号复位为"亮"并重新计时，立刻补画 */
static void ui_clock_colon_reset(void)
{
    s_clock_colon_on = 1;
    s_clock_colon_tick = bsp_system_tick_ms();
    ui_clock_colon_apply();
}

/** @brief 秒数 → "mm:ss" */
static void ui_fmt_mmss(char *txt, uint32_t sec)
{
    uint32_t mm = (sec / 60) % 100;
    uint32_t ss = sec % 60;

    txt[0] = (char)('0' + (mm / 10) % 10);
    txt[1] = (char)('0' + mm % 10);
    txt[2] = ':';
    txt[3] = (char)('0' + (ss / 10) % 10);
    txt[4] = (char)('0' + ss % 10);
    txt[5] = '\0';
}

/** @brief 刷新大号时间（label 增量擦旧画新），并补回可能被扫到的冒号格 */
static void ui_clock_update(uint32_t sec)
{
    char txt[6];

    ui_fmt_mmss(txt, sec);
    mui_label_set_text(&s_clock, txt);
    ui_clock_colon_apply();
}

/* -------- 当前页 -------- */
static uint8_t s_page = 0;

/* -------- 第 3 页：进度百分比 + 图片进度条 --------
 * 帧号 = 百分比 / PRO_PCT_STEP，与 img_pro_0~10 一一对应（pro_0 = 空、pro_10 = 满）。
 * 布局：条 34x76（垂直居中于内容区）+ 6px 间距 + 右侧文字块（百分比 + "Power"），整体横向居中。
 */
#define PRO_PCT_STEP   (10)
#define PRO_PCT_MAX    (100)
#define PRO_GAP        (6)
#define PRO_TEXT_W     (48)                    /* = "100%" 实测宽（light_20：11+11+11+15） */
#define PRO_LABEL      "Power"
#define PRO_GROUP_W    ((int16_t)(pro_0.w + PRO_GAP + PRO_TEXT_W))
#define PRO_BAR_X      ((int16_t)((BSP_LCD_WIDTH - PRO_GROUP_W) / 2))
#define PRO_TEXT_X     ((int16_t)(PRO_BAR_X + pro_0.w + PRO_GAP))
#define PRO_BAR_Y      ((int16_t)(UI_CONTENT_TOP + (UI_CONTENT_H - pro_0.h) / 2))
#define PRO_BLOCK_H    ((int16_t)(harmony_os_light_20.line_height + harmony_os_10.line_height))
#define PRO_VAL_Y      ((int16_t)(UI_CONTENT_TOP + (UI_CONTENT_H - PRO_BLOCK_H) / 2))
#define PRO_LBL_Y      ((int16_t)(PRO_VAL_Y + harmony_os_light_20.line_height))

static const mui_image_t *const s_pro_frames[] = {
    &pro_0, &pro_1, &pro_2, &pro_3, &pro_4, &pro_5,
    &pro_6, &pro_7, &pro_8, &pro_9, &pro_10
};
#define PRO_FRAME_NUM  ((uint8_t)(sizeof(s_pro_frames) / sizeof(s_pro_frames[0])))

/* -------- 页参数：唯一真值，定义在这里是因为首页信息条要读（信息条排在页面实现之前） -------- */
#define SIG_VAL_MAX      (1000)                /* 第 4 页数值上限 */
static uint8_t  s_percent = 60;                /* 第 3 页进度百分比 */
static uint16_t s_sig_val = SIG_VAL_MAX;       /* 第 4 页数值 */

/** @brief 百分比 → "60%"（无前导零） */
static void ui_fmt_pct(char *txt, uint8_t pct)
{
    uint8_t n = 0;

    if (pct >= 100) {
        txt[n++] = '1';
        pct = (uint8_t)(pct - 100);
    }
    if (pct >= 10 || n > 0) {                  /* 已写过百位则十位必写（含 100 的 0） */
        txt[n++] = (char)('0' + (pct / 10) % 10);
    }
    txt[n++] = (char)('0' + pct % 10);
    txt[n++] = '%';
    txt[n] = '\0';
}

/** @brief 百分比 → 进度条帧号（越界夹到末帧） */
static uint8_t pro_frame_of(uint8_t pct)
{
    uint8_t f = (uint8_t)(pct / PRO_PCT_STEP);

    return (f >= PRO_FRAME_NUM) ? (uint8_t)(PRO_FRAME_NUM - 1) : f;
}

/** @brief 0~1000 → 十进制（无前导零），返回字符数 */
static uint8_t ui_fmt_val(char *txt, uint16_t v)
{
    static const uint16_t div_tab[4] = { 1000, 100, 10, 1 };
    uint8_t n = 0;
    uint8_t i;

    for (i = 0; i < 4; i++) {
        uint8_t d = (uint8_t)((v / div_tab[i]) % 10);

        if (d != 0 || n > 0 || i == 3) {       /* 跳过高位 0；0 本身仍输出一个 '0' */
            txt[n++] = (char)('0' + d);
        }
    }
    txt[n] = '\0';
    return n;
}

/** @brief 数值 → 信息条第 2 项："1000Hz"；0 给 "Null"（与第 4 页大字同口径） */
static void ui_fmt_hz(char *txt, uint16_t v)
{
    uint8_t n;

    if (v == 0) {
        for (n = 0; UI_NULL_TXT[n] != '\0'; n++) {
            txt[n] = UI_NULL_TXT[n];
        }
        txt[n] = '\0';
        return;
    }
    n = ui_fmt_val(txt, v);
    txt[n++] = 'H';
    txt[n++] = 'z';
    txt[n] = '\0';
}

/* -------- 信息条：横向三等分，每项在自己格子里水平居中 -------- */
#define INFO_COL_NUM   (3)
#define INFO_COL_W     ((int16_t)(BSP_LCD_WIDTH / INFO_COL_NUM))

/** @brief 信息条文本顶边（由"下划线落在 INFO_RULE_Y"反推） */
static int16_t ui_info_y(void)
{
    return (int16_t)(INFO_RULE_Y
                     - (harmony_os_10.line_height - harmony_os_10.base_line + 1));
}

/** @brief 画一项（只画不清：文案会变长的格子要用 ui_info_item_redraw） */
static void ui_info_item(uint8_t idx, const char *txt, uint16_t fg)
{
    int16_t y  = ui_info_y();
    int16_t tw = mui_text_width(txt, &harmony_os_10, 1);
    int16_t x  = (int16_t)(idx * INFO_COL_W + (INFO_COL_W - tw) / 2);

    mui_text_draw_ex(x, y, txt, &harmony_os_10, fg, UI_BG, 1, MUI_TEXT_UNDERLINE);
}

/** @brief 清掉整格再画（文案变宽/变窄、ON↔OFF 之类必须走它） */
static void ui_info_item_redraw(uint8_t idx, const char *txt, uint16_t fg)
{
    mui_rect_fill((int16_t)(idx * INFO_COL_W), ui_info_y(), INFO_COL_W,
                  mui_text_height(&harmony_os_10, 1, MUI_TEXT_UNDERLINE), UI_BG);
    ui_info_item(idx, txt, fg);
}

/** @brief 画信息条三项：百分比 / 数值 / 开关（前两项与页参数同源） */
static void ui_draw_info_bar(void)
{
    char pct[8];
    char hz[10];

    ui_fmt_pct(pct, s_percent);
    ui_info_item(0, pct, MUI_RGB565(0xa0, 0xa0, 0xa0));
    ui_fmt_hz(hz, s_sig_val);
    ui_info_item(1, hz, MUI_RGB565(0xa0, 0xa0, 0xa0));
    ui_info_item(2, s_power_on ? "ON" : "OFF",
                 s_power_on ? MUI_BLACK : THEME_DEEP_BLUE);
}

/** @brief 设置开关：开启从当前值继续递减（不重置时间），关闭停在当前值 */
static void ui_power_set(uint8_t on)
{
    s_power_on = on ? 1 : 0;
    if (s_power_on) {
        s_timer_tick = bsp_system_tick_ms();
    }
    ui_clock_colon_reset();

    if (s_page != 0) {
        return;
    }
    ui_info_item_redraw(2, s_power_on ? "ON" : "OFF",
                        s_power_on ? MUI_BLACK : THEME_DEEP_BLUE);
    /* 不要清 s_clock.drawn：首绘是稀疏绘制（不铺底色），会把旧数字从新数字空腔里透出来 */
    ui_clock_update(s_remain_s);
}

/** @brief 切换开关（首页按 k3） */
static void ui_power_toggle(void)
{
    ui_power_set((uint8_t)(!s_power_on));
}

/** @brief 首页 +/-：调倒计时（±1 分钟，秒归零，夹 0~99:59；运行中重新起算 1 秒） */
static void ui_timer_adjust(int dir)
{
    int32_t v = (int32_t)s_remain_s + dir * TIMER_STEP_SEC;

    v = v / TIMER_STEP_SEC * TIMER_STEP_SEC;   /* 秒归零：对齐到整分钟 */
    if (v < 0) {
        v = 0;
    }
    if (v > TIMER_MAX_SEC) {
        v = TIMER_MAX_SEC;
    }
    if ((uint16_t)v == s_remain_s) {
        return;                                /* 已在边界 */
    }
    s_remain_s = (uint16_t)v;

    if (s_power_on) {
        s_timer_tick = bsp_system_tick_ms();
    }
    if (s_page == 0) {
        ui_clock_update(s_remain_s);
    }
}

/** @brief 第 1 页：Mode 按钮 + 大号时间 + 信息条（内容区已清空） */
static void ui_page_draw_home(void)
{
    mui_button_draw(&s_mode);
    ui_draw_info_bar();
    s_clock.drawn = 0;           /* 屏幕已清空，走"整串直接画" */
    ui_clock_update(s_remain_s);
    ui_clock_colon_reset();
}

/** @brief 第 3 页：百分比文字（先清该行再画，值宽窄变化不留残影） */
static void ui_pro_text_draw(void)
{
    char txt[8];
    int16_t tw;
    int16_t x;

    ui_fmt_pct(txt, s_percent);
    tw = mui_text_width(txt, &harmony_os_light_20, 1);
    x  = (int16_t)(PRO_TEXT_X + (PRO_TEXT_W - tw) / 2);   /* 块内居中：数字中心不动 */

    mui_rect_fill(PRO_TEXT_X, PRO_VAL_Y, PRO_TEXT_W,
                  harmony_os_light_20.line_height, UI_BG);
    mui_text_draw(x, PRO_VAL_Y, txt, &harmony_os_light_20, MUI_BLACK, UI_BG, 1);
}

/** @brief 第 3 页："Power" 标签（静态，进页画一次） */
static void ui_pro_label_draw(void)
{
    int16_t tw = mui_text_width(PRO_LABEL, &harmony_os_10, 1);
    int16_t x  = (int16_t)(PRO_TEXT_X + (PRO_TEXT_W - tw) / 2);

    mui_text_draw(x, PRO_LBL_Y, PRO_LABEL, &harmony_os_10, MUI_DARKGREY, UI_BG, 1);
}

/** @brief 第 3 页：进度条 + 百分比 + "Power" */
static void ui_page_draw_progress(void)
{
    const mui_image_t *img = s_pro_frames[pro_frame_of(s_percent)];

    mui_image_draw(PRO_BAR_X, PRO_BAR_Y, img->w, img->h, img->data);
    ui_pro_text_draw();
    ui_pro_label_draw();
}

/** @brief 第 3 页 +/-：百分比 ±10%（夹 0~100），同时更新首页信息条第 1 项 */
static void ui_pro_step(int dir)
{
    int16_t v = (int16_t)(s_percent + dir * PRO_PCT_STEP);

    if (v < 0) {
        v = 0;
    }
    if (v > PRO_PCT_MAX) {
        v = PRO_PCT_MAX;
    }
    if ((uint8_t)v == s_percent) {
        return;                        /* 已在边界 */
    }
    s_percent = (uint8_t)v;

    if (s_page == 2) {                 /* "Power" 是静态的，不用重画 */
        ui_pro_text_draw();
        mui_image_draw(PRO_BAR_X, PRO_BAR_Y, pro_0.w, pro_0.h,
                       s_pro_frames[pro_frame_of(s_percent)]->data);
    }
    if (s_page == 0) {                 /* 首页信息条第 1 项（文字宽度会变 → 整格重画） */
        char pct[8];

        ui_fmt_pct(pct, s_percent);
        ui_info_item_redraw(0, pct, MUI_RGB565(0xa0, 0xa0, 0xa0));
    }
}

/* -------- 第 2 页：模式列表（内容区纵向三等分，每项格内居中） -------- */

/** @brief 第 idx 项格子顶边（idx = MODE_LIST_NUM 时即内容区底边） */
static int16_t ui_mode_item_top(uint8_t idx)
{
    return (int16_t)(UI_CONTENT_TOP + UI_CONTENT_H * idx / MODE_LIST_NUM);
}

/** @brief 画一项：先清该项文字带（文本宽 + 2）再按选中态重画 */
static void ui_mode_item_draw(uint8_t idx)
{
    int16_t top = ui_mode_item_top(idx);
    int16_t h   = (int16_t)(ui_mode_item_top((uint8_t)(idx + 1)) - top);
    const char *name = s_mode_names[idx];
    int16_t tw = mui_text_width(name, &harmony_os_black_14, 1);
    int16_t tx = (int16_t)((BSP_LCD_WIDTH - tw) / 2);
    int16_t ty = (int16_t)(top + (h - harmony_os_black_14.line_height) / 2);
    uint8_t sel = (uint8_t)(idx == s_mode_sel);

    mui_rect_fill((int16_t)(tx - 1), ty, (int16_t)(tw + 2),
                  harmony_os_black_14.line_height, UI_BG);
    mui_text_draw(tx, ty, name, &harmony_os_black_14,
                  sel ? MODE_COLOR_ON : MODE_COLOR_OFF, UI_BG, 1);
}

/** @brief 画整张模式列表 */
static void ui_page_draw_modes(void)
{
    uint8_t i;

    for (i = 0; i < MODE_LIST_NUM; i++) {
        ui_mode_item_draw(i);
    }
}

/** @brief 第 2 页 +/-：循环换模式（只重画选中态变过的两项） */
static void ui_mode_step(int dir)
{
    uint8_t old = s_mode_sel;

    s_mode_sel = (uint8_t)((old + MODE_LIST_NUM + dir) % MODE_LIST_NUM);

    mui_button_set_text(&s_mode, s_mode_titles[s_mode_sel]);
    if (s_page == 0) {
        mui_button_draw(&s_mode);
    }
    if (s_page == 1) {
        ui_mode_item_draw(old);
        ui_mode_item_draw(s_mode_sel);
    }
}

/* -------- 第 4 页：数值 + 信号强度条（6 格几何绘制，0 资源） --------
 * 区间 → 点亮格数：0 → 0 格（"Null" 全灭）｜1-99 → 1｜100-299 → 2｜300-499 → 3
 *                  ｜500-699 → 4｜700-899 → 5｜900-1000 → 6
 * 档位由 sig_level_of(值) 算出，不另存状态；步进 1 且首尾循环。
 */
#define SIG_BAR_NUM      (6)
#define SIG_BAR_W        (10)
#define SIG_BAR_STEP     (20)
#define SIG_BAR_X0       (8)
#define SIG_BAR_BOTTOM   (103)                 /* 所有格底边（只往上长高） */
#define SIG_BAR_H_MIN    (25)
#define SIG_BAR_H_STEP   (9)
#define SIG_BAR_R        (4)
#define SIG_COLOR_ON     THEME_DEEP_BLUE
#define SIG_COLOR_OFF    MUI_LIGHTGREY
#define SIG_VAL_STEP     (1)
#define SIG_TEXT_X       (5)                   /* 大字左对齐（位数变化时左边界不动） */
#define SIG_TEXT_Y       (32)
#define SIG_TEXT_W       (46)                  /* = "1000" 实测宽 44 + 2 */
#define SIG_UNIT_X       (7)
#define SIG_UNIT_Y       (55)

/** @brief 数值 → 点亮格数（0 表示"Null"：全灭） */
static uint8_t sig_level_of(uint16_t val)
{
    static const uint16_t th[SIG_BAR_NUM - 1] = { 100, 300, 500, 700, 900 };
    uint8_t lv = 1;
    uint8_t i;

    if (val == 0) {
        return 0;
    }
    for (i = 0; i < SIG_BAR_NUM - 1; i++) {
        if (val >= th[i]) {
            lv++;
        }
    }
    return lv;
}

/** @brief 第 4 页大字：数值；0 → "Null"（先清固定文字带，位数变化不留残影） */
static void ui_sig_text_draw(void)
{
    char txt[6];

    mui_rect_fill(SIG_TEXT_X, SIG_TEXT_Y, SIG_TEXT_W,
                  harmony_os_light_20.line_height, UI_BG);
    if (s_sig_val == 0) {
        mui_text_draw(SIG_TEXT_X, SIG_TEXT_Y, UI_NULL_TXT,
                      &harmony_os_light_20, MUI_BLACK, UI_BG, 1);
        return;
    }
    ui_fmt_val(txt, s_sig_val);
    mui_text_draw(SIG_TEXT_X, SIG_TEXT_Y, txt,
                  &harmony_os_light_20, MUI_BLACK, UI_BG, 1);
}

/** @brief 第 i 格几何：输出格高，返回顶边 y */
static int16_t sig_bar_top(uint8_t i, int16_t *h)
{
    *h = (int16_t)(SIG_BAR_H_MIN + i * SIG_BAR_H_STEP);
    return (int16_t)(SIG_BAR_BOTTOM + 1 - *h);
}

/** @brief 画第 i 格（颜色按当前档位）；调用方须保证格子里是页底色 */
static void ui_sig_bar_draw(uint8_t i)
{
    int16_t  h;
    int16_t  y     = sig_bar_top(i, &h);
    uint16_t color = (i < sig_level_of(s_sig_val)) ? SIG_COLOR_ON : SIG_COLOR_OFF;

    mui_round_rect_fill_aa((int16_t)(SIG_BAR_X0 + i * SIG_BAR_STEP), y,
                           SIG_BAR_W, h, SIG_BAR_R, color, UI_BG);
}

/** @brief 原地重画第 i 格：先按页底色清包围盒再画 */
static void ui_sig_bar_repaint(uint8_t i)
{
    int16_t h;
    int16_t y = sig_bar_top(i, &h);

    /* 必须清底：AA 的混合基底是回读当前像素，直接重画会拿"旧色"当基底，
     * 蓝转灰时圆角弧边会残留一圈蓝（代价 ≤ 10x70 px，仅跨段变色时发生） */
    mui_rect_fill((int16_t)(SIG_BAR_X0 + i * SIG_BAR_STEP), y, SIG_BAR_W, h, UI_BG);
    ui_sig_bar_draw(i);
}

/** @brief 第 4 页：大字 + 单位 + 6 格 */
static void ui_page_draw_signal(void)
{
    uint8_t i;

    ui_sig_text_draw();
    mui_text_draw(SIG_UNIT_X, SIG_UNIT_Y, "Hz",
                  &harmony_os_10, MUI_DARKGREY, UI_BG, 1);
    for (i = 0; i < SIG_BAR_NUM; i++) {
        ui_sig_bar_draw(i);
    }
}

/** @brief 第 4 页 +/-：数值 ±1 且首尾循环，重画变过档位的格 + 大字 */
static void ui_sig_val_step(int dir)
{
    uint8_t old_lv = sig_level_of(s_sig_val);
    uint8_t new_lv;
    uint8_t lo, hi, i;
    int32_t v = (int32_t)s_sig_val + dir * SIG_VAL_STEP;

    if (v < 0) {
        v = SIG_VAL_MAX;                       /* 0（Null）再减 → 1000 */
    } else if (v > SIG_VAL_MAX) {
        v = 0;                                 /* 1000 再加 → 0（Null） */
    }
    s_sig_val = (uint16_t)v;
    new_lv = sig_level_of(s_sig_val);

    if (s_page == 0) {                         /* 首页信息条第 2 项同源 */
        char hz[10];

        ui_fmt_hz(hz, s_sig_val);
        ui_info_item_redraw(1, hz, MUI_RGB565(0xa0, 0xa0, 0xa0));
        return;
    }
    if (s_page != 3) {
        return;                                /* 其它页：只改数值 */
    }

    ui_sig_text_draw();
    lo = (uint8_t)((old_lv < new_lv) ? old_lv : new_lv);
    hi = (uint8_t)((old_lv < new_lv) ? new_lv : old_lv);
    for (i = lo; i < hi; i++) {
        ui_sig_bar_repaint(i);
    }
}

/** @brief 画指定页内容区（调用前内容区须已清空） */
static void ui_page_draw(uint8_t page)
{
    switch (page) {
    case 0:
        ui_page_draw_home();
        break;
    case 1:
        ui_page_draw_modes();
        break;
    case 2:
        ui_page_draw_progress();
        break;
    case 3:
        ui_page_draw_signal();
        break;
    default:
        break;
    }
}

/** @brief 切页：清内容区 → 重画（公共区不动） */
static void ui_page_show(uint8_t page)
{
    if (page >= UI_PAGE_NUM) {
        return;
    }
    s_page = page;
    mui_rect_fill(0, UI_CONTENT_TOP, BSP_LCD_WIDTH, UI_CONTENT_H, UI_BG);
    ui_page_draw(page);
}

/** @brief 切页 + 同步底部四键选中态 */
static void ui_page_select(uint8_t page)
{
    uint8_t k;

    for (k = 0; k < UI_BTN_NUM; k++) {
        mui_button_set_pressed(&s_btns[k], (uint8_t)(k == page));
    }
    ui_page_show(page);
    app_ui_draw_buttons();
}

void app_ui_draw_buttons(void)
{
    uint8_t i;

    for (i = 0; i < UI_BTN_NUM; i++) {
        mui_button_draw(&s_btns[i]);
    }
}

/** @brief 物理按键：按 s_key_fn[] 分派（只在按下沿动作） */
void app_ui_key(uint8_t key_id, uint8_t down)
{
    if (key_id >= KEY_FN_NUM || !down) {
        return;
    }

    switch (s_key_fn[key_id]) {
    case KEY_FN_PAGE_NEXT:
        ui_page_select((uint8_t)((s_page + 1) % UI_PAGE_NUM));
        break;

    case KEY_FN_MODE_NEXT:
        ui_mode_step(1);
        break;

    case KEY_FN_SIGNAL_UP:
        if (s_page == 0) {
            ui_timer_adjust(1);
        } else if (s_page == 1) {
            ui_mode_step(1);
        } else if (s_page == 2) {
            ui_pro_step(1);
        } else {
            ui_sig_val_step(1);
        }
        break;

    case KEY_FN_SIGNAL_DOWN:
        if (s_page == 0) {
            ui_timer_adjust(-1);
        } else if (s_page == 1) {
            ui_mode_step(-1);
        } else if (s_page == 2) {
            ui_pro_step(-1);
        } else {
            ui_sig_val_step(-1);
        }
        break;

    case KEY_FN_HOME_POWER:
        if (s_page != 0) {
            ui_page_select(0);
        } else {
            ui_power_toggle();
        }
        break;

    default:
        break;
    }

    mui_screen_flush();
}

/** @brief 消费触摸事件：底部四键切页、Mode 键切模式（事件驱动，无事件不写屏） */
static void ui_poll_input(void)
{
    mui_touch_event_t ev;
    uint8_t i;

    while ((ev = mui_touch_poll()) != MUI_TOUCH_NONE) {
        for (i = 0; i < UI_BTN_NUM; i++) {
            if (mui_button_touch(&s_btns[i], ev)) {
                ui_page_select(i);
            }
        }
        if (mui_button_touch(&s_mode, ev)) {
            if (s_page == 0) {             /* Mode 按钮只在首页可见 */
                mui_button_draw(&s_mode);
            }
        }
        if (ev != MUI_TOUCH_MOVE) {        /* MOVE 不改按钮状态，不必重画 */
            app_ui_draw_buttons();
        }
    }
}

void app_ui_init(void)
{
    int16_t by = BTN_TOP;

    mui_init(BSP_LCD_WIDTH, BSP_LCD_HEIGHT);   /* 内部调 mui_port_init() → bsp_lcd_init() */
    mui_screen_clear(UI_BG);

    /* 公共区：logo + 分隔线 */
    mui_image_draw((int16_t)((BSP_LCD_WIDTH - logo.w) / 2), 4,
                   logo.w, logo.h, logo.data);
    mui_hline_draw(4, 20, 120, MUI_DARKGREY);

    /* 公共区：底部四键 */
    mui_button_init(&s_btns[0], BTN_MARGIN + 0 * (BTN_W + BTN_GAP), by, BTN_W, BTN_H,
                    &s_btn_style, NULL, &g12x12_1);
    mui_button_init(&s_btns[1], BTN_MARGIN + 1 * (BTN_W + BTN_GAP), by, BTN_W, BTN_H,
                    &s_btn_style, NULL, &g12x12_2);
    mui_button_init(&s_btns[2], BTN_MARGIN + 2 * (BTN_W + BTN_GAP), by, BTN_W, BTN_H,
                    &s_btn_style, NULL, &g12x12_3);
    mui_button_init(&s_btns[3], BTN_MARGIN + 3 * (BTN_W + BTN_GAP), by, BTN_W, BTN_H,
                    &s_btn_style, NULL, &g12x12_4);

    mui_button_set_pressed(&s_btns[0], 1);
    app_ui_draw_buttons();

    /* 第 1 页中部对象（内容由 ui_page_draw_home 画） */
    mui_button_init(&s_mode, MODE_X, MODE_Y, MODE_W, BTN_H,
                    &s_btn_style, NULL, NULL);
    mui_button_set_font(&s_mode, &harmony_os_10);
    mui_button_set_text(&s_mode, s_mode_titles[s_mode_sel]);
    mui_button_set_pressed(&s_mode, 1);
    mui_button_set_latch(&s_mode, 1);

    ui_clock_layout();
    ui_page_show(0);

    mui_screen_flush();
}

void app_ui_frame(void)
{
    uint32_t ms = bsp_system_tick_ms();

    ui_poll_input();

    /* 倒计时：开关开着才走（不在首页只更新变量，不刷屏） */
    if (s_power_on) {
        uint8_t ticked = 0;

        while ((ms - s_timer_tick) >= 1000) {   /* 用 +=1000 累计，避免慢帧丢秒 */
            s_timer_tick += 1000;
            ticked = 1;
            if (s_remain_s > 0) {
                s_remain_s--;
            }
            if (s_remain_s == 0) {
                ui_power_set(0);                /* 到 0 自动关（停在 00:00） */
                break;
            }
        }
        if (ticked && s_page == 0) {
            ui_clock_update(s_remain_s);
        }
    }

    /* 首页：运行中让冒号闪烁（停止时由 ui_clock_colon_reset 保证常亮） */
    if (s_page == 0 && s_power_on && (ms - s_clock_colon_tick) >= CLOCK_COLON_BLINK_MS) {
        s_clock_colon_tick = ms;
        s_clock_colon_on = (uint8_t)(!s_clock_colon_on);
        ui_clock_colon_apply();
    }

    mui_screen_flush();
}
