/**
 * @file app_ui.c
 * @brief 应用界面：MUI demo（128x128 屏适配版）
 *
 * 演示内容：
 *   1. 品牌 logo（白底彩色位图，顶部居中）
 *   2. 首页中部大号倒计时（mm:ss，38px；开关开启时每秒递减 + 冒号闪烁，上电默认 10:00）
 *   3. 底部四等分按钮 + Mode 按钮（mui_button_t 对象，OO 风格：init / set_xxx / draw）
 *   4. 第 2 页：模式列表（ALL / RED / NIR 纵向三等分居中，选中项高亮；+/- 换模式），
 *      当前模式同时显示在首页 Mode 按钮上（"Mode:xxx"）
 *   5. 第 3 页：进度百分比 + 图片版进度条（+/- 以 10% 步进；帧号 = 百分比/10，
 *      首页信息条第 1 项的百分比与它同源，不再写死 60%）
 *   6. 物理按键接入：app_ui_key() 按 s_key_fn 表分派键号到功能，
 *      默认单个键循环切页（换键/换功能只改那张表）；鼠标与触摸屏另走触摸链路；
 *      +/- 是"页内参数键"：首页调倒计时、第 2 页换模式、第 3 页调百分比、第 4 页调数值
 *      （±1，0~1000 首尾循环：1000 加→0/Null、0 减→1000；信号条档位由数值所在区间决定）
 *   8. 信息条第 1、2 项与页参数**同源**：第 1 项 = 第 3 页百分比，第 2 项 = 第 4 页数值
 *      （显示 "1000Hz"；数值为 0 时与第 4 页大字一样显示 "Null"）
 *   7. 页面（Tab）：底部四键对应 4 个页面。屏幕分三层——上部 logo+分隔线、
 *      中部内容区、底部四键；只有中部内容区随页面切换（切页 = 清内容区 + 重画）
 *
 * 刷新策略（直绘方案不闪烁的关键）：
 *   MUI 是"直绘 + 无帧缓冲"：屏幕内容不会自动保持/重放，谁被擦掉就要重画谁。
 *   因此不能每帧整屏清屏重画（那样整屏会持续闪烁），而是：
 *     - 静态部分（背景/logo/分隔线/按钮）只在 app_ui_init() 画一次；
 *     - 状态变化时用 OO API 更新按钮对象并重画（app_ui_draw_buttons）；
 *     - 时间用 mui_label 对象，每秒 set_text 整格覆盖刷新（无擦白阶段，不闪烁）。
 *
 *   输出后端由库的 mui_conf.h 编译期选择（直绘/单缓冲/双缓冲）：
 *   缓冲后端下绘制只改内存，故在 app_ui_init()、app_ui_frame()、app_ui_key()
 *   的末尾各调用一次 mui_screen_flush() 统一推屏；直绘后端下它是空操作，
 *   本文件无需为两种后端写两套逻辑。
 */

#include "app_ui.h"
#include "mui.h"
#include "mui_button.h"     /* 按钮 API（新版已从 mui.h 拆到独立头文件） */
#include "mui_label.h"      /* 文本标签对象（自动擦旧画新） */
#include "mui_font.h"
#include "img_logo.h"
#include "bsp_lcd.h"
#include "bsp_system.h"
#include "mui_font_harmony_os_10.h"
#include "mui_font_harmony_os_light_20.h"
// #include "mui_font_harmony_os_32.h"
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

/* -------- 界面底色（换主题只改这一行） --------
 * 直绘体系下"背景色"是应用层与库的约定，不是读屏得来的：
 *   所有文字的抗锯齿混合、label 的擦除/覆盖铺底都依赖它 —— 必须与真实屏幕底色一致。
 * 例：换黑底 → #define UI_BG MUI_BLACK，再把各前景色调成深色底上可见的浅色即可。
 */
#define UI_BG                MUI_WHITE
#define THEME_DEEP_BLUE      MUI_RGB565(0, 109, 249)      /* 主题深蓝色 */
#define THEME_BLUE           MUI_RGB565(4, 170, 244)      /* 按钮未按下的灰蓝色 */
#define UI_NULL_TXT          "Null"    /* 无读数占位文字（第 4 页大字 + 首页信息条第 2 项共用同一口径） */

/* -------- 按钮样式 -------- */
static const mui_button_style_t s_btn_style = {
    .bg = THEME_BLUE,
    .bg_press = THEME_DEEP_BLUE,
    .fg = MUI_WHITE,
    .border = MUI_WHITE,
    .shape = MUI_BUTTON_SHAPE_ROUND,
    .radius = 3
};


/* -------- 底部按钮对象 -------- */
#define UI_BTN_NUM  4
static mui_button_t s_btns[UI_BTN_NUM];

#define BTN_MARGIN	(2)
#define BTN_GAP		(1)  /* 按钮间距 */
#define BTN_H		(15) /* 按钮高 */
#define BTN_W		((int16_t)((BSP_LCD_WIDTH - 2 * BTN_MARGIN - 3 * BTN_GAP) / 4)) /* 按钮高 */
#define BTN_TOP		((int16_t)(BSP_LCD_HEIGHT - BTN_MARGIN - BTN_H))                  /* 底部键顶边 */

/* -------- 页面（Tab）与内容区 --------
 * 屏幕分三层：上部 logo + 分隔线、中部内容区、底部四键。
 * 只有中部内容区随页面切换，上下两层固定不动。
 */
#define UI_PAGE_NUM			(4)
#define UI_CONTENT_TOP		(22)                              /* 内容区顶边（分隔线之下） */
#define UI_CONTENT_BOTTOM	(BTN_TOP)                         /* 内容区底边（底部按钮之上） */
#define UI_CONTENT_H		((int16_t)(UI_CONTENT_BOTTOM - UI_CONTENT_TOP))
#define INFO_RULE_Y			((int16_t)(BSP_LCD_HEIGHT - 25))  /* 信息条三项下划线所在行（y） */

/* -------- 模式对象 -------- */
#define MODE_X		((int16_t)((BSP_LCD_WIDTH - 60) / 2))
#define MODE_Y		(28)
#define MODE_W		(60)
static mui_button_t s_mode;

/* -------- 模式（Mode）：首页 MODE 按钮与第 2 页列表共用同一份状态 --------
 * 这是"测量模式"（ALL = 全光谱 / RED = 红光 / NIR = 近红外），不是按钮控件：
 *   首页 MODE 按钮文案 = s_mode_titles[当前模式]（即 "Mode:xxx"）
 *   第 2 页列表       = s_mode_names[] 竖排三项，选中项高亮
 * 换模式只改 s_mode_sel（唯一真值来源），两处显示各自按它重画。
 */
#define MODE_LIST_NUM   (3)
#define MODE_COLOR_ON   THEME_DEEP_BLUE                    /* 选中：主题深蓝 */
#define MODE_COLOR_OFF  MUI_BLACK

static const char *const s_mode_names[MODE_LIST_NUM]  = { "ALL", "RED", "NIR" };
static const char *const s_mode_titles[MODE_LIST_NUM] = { "Mode:ALL", "Mode:RED", "Mode:NIR" };
static uint8_t s_mode_sel = 1;    /* 当前模式下标（默认 RED，与首页按钮初始文案一致） */

/* -------- 物理按键功能分配 --------
 * 数组下标 = 键号（0~3 = 底部四键，4 = Mode 键），值 = 该键的功能。
 * 换键或换功能只改这张表，分派逻辑不用动：
 *   例 1：把切页键从 0 号改到 2 号 —— 把 KEY_FN_PAGE_NEXT 挪到第 3 行
 *   例 2：某个键不响应 —— 该行写 KEY_FN_NONE
 */
typedef enum {
        KEY_FN_HOME_POWER = 0,    /**< 返回首页；已经在首页则切换开关状态（ON/OFF） */

    KEY_FN_PAGE_NEXT,     /**< 循环切换页面：页1→页2→页3→页4→页1 */
    KEY_FN_MODE_NEXT,     /**< 换下一个模式（ALL→RED→NIR→ALL；当前未分配，可挂到任意键） */
    KEY_FN_SIGNAL_UP,     /**< 参数 +1（首页 = 倒计时 +1 分；第 2 页 = 换模式；第 3 页 = 百分比 +10%；其余页 = 档位 +1） */
    KEY_FN_SIGNAL_DOWN,   /**< 参数 -1（同上） */
    KEY_FN_NONE      /**< 不响应 */
} app_key_fn_t;

/* 键号（0~3 = 底部四键，4 = 原 Mode 键）→ 功能。换键/换功能只改这张表 */
static const uint8_t s_key_fn[] = {
    KEY_FN_NONE,          /* k4（键 3）：未分配 */
    KEY_FN_PAGE_NEXT,     /* k1（键 0）：切换页面 */    
    KEY_FN_SIGNAL_UP,     /* k5（键 4）：加 */
    KEY_FN_HOME_POWER,    /* k3（键 2）：返回首页 / 已在首页则开关切换 */
    KEY_FN_SIGNAL_DOWN,   /* k2（键 1）：减 */
};
#define KEY_FN_NUM  ((uint8_t)(sizeof(s_key_fn) / sizeof(s_key_fn[0])))

/* -------- 中部大号时间：倒计时（mm:ss） --------
 * 显示值 = 剩余秒数，受首页的开关（k3）与 +/-（k2/k5）控制：
 *   上电：默认 10:00，开关为**关**（显示不递减、冒号常亮）
 *   开启：从当前值继续递减（**不重置**），冒号闪烁；关闭：停在当前值
 *   首页 +/-：每次 ±1 分钟调剩余时间（其它页 +/- 仍是信号档位）
 *   到 0：自动关闭（停在 00:00）
 */
#define TIMER_DEFAULT_SEC   (10 * 60)         /* 上电默认时间：10:00 */
#define TIMER_STEP_SEC      (60)              /* 首页 +/- 每按一次的步长（1 分钟） */
#define TIMER_MAX_SEC       (99 * 60)         /* 显示 mm:ss 最多 99 分 */

#define UI_CLOCK_TOP     22                             
#define UI_CLOCK_BOTTOM  (BSP_LCD_HEIGHT - 2 - 15)       

static const char UI_CLOCK_REF[] = "88:88";   /* 基准文本：定水平居中位置（防跳动） */
static mui_label_t s_clock;                   /* 时间标签对象（内部缓冲 + 增量刷新） */
static uint8_t  s_power_on = 0;                       /* 开关状态（0 = OFF，1 = ON） */
static uint16_t s_remain_s = TIMER_DEFAULT_SEC;       /* 剩余秒数（即显示值） */
static uint32_t s_timer_tick;                         /* 上一次减 1 秒的时间（ms） */

/* -------- 冒号闪烁 --------
 * 时间文本由 label 增量刷新（每秒只重画变化的数字），冒号另外单独开关：
 *   亮 = 用 mui_text_draw_char 画一个 ':'（与 label 同字体、同位置）
 *   灭 = 用底色填掉冒号所在的那一格
 * 只动冒号一格（格宽 x 行高），比整串重画省得多，也不会碰到两侧数字 ——
 * 能整格填底色的前提是"数字墨迹限于自己的步进宽内"（本字体实测 max(ofs_x+box_w-1)
 * = 步进宽-1，故冒号格与左右数字格不重叠）。
 */
#define CLOCK_COLON_BLINK_MS   (500)          /* 半周期：亮 0.5s / 灭 0.5s */

static int16_t  s_clock_colon_x;              /* 冒号格左边界（layout 时算好） */
static int16_t  s_clock_colon_w;              /* 冒号格宽（= ':' 的步进宽） */
static uint8_t  s_clock_colon_on = 1;         /* 当前是否显示冒号 */
static uint32_t s_clock_colon_tick;           /* 上次翻转时间（ms） */

/**
 * @brief 初始化时间标签：按基准文本宽度水平居中、在 logo 与按钮之间垂直居中
 * @note  label 为左对齐；mm:ss 是定长 5 字符，位置固定故不会跳动
 */
static void ui_clock_layout(void)
{
    int16_t tw = mui_text_width(UI_CLOCK_REF, &harmony_os_medium_38, 1);
    int16_t tx = (int16_t)((BSP_LCD_WIDTH - tw) / 2);
    int16_t ty = (int16_t)(UI_CLOCK_TOP
                           + (UI_CLOCK_BOTTOM - UI_CLOCK_TOP
                              - harmony_os_medium_38.line_height) / 2);

    mui_label_init(&s_clock, tx, ty, &harmony_os_medium_38, MUI_BLACK, UI_BG, 1);

    /* 冒号是第 3 个字符：格左 = 文本左 + "mm" 两字符宽；格宽 = ':' 的步进宽 */
    s_clock_colon_x = (int16_t)(tx + mui_text_width("88", &harmony_os_medium_38, 1));
    s_clock_colon_w = mui_text_width(":", &harmony_os_medium_38, 1);
}

/** @brief 按当前相位画/抹冒号（不动其它内容） */
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

/** @brief 冒号复位成"亮"并重新计时（切页/重画后调用），立刻把冒号补回来 */
static void ui_clock_colon_reset(void)
{
    s_clock_colon_on = 1;
    s_clock_colon_tick = bsp_system_tick_ms();
    ui_clock_colon_apply();
}

/**
 * @brief 把秒数格式化成 mm:ss（5 字符）
 * @param txt 输出缓冲（至少 6 字节）
 * @param sec 秒数（倒计时的剩余值）
 */
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

/**
 * @brief 更新大号时间显示（mm:ss）：label 自动"从差异点擦旧画新"
 * @param sec 剩余秒数（倒计时值；停止时传 s_remain_s 即"停在当前值"）
 * @note  例如 09:59 → 09:58 只重画末两位，写屏量与闪烁都最小
 */
static void ui_clock_update(uint32_t sec)
{
    char txt[6];

    ui_fmt_mmss(txt, sec);
    mui_label_set_text(&s_clock, txt);
    /* 增量重画可能扫到冒号格（如分钟进位时从差异点重画到尾），按当前相位补回 */
    ui_clock_colon_apply();
}

/* -------- 页面（Tab）内容：只画中部内容区，公共区由 init 负责 -------- */

static uint8_t s_page = 0;              /**< 当前页（0~3） */

/* -------- 第 3 页：进度百分比 + 图片版进度条（数值驱动） --------
 * 只保留图片版：胶囊形状与抗锯齿是**烘焙进图片像素**的，静态显示就是一次纯 blit。
 * 原来的 HARD / AA 两列（mui_progressbar 控件 + style.aa 对照）只是用来证明
 * "几何版能 1:1 复刻图片版"，结论已验证并记录，故从本页移出；控件能力仍由单测覆盖
 * （test_progressbar_aa / _track_aa / _full_cover）。
 *
 * **数值驱动**（2026-09-23 用户要求：本页显示百分比、+/- 以 10% 步进、进度条跟着变）：
 *   帧号 = s_percent / PRO_PCT_STEP，与 img_pro_0~10 **一一对应**（实测：pro_0 填充 0 px = 空、
 *   pro_10 填充 2052 px = 满；填充色 0x037F、轨道色 0xD6BA、画布外 0xFFFF = UI_BG）。
 *   原来的"11 帧循环动画"已撤 —— 帧号不再是独立状态，避免出现第二个真值来源。
 *   s_percent 是**唯一真值**：首页信息条第 1 项的百分比也由它驱动。
 *
 * 布局：**进度条 + 6px 间距 + 右侧文字块(48)** 整体横向居中；纵向各自居中于内容区：
 *   条 34x76 垂直居中 → y = 22 + (89-76)/2 = 28（28..103，中心 66）
 *   文字块 = 百分比行（light_20，23 高）+ "power" 小标签（10px 暗灰，11 高）→ 34 高，
 *            同样以 66 为中心：百分比 49..71、"power" 72..82。
 *   文字块按最长串 "100%" 定宽、各值在块内居中 → 值变宽变窄时数字中心不动，视觉不跳。
 */
#define PRO_PCT_STEP   (10)                    /* +/- 的百分比步进 */
#define PRO_PCT_MAX    (100)                   /* 上限（= 满格帧） */
#define PRO_GAP        (6)                     /* 进度条与右侧文字块之间的间距 */
#define PRO_TEXT_W     (48)                    /* 文字块宽 = "100%" 实测宽（light_20：11+11+11+15） */
#define PRO_LABEL      "Power"                 /* 百分比下方的标签（10px 暗灰） */
#define PRO_GROUP_W    ((int16_t)(pro_0.w + PRO_GAP + PRO_TEXT_W))
#define PRO_BAR_X      ((int16_t)((BSP_LCD_WIDTH - PRO_GROUP_W) / 2))   /* 条左边界（组横向居中） */
#define PRO_TEXT_X     ((int16_t)(PRO_BAR_X + pro_0.w + PRO_GAP))       /* 文字块左边界 */
#define PRO_BAR_Y      ((int16_t)(UI_CONTENT_TOP + (UI_CONTENT_H - pro_0.h) / 2))   /* 垂直居中 */
#define PRO_BLOCK_H    ((int16_t)(harmony_os_light_20.line_height + harmony_os_10.line_height))
#define PRO_VAL_Y      ((int16_t)(UI_CONTENT_TOP + (UI_CONTENT_H - PRO_BLOCK_H) / 2)) /* 百分比行框顶 */
#define PRO_LBL_Y      ((int16_t)(PRO_VAL_Y + harmony_os_light_20.line_height))      /* "power" 行框顶 */

static const mui_image_t *const s_pro_frames[] = {
    &pro_0, &pro_1, &pro_2, &pro_3, &pro_4, &pro_5,
    &pro_6, &pro_7, &pro_8, &pro_9, &pro_10
};
#define PRO_FRAME_NUM  ((uint8_t)(sizeof(s_pro_frames) / sizeof(s_pro_frames[0])))

/* -------- 页参数：两个"数值型"页状态的唯一定义处 --------
 * 首页信息条第 1 / 2 项直接显示它们（百分比 / 数值），而信息条在文件里排在两个页面实现之前，
 * 所以这两个状态与数值上限必须定义在这里 —— 页面段落只放各自的几何与绘制。
 */
#define SIG_VAL_MAX      (1000)                /* 第 4 页数值上限（= 第 6 段 / 满格） */
static uint8_t  s_percent = 60;                /* 第 3 页进度百分比（0~100；默认 60） */
static uint16_t s_sig_val = SIG_VAL_MAX;       /**< 第 4 页数值（0~1000；默认满量程） */

/**
 * @brief 把百分比格式化成 "60%" 形式（0~100，无前导零）
 * @param txt 输出缓冲（至少 5 字节）
 * @param pct 百分比 0~100
 */
static void ui_fmt_pct(char *txt, uint8_t pct)
{
    uint8_t n = 0;

    if (pct >= 100) {
        txt[n++] = '1';
        pct = (uint8_t)(pct - 100);
    }
    if (pct >= 10 || n > 0) {          /* 已写过百位则十位必写（含 100 的 0） */
        txt[n++] = (char)('0' + (pct / 10) % 10);
    }
    txt[n++] = (char)('0' + pct % 10);
    txt[n++] = '%';
    txt[n] = '\0';
}

/**
 * @brief 百分比 → 进度条帧号（越界夹到末帧）
 * @param pct 百分比 0~100
 * @return 帧下标 0 ~ PRO_FRAME_NUM-1
 */
static uint8_t pro_frame_of(uint8_t pct)
{
    uint8_t f = (uint8_t)(pct / PRO_PCT_STEP);

    return (f >= PRO_FRAME_NUM) ? (uint8_t)(PRO_FRAME_NUM - 1) : f;
}

/* -------- 数值 → 显示串（首页信息条与第 3 / 4 页共用） -------- */

/**
 * @brief 把 0~1000 格式化成十进制（无前导零）
 * @param txt 输出缓冲（至少 6 字节）
 * @param v   数值 0~1000
 * @return 写入的字符数（不含结尾 '\0'）
 */
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

/**
 * @brief 把数值格式化成信息条第 2 项用的串："1000Hz"；数值为 0 时给 "Null"
 * @param txt 输出缓冲（至少 8 字节）
 * @param v   数值 0~1000
 * @note  0（无读数）与第 4 页大字同口径 —— 都显示 "Null"，不再拼 "0Hz" / "NullHz"。
 */
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

/* -------- 信息条布局：屏幕横向三等分，每项在自己的格子里水平居中 --------
 * 不要用固定左边界（那样文字一变长只能向右长，"1000Hz" 就会把间距顶歪）：
 * 居中靠 mui_text_width 量出宽度后 x = 格左 + (格宽 - 文本宽) / 2，
 * 文字长短怎么变都自动居中。
 */
#define INFO_COL_NUM   (3)
#define INFO_COL_W     ((int16_t)(BSP_LCD_WIDTH / INFO_COL_NUM))   /* 每格宽（128/3 = 42） */

/** @brief 信息条文本顶边（由"下划线落在 INFO_RULE_Y"反推，三项共用一条水平线） */
static int16_t ui_info_y(void)
{
    return (int16_t)(INFO_RULE_Y
                     - (harmony_os_10.line_height - harmony_os_10.base_line + 1));
}

/**
 * @brief 画信息条的一项：在第 idx 格里水平居中（文本 + 下划线装饰）
 * @param idx 格号 0~2（自左向右）
 * @param txt 文本（长度任意，自动居中）
 * @param fg  文字色（下划线与字形同色，跟着一起变色）
 * @note  只画不清 —— 文案会变长变短的格子（如 ON ↔ OFF）要用 ui_info_item_redraw()
 */
static void ui_info_item(uint8_t idx, const char *txt, uint16_t fg)
{
    int16_t y  = ui_info_y();
    int16_t tw = mui_text_width(txt, &harmony_os_10, 1);
    int16_t x  = (int16_t)(idx * INFO_COL_W + (INFO_COL_W - tw) / 2);

    mui_text_draw_ex(x, y, txt, &harmony_os_10, fg, UI_BG, 1, MUI_TEXT_UNDERLINE);
}

/**
 * @brief 重画信息条某一格：先按底色清掉整格（含下划线那一行）再画
 * @note  ON ↔ OFF 这类改文案必须走它：文字变宽/变窄时旧墨与旧下划线都会残留
 */
static void ui_info_item_redraw(uint8_t idx, const char *txt, uint16_t fg)
{
    mui_rect_fill((int16_t)(idx * INFO_COL_W), ui_info_y(), INFO_COL_W,
                  mui_text_height(&harmony_os_10, 1, MUI_TEXT_UNDERLINE), UI_BG);
    ui_info_item(idx, txt, fg);
}

/**
 * @brief 绘制信息条（三项数值，各自居中）
 * @note  三项统一用 MUI_TEXT_UNDERLINE 装饰（线宽 = 文本宽度、颜色 = 文字色）；
 *        第 3 项是开关状态：开 = "ON" + 黑字，关 = "OFF" + 蓝字。
 *        **第 1 / 2 项与页参数同源**：第 1 项 = 第 3 页百分比、第 2 项 = 第 4 页数值
 *        （"1000Hz" / "Null"）—— 这里不写死，进页时按当前值重画。
 */
static void ui_draw_info_bar(void)
{
    char pct[8];
    char hz[10];

    ui_fmt_pct(pct, s_percent);     /* 第 1 项 = 第 3 页的进度百分比（同源，不再写死 60%） */
    ui_info_item(0, pct, MUI_RGB565(0xa0, 0xa0, 0xa0));
    ui_fmt_hz(hz, s_sig_val);       /* 第 2 项 = 第 4 页的数值（同源，不再写死 1Hz） */
    ui_info_item(1, hz, MUI_RGB565(0xa0, 0xa0, 0xa0));
    ui_info_item(2, s_power_on ? "ON" : "OFF",
                 s_power_on ? MUI_BLACK : THEME_DEEP_BLUE);
}

/**
 * @brief 设置开关状态（首页按 k3 时切换）
 * @param on 非 0 = 开启
 * @note  开启 = 从**当前值**继续递减（**不重置时间**，想改时间用首页 +/-），冒号开始闪；
 *        关闭 = 停在当前值（冒号常亮）。不在首页时只改状态，等切回首页由整页重画一次画对。
 */
static void ui_power_set(uint8_t on)
{
    s_power_on = on ? 1 : 0;
    if (s_power_on) {
        s_timer_tick = bsp_system_tick_ms();   /* 从现在开始递减（不重置时间值） */
    }
    ui_clock_colon_reset();                    /* 冒号回到"亮"并重新计时 */

    if (s_page != 0) {
        return;
    }
    ui_info_item_redraw(2, s_power_on ? "ON" : "OFF",
                        s_power_on ? MUI_BLACK : THEME_DEEP_BLUE);
    /* 时间交给 label 自己的增量路径更新，**不要**把 drawn 清 0：
     * 清 0 会走"首绘"分支，而首绘是**稀疏绘制**（只写 alpha != 0 的像素，不铺底色），
     * 屏幕上的旧数字会从新数字的空腔里透出来 —— 实测 00:01 → 00:00（到点自动关）时，
     * 旧 '1' 的竖笔（格内 x≈6~11）正好落进新 '0' 的空腔（格内 x≈8~15）留下残影。
     * 走增量路径：等长同宽 → 整格覆盖（连底色一起铺）→ 旧墨被彻底盖掉；
     * 文本没变（单纯开关切换）则什么都不画，也更省写屏量。
     * 注：drawn = 0 只适用于"屏幕已被清空"的场合（见 ui_page_draw_home）。 */
    ui_clock_update(s_remain_s);
}

/** @brief 切换开关状态（首页按 k3） */
static void ui_power_toggle(void)
{
    ui_power_set((uint8_t)(!s_power_on));
}

/**
 * @brief 首页 +/- 调整倒计时时间
 * @param dir 1 = 加，-1 = 减（步长 TIMER_STEP_SEC = 1 分钟）
 * @note  调整后**秒归零**（落到整分钟）：10:58 +1 = 11:00，10:58 -1 = 09:00。
 *        运行中调整会**重新起算 1 秒**（新设的整分钟完整停留 1 秒再递减，
 *        否则刚好卡在落秒点上时会"设完立刻掉 1 秒"）；夹在 0 ~ 99:59。
 *        停止状态下调整就是"设定下一次要跑的时长"。
 */
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
        return;                                /* 已在边界：画面无变化 */
    }
    s_remain_s = (uint16_t)v;

    if (s_power_on) {
        s_timer_tick = bsp_system_tick_ms();   /* 运行中：重新起算 1 秒节拍 */
    }
    if (s_page == 0) {
        ui_clock_update(s_remain_s);
    }
}

/**
 * @brief 绘制第 1 页（首页）：模式按钮 + 大号时间 + 信息条
 * @note  调用前内容区已清空，故增量刷新的对象必须强制全量重绘：
 *        把 drawn 清 0，set_text/set_value 即走"无擦除"的全量绘制分支
 */
static void ui_page_draw_home(void)
{
    mui_button_draw(&s_mode);
    ui_draw_info_bar();          /* 第 3 项显示当前开关状态（ON 黑 / OFF 蓝） */
    s_clock.drawn = 0;
    ui_clock_update(s_remain_s); /* 显示当前剩余时间（上电 = 10:00） */
    ui_clock_colon_reset();      /* 进页时冒号先亮着并重新计时（运行中随后开始闪） */
}

/**
 * @brief 画第 3 页的百分比文本（文字块内居中）
 * @note  只清"百分比那一行"（PRO_TEXT_W x line_height ≈ 1100 px）：值会变宽变窄
 *        （"0%" 26px ↔ "100%" 48px），按固定块清比"算新旧宽度并集"少一份状态；
 *        清的范围不含下方的 "power" 标签 → 改值不必重画标签。
 */
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

/**
 * @brief 画百分比下方的 "power" 标签（10px 暗灰，块内居中）
 * @note  静态内容：进页时（内容区已清空）画一次即可，改百分比不用重画。
 */
static void ui_pro_label_draw(void)
{
    int16_t tw = mui_text_width(PRO_LABEL, &harmony_os_10, 1);
    int16_t x  = (int16_t)(PRO_TEXT_X + (PRO_TEXT_W - tw) / 2);

    mui_text_draw(x, PRO_LBL_Y, PRO_LABEL, &harmony_os_10, MUI_DARKGREY, UI_BG, 1);
}

/**
 * @brief 绘制第 3 页：进度条 + 百分比 + "power" 标签（帧号由 s_percent 决定）
 * @note  内容区刚被清空，整幅重画即可（位图没有增量能力）。
 */
static void ui_page_draw_progress(void)
{
    const mui_image_t *img = s_pro_frames[pro_frame_of(s_percent)];

    mui_image_draw(PRO_BAR_X, PRO_BAR_Y, img->w, img->h, img->data);
    ui_pro_text_draw();
    ui_pro_label_draw();
}

/**
 * @brief 第 3 页 +/- ：进度百分比增减 10%（同时更新首页信息条第 1 项）
 * @param dir 1 = 加，-1 = 减
 * @note  夹在 0 ~ 100（到边界不再变化，与倒计时/信号档位一致）；
 *        一次步进写屏量 ≈ 进度条整幅 34x76（2584 px，图片 API 不能贴子区域）+ 文字块 1104 px。
 */
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
        return;                        /* 已在边界：画面无变化 */
    }
    s_percent = (uint8_t)v;

    if (s_page == 2) {                 /* 第 3 页：文字 + 进度条一起换（"power" 是静态的，不用重画） */
        ui_pro_text_draw();
        mui_image_draw(PRO_BAR_X, PRO_BAR_Y, pro_0.w, pro_0.h,
                       s_pro_frames[pro_frame_of(s_percent)]->data);
    }
    if (s_page == 0) {                 /* 首页：信息条第 1 项（文字宽度会变 → 整格重画） */
        char pct[8];

        ui_fmt_pct(pct, s_percent);
        ui_info_item_redraw(0, pct, MUI_RGB565(0xa0, 0xa0, 0xa0));
    }
}

/**
 * @brief 绘制占位页（2~4）：内容区居中显示页码
 * @param page 页号（1~3）
 * @note  用 harmony_os_10（覆盖全 ASCII）放大 2 倍；
 *        harmony_os_32 只裁了数字与冒号，无法显示 "Page"
 */
static void ui_page_draw_placeholder(uint8_t page)
{
    char txt[8] = { 'P', 'a', 'g', 'e', ' ', (char)('1' + page), '\0', '\0' };
    int16_t tw = mui_text_width(txt, &harmony_os_10, 2);
    int16_t tx = (int16_t)((BSP_LCD_WIDTH - tw) / 2);
    int16_t ty = (int16_t)(UI_CONTENT_TOP
                           + (UI_CONTENT_H - harmony_os_10.line_height * 2) / 2);

    mui_text_draw(tx, ty, txt, &harmony_os_10, MUI_BLACK, UI_BG, 2);
}

/* -------- 第 2 页：模式列表（ALL / RED / NIR） --------
 * 布局：内容区（logo 分隔线之下、底部四键之上）**纵向三等分**，每项在自己的格子里
 *       水平 + 垂直居中 —— 与信息条"横向三等分 + 居中"是同一套做法（都靠量字宽定位）。
 * 绘制：三项文案固定，模式变化只影响"选中态"（颜色 + 下划线），因此换模式只需重画
 *       选中态真正变过的两项，且清底范围就是该项文字所在的矩形（文本宽 x 行高，
 *       选中项的下划线也落在行框内）——不清整格，省总线。
 */

/** @brief 第 idx 项格子的顶边 = 内容区纵向三等分的第 idx 段（idx = 3 时即内容区底边） */
static int16_t ui_mode_item_top(uint8_t idx)
{
    return (int16_t)(UI_CONTENT_TOP + UI_CONTENT_H * idx / MODE_LIST_NUM);
}

/**
 * @brief 画模式列表的一项：先清该项文字带，再按选中态重画
 * @param idx 模式下标 0 ~ MODE_LIST_NUM-1
 * @note  清底矩形 = (文本宽 + 2) x 行高；左右各留 1px 余量吸收抗锯齿。
 *        三项文案都是 3 字符且字号一致，故"文字带"就是可能被写到的全部像素。
 */
static void ui_mode_item_draw(uint8_t idx)
{
    int16_t top = ui_mode_item_top(idx);
    int16_t h   = (int16_t)(ui_mode_item_top((uint8_t)(idx + 1)) - top);
    const char *name = s_mode_names[idx];
    int16_t tw = mui_text_width(name, &harmony_os_black_14, 1);
    int16_t tx = (int16_t)((BSP_LCD_WIDTH - tw) / 2);     /* 水平居中 */
    int16_t ty = (int16_t)(top + (h - harmony_os_black_14.line_height) / 2); /* 格内垂直居中 */
    uint8_t sel   = (uint8_t)(idx == s_mode_sel);

    mui_rect_fill((int16_t)(tx - 1), ty, (int16_t)(tw + 2),
                  harmony_os_black_14.line_height, UI_BG);
    mui_text_draw(tx, ty, name, &harmony_os_black_14,
                     sel ? MODE_COLOR_ON : MODE_COLOR_OFF, UI_BG, 1);
    
}

/** @brief 画整张模式列表（进页时内容区已清空，三项全画一遍） */
static void ui_page_draw_modes(void)
{
    uint8_t i;

    for (i = 0; i < MODE_LIST_NUM; i++) {
        ui_mode_item_draw(i);
    }
}

/**
 * @brief 切换模式（第 2 页 +/- 调用）：循环 ALL → RED → NIR → ALL
 * @param dir 1 = 下一个，-1 = 上一个
 * @note  到边界**循环**（与 k1 换页一致；要改成"夹住不动"就把取模换成夹取）。
 *        一次切换只写两小片文字带：列表文案不随模式变化，只有选中态变。
 *        首页 MODE 按钮文案跟着模式走；不在首页就只改字符串，等切回首页整页重画。
 */
static void ui_mode_step(int dir)
{
    uint8_t old = s_mode_sel;

    s_mode_sel = (uint8_t)((old + MODE_LIST_NUM + dir) % MODE_LIST_NUM);

    mui_button_set_text(&s_mode, s_mode_titles[s_mode_sel]);
    if (s_page == 0) {
        mui_button_draw(&s_mode);
    }
    if (s_page == 1) {
        ui_mode_item_draw(old);            /* 原选中项：取消高亮 */
        ui_mode_item_draw(s_mode_sel);     /* 新选中项：加上高亮 */
    }
}

/* -------- 第 4 页：信号强度条（6 格，几何绘制） --------
 * 几何比例照原 116x76 位图设计反推（该位图资源已删除，只保留这里的参数）。
 * 几何参数是照原图（116x76）的像素数据反推的，画出来与原图 1:1：
 *   单格宽 10、相邻格左边距 20（缝 10）、每格高 +9、圆角半径 4、底边统一；
 *   原图整幅放在 (5,30)，其内部第 1 格左边在 x=3、底边在 y=73
 *   → 屏幕坐标：第 1 格 x=8，所有格底边 = 103，第 1 格高 25、第 6 格高 70。
 * 每格就是一个抗锯齿圆角矩形：有信号画 SIG_COLOR_ON，没信号画 SIG_COLOR_OFF，
 * 所以"变档"只改颜色、不需要像图片方案那样每档烘一张图（0 字节 Flash）。
 *
 * **数值驱动**（2026-09-23 用户要求：+/- 调这个占位数值，范围 0~1000，0 显示 "Null"）：
 *   数值所在**区间**决定点亮几格：
 *     0 → **0 格（"Null"：全部熄灭）** | 1-99 → 1 格 | 100-299 → 2 格 | 300-499 → 3 格
 *     | 500-699 → 4 格 | 700-899 → 5 格 | 900-1000 → 6 格
 *   档位**不再单独存**（原来 s_sig_level 是第二个真值来源）—— 一律用 sig_level_of(数值) 算出来。
 *   步进 1、**首尾循环**：1000 再加 → 0（显示 "Null"）；0（Null）再减 → 1000。
 */
#define SIG_BAR_NUM      (6)                   /* 格数 = 档位上限（0~6） */
#define SIG_BAR_W        (10)                  /* 单格宽 */
#define SIG_BAR_STEP     (20)                  /* 相邻两格左边距差 = 宽 10 + 缝 10 */
#define SIG_BAR_X0       (8)                   /* 第 1 格左边界（与原图位置一致） */
#define SIG_BAR_BOTTOM   (103)                 /* 所有格的底边（统一基线，只往上长高） */
#define SIG_BAR_H_MIN    (25)                  /* 第 1 格高（第 6 格 = 25 + 5*9 = 70） */
#define SIG_BAR_H_STEP   (9)                   /* 每格递增高度 */
#define SIG_BAR_R        (4)                   /* 圆角半径（由原图角部 AA 反推） */
#define SIG_COLOR_ON     THEME_DEEP_BLUE       /* 有信号：主题蓝（与原图 0x037F 同色） */
#define SIG_COLOR_OFF    MUI_LIGHTGREY         /* 无信号：灰色 */

#define SIG_VAL_STEP     (1)                   /* +/- 步进（1 格数值；首尾**循环**见 ui_sig_val_step） */
                                               /* （上限 SIG_VAL_MAX 与状态 s_sig_val 定义在文件前面，信息条要用） */
#define SIG_TEXT_X       (5)                   /* 大字的行框左边界（左对齐：位数变化时左边界不动） */
#define SIG_TEXT_Y       (32)                  /* 大字行框顶边 */
#define SIG_TEXT_W       (46)                  /* 大字清底带宽 = "1000" 实测宽 44（light_20 数字 11px）+ 2 余量 */
#define SIG_UNIT_X       (7)                   /* 单位 "Hz" 的位置 */
#define SIG_UNIT_Y       (55)

/**
 * @brief 数值 → 点亮格数
 * @param val 数值 0~1000
 * @return 0 ~ SIG_BAR_NUM（**0 = 数值为 0 / 显示 "Null"：全部熄灭**，其余按段：1~99→1 格 … 900~1000→6 格）
 */
static uint8_t sig_level_of(uint16_t val)
{
    static const uint16_t th[SIG_BAR_NUM - 1] = { 100, 300, 500, 700, 900 };  /* 段上界（不含） */
    uint8_t lv = 1;
    uint8_t i;

    if (val == 0) {
        return 0;                              /* Null（无读数）→ 一格都不亮 */
    }
    for (i = 0; i < SIG_BAR_NUM - 1; i++) {
        if (val >= th[i]) {
            lv++;
        }
    }
    return lv;
}

/**
 * @brief 画第 4 页的大字：数值本身；数值为 0 时显示 "Null"
 * @note  左对齐在固定 x（位数变化时左边界不动、只向右长）；
 *        先清一条固定文字带再画 —— 位数在 3~4 位之间变时不会留残影。
 */
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

/**
 * @brief 第 i 格的几何参数
 * @param i 格序号
 * @param h 输出：格高（第 1 格 25 … 第 6 格 70）
 * @return 格顶边 y（所有格底边统一 = SIG_BAR_BOTTOM，只往上长高）
 */
static int16_t sig_bar_top(uint8_t i, int16_t *h)
{
    *h = (int16_t)(SIG_BAR_H_MIN + i * SIG_BAR_H_STEP);
    return (int16_t)(SIG_BAR_BOTTOM + 1 - *h);
}

/**
 * @brief 画第 i 格信号条（颜色由当前数值所在档位决定）
 * @param i 格序号：0 = 最矮（最左），SIG_BAR_NUM-1 = 最高（最右）
 * @note  本函数假定"格子里是页底色"（进页时内容区刚清空，或调用方先 ui_sig_bar_repaint 清过）——
 *        抗锯齿要拿格子底下的颜色做混合基底，见 ui_sig_bar_repaint 的说明。
 */
static void ui_sig_bar_draw(uint8_t i)
{
    int16_t  h;
    int16_t  y     = sig_bar_top(i, &h);
    uint16_t color = (i < sig_level_of(s_sig_val)) ? SIG_COLOR_ON : SIG_COLOR_OFF;

    mui_round_rect_fill_aa((int16_t)(SIG_BAR_X0 + i * SIG_BAR_STEP), y,
                           SIG_BAR_W, h, SIG_BAR_R, color, UI_BG);
}

/**
 * @brief 原地重画第 i 格：先按页底色清掉它的包围盒，再画
 *
 * @param i 格序号
 * @note  **不能直接 ui_sig_bar_draw 了事**：缓冲后端下 AA 的混合基底是**回读当前像素**，
 *        直接重画会拿"这一格旧的颜色"当基底 —— 蓝色熄灭成灰时，圆角弧边会留下一圈蓝
 *        （实机可见的"残留圆角"）。先按 UI_BG 清包围盒再画，结果与"整页重画"逐像素一致
 *        （已验证：两种到达路径的 PNG 哈希相同）。
 *        代价 = 多写 SIG_BAR_W x h（≤ 10x70 = 700 px），只在跨段变色时才发生。
 */
static void ui_sig_bar_repaint(uint8_t i)
{
    int16_t h;
    int16_t y = sig_bar_top(i, &h);

    mui_rect_fill((int16_t)(SIG_BAR_X0 + i * SIG_BAR_STEP), y, SIG_BAR_W, h, UI_BG);
    ui_sig_bar_draw(i);
}

/**
 * @brief 第 4 页内容：大字数值（0 → "Null"）+ 单位 + 按数值档位画满 6 格
 * @note  切页或整页重画时调用；进页时内容区已清空
 */
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

/**
 * @brief 第 4 页 +/- ：调数值（±SIG_VAL_STEP），信号条跟着区间变，首尾**循环**
 * @param dir 1 = 加，-1 = 减
 * @note  循环：1000 再加 → 0（"Null"）；0 再减 → 1000（与第 2 页换模式同为循环语义，
 *        区别于首页倒计时/第 3 页百分比的"夹住不动"）。
 *        只重画"点亮状态真正变过"的那几格（步进 1 时通常 0~1 格，循环那一下会跨 5 格）
 *        + 大字文字带，不整页重画；不在第 4 页时只改数值，等切回该页一次画对。
 */
static void ui_sig_val_step(int dir)
{
    uint8_t old_lv = sig_level_of(s_sig_val);
    uint8_t new_lv;
    uint8_t lo, hi, i;
    int32_t v = (int32_t)s_sig_val + dir * SIG_VAL_STEP;

    if (v < 0) {
        v = SIG_VAL_MAX;                       /* 0（Null）再减 → 绕到上限 1000 */
    } else if (v > SIG_VAL_MAX) {
        v = 0;                                 /* 1000 再加 → 绕到 0（显示 "Null"） */
    }
    s_sig_val = (uint16_t)v;
    new_lv = sig_level_of(s_sig_val);

    if (s_page == 0) {                              /* 首页：信息条第 2 项与本项同源（见 ui_pro_step 同款处理） */
        char hz[10];

        ui_fmt_hz(hz, s_sig_val);
        ui_info_item_redraw(1, hz, MUI_RGB565(0xa0, 0xa0, 0xa0));
        return;
    }
    if (s_page != 3) {
        return;                                     /* 其它页：只改数值 */
    }

    ui_sig_text_draw();
    lo = (uint8_t)((old_lv < new_lv) ? old_lv : new_lv);
    hi = (uint8_t)((old_lv < new_lv) ? new_lv : old_lv);
    for (i = lo; i < hi; i++) {
        ui_sig_bar_repaint(i);                      /* 只有这几格的点亮状态变了（先清底再画，防 AA 残留） */
    }
}
/**
 * @brief 绘制指定页面的中部内容（调用前内容区须已清空）
 * @param page 页号（0~3）
 */
static void ui_page_draw(uint8_t page)
{
    switch (page) {
    case 0:
        ui_page_draw_home();
        break;
    case 1:
        ui_page_draw_modes();       /* 模式列表（ALL / RED / NIR，选中项高亮） */
        break;
    case 2:
        ui_page_draw_progress();    /* 百分比 + 图片版进度条（帧号由百分比决定） */
        break;
    case 3:
        ui_page_draw_signal();      /* 数值(0 → "Null") + 单位 + 信号条（几何绘制，0 资源） */
        break;
    default:
        ui_page_draw_placeholder(page);
        break;
    }
}

/**
 * @brief 切换页面：清空内容区 → 重画该页
 * @param page 页号（0~3）；越界忽略
 * @note  公共区（logo/分隔线/底部四键）不在清除范围内，故切页不影响它们
 */
static void ui_page_show(uint8_t page)
{
    if (page >= UI_PAGE_NUM) {
        return;
    }
    s_page = page;
    mui_rect_fill(0, UI_CONTENT_TOP, BSP_LCD_WIDTH, UI_CONTENT_H, UI_BG);
    ui_page_draw(page);
}

/**
 * @brief 切到指定页，并同步底部四键的选中态
 * @param page 页号（0~3）
 * @note  切页与"高亮键"始终一致：按键/触摸切页都走这里
 */
static void ui_page_select(uint8_t page)
{
    uint8_t k;

    for (k = 0; k < UI_BTN_NUM; k++) {
        mui_button_set_pressed(&s_btns[k], (uint8_t)(k == page));
    }
    ui_page_show(page);
    app_ui_draw_buttons();
}

/**
 * @brief 按对象当前状态重画底部四键（状态变化后调用即可）
 * @note  只画公共区的底部四键；Mode 按钮属于第 1 页中部内容，
 *        由 ui_page_draw_home() 负责，避免它被画到其它页上
 */
void app_ui_draw_buttons(void)
{
    uint8_t i;

    for (i = 0; i < UI_BTN_NUM; i++) {
        mui_button_draw(&s_btns[i]);
    }
}

/**
 * @brief 物理按键回调：按"键号 → 功能"表分派
 * @param key_id 键号：0~3 = 底部四键，4 = Mode 键
 * @param down   1 = 按下，0 = 抬起
 * @note  只在按下沿动作，长按不会连续触发。功能分配见 s_key_fn 表（按丝印 k1~k5 叫：
 *        切页 / 加 / 减 / 回首页·首页切开关；**换键位只改那张表**，这里不写死键号）。
 *        加/减是**页内参数键**：首页调倒计时（±1 分钟）、第 2 页换测量模式、
 *        第 3 页调进度百分比（±10%）、第 4 页调数值（±1，0~1000 首尾循环，0 显示 "Null"）。
 */
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
        ui_mode_step(1);            /* 单个键循环换模式（当前未分配到任何键） */
        break;

    case KEY_FN_SIGNAL_UP:
        /* 加：首页 = 倒计时 +1 分钟；第 2 页 = 换模式；第 3 页 = 百分比 +10%；第 4 页 = 数值 +100 */
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
        /* 减：首页 = 倒计时 -1 分钟；第 2 页 = 换模式；第 3 页 = 百分比 -10%；第 4 页 = 数值 -100 */
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
            ui_page_select(0);          /* 不在首页：先回首页 */
        } else {
            ui_power_toggle();          /* 已在首页：切换开关（ON ↔ OFF，倒计时启停） */
        }
        break;

    default:
        break;
    }

    mui_screen_flush();    /* 缓冲后端：按键引起的绘制立即推屏；直绘下为空操作 */
}

/**
 * @brief 消费触摸事件队列：统一驱动按钮高亮与点击业务
 * @note  按键、鼠标、触摸屏都经由此处生效；事件驱动重画，无事件不写屏。
 */
static void ui_poll_input(void)
{
    mui_touch_event_t ev;
    uint8_t i;

    while ((ev = mui_touch_poll()) != MUI_TOUCH_NONE) {
        for (i = 0; i < UI_BTN_NUM; i++) {
            if (mui_button_touch(&s_btns[i], ev)) {
                /* 业务：底部四键 → 切到第 i 页（含互斥选中） */
                ui_page_select(i);
            }
        }
        if (mui_button_touch(&s_mode, ev)) {
            /* 业务：Mode 键切换（latch 模式下 set_pressed 已由库切换）。
             * Mode 只在第 1 页可见，故仅在该页重画它。 */
            if (s_page == 0) {
                mui_button_draw(&s_mode);
            }
        }
        /* MOVE 不改变按钮状态，跳过重画；其余事件状态可能已变 */
        if (ev != MUI_TOUCH_MOVE) {
            app_ui_draw_buttons();
        }
    }
}



void app_ui_init(void)
{        
    int16_t by = BTN_TOP;              /* 底部对齐（与按键映射表同源） */

    /* 内部会调用 mui_port_init() → bsp_lcd_init()，完成显示屏初始化 */
    mui_init(BSP_LCD_WIDTH, BSP_LCD_HEIGHT);
    mui_screen_clear(UI_BG);               

    /* ---- 公共区（不随页面切换）：logo + 分隔线 ---- */
    mui_image_draw((int16_t)((BSP_LCD_WIDTH - logo.w) / 2), 4,
                    logo.w, logo.h, logo.data);
    mui_hline_draw(4, 20, 120, MUI_DARKGREY);

    /* ---- 公共区：底部四键（点击即切到对应页） ---- */
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

    /* ---- 第 1 页的中部对象（内容由 ui_page_draw_home 绘制） ---- */
    mui_button_init(&s_mode, MODE_X, MODE_Y, MODE_W, BTN_H, 
                    &s_btn_style, NULL, NULL);
    mui_button_set_font(&s_mode, &harmony_os_10);
    mui_button_set_text(&s_mode, s_mode_titles[s_mode_sel]);   /* 文案随当前模式 */
    mui_button_set_pressed(&s_mode, 1);
    mui_button_set_latch(&s_mode, 1);  /* 锁存：物理键/点击切换模式并保持 */

    ui_clock_layout();

    /* ---- 首次显示第 1 页（清内容区后完整绘制） ---- */
    ui_page_show(0);

    mui_screen_flush();    /* 缓冲后端：把首屏推给屏；直绘下为空操作 */
}

void app_ui_frame(void)
{
    uint32_t ms = bsp_system_tick_ms();

    /* ---- 输入：物理按键（app_ui_key）/ 鼠标 / 触摸屏统一在此落地 ---- */
    ui_poll_input();

    /* ---- 倒计时：开关开着才走；与当前页无关（不在首页只更新变量，不刷屏） ---- */
    if (s_power_on) {
        uint8_t ticked = 0;

        while ((ms - s_timer_tick) >= 1000) {   /* 用 +=1000 累计，避免慢帧丢秒 */
            s_timer_tick += 1000;
            ticked = 1;
            if (s_remain_s > 0) {
                s_remain_s--;
            }
            if (s_remain_s == 0) {
                ui_power_set(0);                /* 到 0 自动关闭（停在 00:00） */
                break;
            }
        }
        if (ticked && s_page == 0) {
            ui_clock_update(s_remain_s);
        }
    }

    /* ---- 各页专属的动态内容：不在本页就不更新，否则会画到别页上 ---- */

    /* 第 1 页：倒计时运行中才让冒号闪烁（停止时冒号常亮，由 ui_clock_colon_reset 保证） */
    if (s_page == 0 && s_power_on && (ms - s_clock_colon_tick) >= CLOCK_COLON_BLINK_MS) {
        s_clock_colon_tick = ms;
        s_clock_colon_on = (uint8_t)(!s_clock_colon_on);
        ui_clock_colon_apply();
    }

    /* 第 3 页：进度条已是"数值驱动"（帧号 = 百分比/10），没有每帧要刷的动画 ——
     * 原来那段 100ms 循环换帧的代码已随动画一起撤掉（要恢复就是另一种语义了）。 */

    mui_screen_flush();    /* 缓冲后端：本帧改动统一推屏；直绘下为空操作 */
}
