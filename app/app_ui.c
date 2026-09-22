/**
 * @file app_ui.c
 * @brief 应用界面：MUI demo（128x128 屏适配版）
 *
 * 演示内容：
 *   1. 品牌 logo（白底彩色位图，顶部居中）
 *   2. 中部 32px 大号运行时间（mm:ss，每秒脏区重画，位置与 demo 一致）
 *   3. 底部四等分按钮 + Mode 按钮（mui_button_t 对象，OO 风格：init / set_xxx / draw）
 *   4. 第 3 页进度条动画（img_pro_0~10 共 11 帧循环播放，替代原进度条控件）
 *   5. 物理按键接入：app_ui_key() 按 s_key_fn 表分派键号到功能，
 *      默认单个键循环切页（换键/换功能只改那张表）；鼠标与触摸屏另走触摸链路
 *   6. 页面（Tab）：底部四键对应 4 个页面。屏幕分三层——上部 logo+分隔线、
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
#include "mui_font_harmony_os_32.h"
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
#define LINE_H				((int16_t)(BSP_LCD_HEIGHT - 25))  /* 信息条下划线 y */

/* -------- 模式对象 -------- */
#define MODE_X		((int16_t)((BSP_LCD_WIDTH - 60) / 2))
#define MODE_Y		(28)
#define MODE_W		(60)
static mui_button_t s_mode;

/* -------- 物理按键功能分配 --------
 * 数组下标 = 键号（0~3 = 底部四键，4 = Mode 键），值 = 该键的功能。
 * 换键或换功能只改这张表，分派逻辑不用动：
 *   例 1：把切页键从 0 号改到 2 号 —— 把 KEY_FN_PAGE_NEXT 挪到第 3 行
 *   例 2：某个键不响应 —— 该行写 KEY_FN_NONE
 */
typedef enum {
    KEY_FN_NONE = 0,      /**< 不响应 */
    KEY_FN_PAGE_NEXT,     /**< 循环切换页面：页1→页2→页3→页4→页1 */
    KEY_FN_MODE_TOGGLE,   /**< 切换 Mode 按钮状态（仅第 1 页可见） */
    KEY_FN_SIGNAL_UP,     /**< 信号档位 +1 格（第 4 页显示，满格后再按无动作） */
    KEY_FN_SIGNAL_DOWN,   /**< 信号档位 -1 格（第 4 页显示，0 格后再按无动作） */
} app_key_fn_t;

static const uint8_t s_key_fn[] = {
    KEY_FN_PAGE_NEXT,     /* 键 0：唯一的切页键 */
    KEY_FN_SIGNAL_UP,     /* 键 1：信号 +1 格（模拟器按键盘 2） */
    KEY_FN_SIGNAL_DOWN,   /* 键 2：信号 -1 格（模拟器按键盘 3） */
    KEY_FN_NONE,          /* 键 3：未分配 */
    KEY_FN_MODE_TOGGLE,   /* 键 4（M）：Mode 切换 */
};
#define KEY_FN_NUM  ((uint8_t)(sizeof(s_key_fn) / sizeof(s_key_fn[0])))

/* -------- 中部大号运行时间 -------- */
#define UI_CLOCK_TOP     22                             
#define UI_CLOCK_BOTTOM  (BSP_LCD_HEIGHT - 2 - 15)       

static const char UI_CLOCK_REF[] = "88:88";   /* 基准文本：定水平居中位置（防跳动） */
static mui_label_t s_clock;                   /* 时间标签对象（内部缓冲 + 增量刷新） */

/**
 * @brief 初始化时间标签：按基准文本宽度水平居中、在 logo 与按钮之间垂直居中
 * @note  label 为左对齐；mm:ss 是定长 5 字符，位置固定故不会跳动
 */
static void ui_clock_layout(void)
{
    int16_t tw = mui_text_width(UI_CLOCK_REF, &harmony_os_32, 1);
    int16_t tx = (int16_t)((BSP_LCD_WIDTH - tw) / 2);
    int16_t ty = (int16_t)(UI_CLOCK_TOP
                           + (UI_CLOCK_BOTTOM - UI_CLOCK_TOP
                              - harmony_os_32.line_height) / 2);

    mui_label_init(&s_clock, tx, ty, &harmony_os_32, MUI_BLACK, UI_BG, 1);
}

/**
 * @brief 更新中部运行时间（mm:ss）：label 自动"从差异点擦旧画新"
 * @param sec 运行秒数（bsp_system_tick_ms() / 1000）
 * @note  例如 00:01 → 00:02 只重画末两位，写屏量与闪烁都最小
 */
static void ui_clock_update(uint32_t sec)
{
    char txt[6];
    uint32_t mm = (sec / 60) % 100;
    uint32_t ss = sec % 60;

    txt[0] = (char)('0' + (mm / 10) % 10);
    txt[1] = (char)('0' + mm % 10);
    txt[2] = ':';
    txt[3] = (char)('0' + (ss / 10) % 10);
    txt[4] = (char)('0' + ss % 10);
    txt[5] = '\0';

    mui_label_set_text(&s_clock, txt);
}

/* -------- 页面（Tab）内容：只画中部内容区，公共区由 init 负责 -------- */

static uint8_t s_page = 0;              /**< 当前页（0~3） */

/* -------- 第 3 页：进度条动画（img_pro_0~10 共 11 帧循环播放） -------- */
static const mui_image_t *const s_pro_frames[] = {
    &pro_0, &pro_1, &pro_2, &pro_3, &pro_4, &pro_5,
    &pro_6, &pro_7, &pro_8, &pro_9, &pro_10
};
#define PRO_FRAME_NUM  ((uint8_t)(sizeof(s_pro_frames) / sizeof(s_pro_frames[0])))
#define PRO_W          (38)   /* 单帧宽（与 assets/img_pro_*.c 一致） */
#define PRO_H          (76)   /* 单帧高 */
#define PRO_X          ((int16_t)((BSP_LCD_WIDTH - PRO_W) / 2))
#define PRO_Y          ((int16_t)(UI_CONTENT_TOP + (UI_CONTENT_H - PRO_H) / 2))
static uint8_t  s_pro_frame = 0;  /**< 当前动画帧（0~10） */
static uint32_t s_pro_tick  = 0;  /**< 上次帧切换时间（ms） */

/** @brief 绘制信息条（60% / 100Hz / OFF 三项数值 + 各自下划线） */
static void ui_draw_info_bar(void)
{
    mui_text_draw(10, LINE_H - 12, "60%", &harmony_os_10,
                          MUI_RGB565(0xa0, 0xa0, 0xa0), UI_BG, 1);
    mui_text_draw(45, LINE_H - 12, "100Hz", &harmony_os_10,
                          MUI_RGB565(0xa0, 0xa0, 0xa0), UI_BG, 1);
    mui_text_draw(102, LINE_H - 12, "OFF", &harmony_os_10,
                          MUI_RGB565(0x00, 0x4e, 0xff), UI_BG, 1);

    mui_hline_draw(10, LINE_H, 25, MUI_RGB565(0xa0, 0xa0, 0xa0));
    mui_hline_draw(45, LINE_H, 30, MUI_LIGHTGREY);
    mui_hline_draw(102, LINE_H, 20, MUI_BLACK);
}

/**
 * @brief 绘制第 1 页（首页）：模式按钮 + 大号时间 + 信息条
 * @note  调用前内容区已清空，故增量刷新的对象必须强制全量重绘：
 *        把 drawn 清 0，set_text/set_value 即走"无擦除"的全量绘制分支
 */
static void ui_page_draw_home(void)
{
    mui_button_draw(&s_mode);

    s_clock.drawn = 0;
    ui_clock_update(bsp_system_tick_ms() / 1000);

    ui_draw_info_bar();
}

/**
 * @brief 绘制第 3 页：进度条动画当前帧（循环播放 img_pro_0~10）
 * @note  内容区刚被清空，直接绘制当前帧即可；重置计时避免切页后立刻跳帧
 */
static void ui_page_draw_progress(void)
{
    const mui_image_t *img = s_pro_frames[s_pro_frame];

    s_pro_tick = bsp_system_tick_ms();
    mui_image_draw(PRO_X, PRO_Y, img->w, img->h, img->data);
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

/**
 * @brief 第 2 页内容：抗锯齿圆角矩形 + 页码
 * @note  矩形下方的 "Page 2" 文本用 harmony_os_10 放大 2 倍（覆盖全 ASCII）
 */
/* 第 2 页演示：一个大圆角抗锯齿矩形（目视检查圆角边缘质量） */
#define AA_DEMO_X    (24)
#define AA_DEMO_Y    (30)
#define AA_DEMO_W    (80)
#define AA_DEMO_H    (50)
#define AA_DEMO_R    (16)     /* 圆角半径取大一点，肉眼容易看出锯齿差异 */

static void ui_page_draw_aa_test(void)
{
    const char txt[] = "Page 2";
    int16_t tw;

    mui_round_rect_fill_aa(AA_DEMO_X, AA_DEMO_Y, AA_DEMO_W, AA_DEMO_H,
                           AA_DEMO_R, THEME_DEEP_BLUE, UI_BG);

    /* 文本画在矩形下方，避免遮挡测试图形 */
    tw = mui_text_width(txt, &harmony_os_10, 2);
    mui_text_draw((int16_t)((BSP_LCD_WIDTH - tw) / 2), 84, txt,
                          &harmony_os_10, MUI_BLACK, UI_BG, 2);
}

/* -------- 第 4 页：信号强度条（6 格，几何绘制） --------
 * 几何比例照原 116x76 位图设计反推（该位图资源已删除，只保留这里的参数）。
 * 几何参数是照原图（116x76）的像素数据反推的，画出来与原图 1:1：
 *   单格宽 10、相邻格左边距 20（缝 10）、每格高 +9、圆角半径 4、底边统一；
 *   原图整幅放在 (5,30)，其内部第 1 格左边在 x=3、底边在 y=73
 *   → 屏幕坐标：第 1 格 x=8，所有格底边 = 103，第 1 格高 25、第 6 格高 70。
 * 每格就是一个抗锯齿圆角矩形：有信号画 SIG_COLOR_ON，没信号画 SIG_COLOR_OFF，
 * 所以"变档"只改颜色、不需要像图片方案那样每档烘一张图（0 字节 Flash）。
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

static uint8_t s_sig_level = SIG_BAR_NUM;      /* 当前档位 0~6（默认满格，与原图一致） */

/**
 * @brief 画第 i 格信号条（颜色由当前档位决定）
 * @param i 格序号：0 = 最矮（最左），SIG_BAR_NUM-1 = 最高（最右）
 */
static void ui_sig_bar_draw(uint8_t i)
{
    int16_t  h     = (int16_t)(SIG_BAR_H_MIN + i * SIG_BAR_H_STEP);
    uint16_t color = (i < s_sig_level) ? SIG_COLOR_ON : SIG_COLOR_OFF;

    mui_round_rect_fill_aa((int16_t)(SIG_BAR_X0 + i * SIG_BAR_STEP),
                           (int16_t)(SIG_BAR_BOTTOM + 1 - h),
                           SIG_BAR_W, h, SIG_BAR_R, color, UI_BG);
}

/**
 * @brief 第 4 页内容：按当前档位画满 6 格（切页或整页重画时调用）
 */
static void ui_page_draw_signal(void)
{
    uint8_t i;

    for (i = 0; i < SIG_BAR_NUM; i++) {
        ui_sig_bar_draw(i);
    }
}

/**
 * @brief 信号档位加减一格（物理按键调用）
 * @param up 非 0 = 加一格，0 = 减一格
 * @note  只重画"颜色真正变过"的那一格（加 → 旧的档位那格由灰变蓝；减 → 新的档位那格由蓝变灰），
 *        不整页重画：一格约 10x61 像素，比整页 6 格省约 85% 的写屏量。
 *        不在第 4 页时只改状态不绘制，等切回该页由 ui_page_draw_signal() 一次画对。
 */
static void ui_sig_level_step(uint8_t up)
{
    uint8_t old = s_sig_level;

    if (up) {
        if (old >= SIG_BAR_NUM) {
            return;                                     /* 已满格 */
        }
        s_sig_level = (uint8_t)(old + 1);
    } else {
        if (old == 0) {
            return;                                     /* 已空格 */
        }
        s_sig_level = (uint8_t)(old - 1);
    }

    if (s_page == 3) {
        ui_sig_bar_draw((uint8_t)(up ? old : s_sig_level));
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
        ui_page_draw_aa_test();     /* 【临时测试】抗锯齿圆角矩形（验证完即删） */
        break;
    case 2:
        ui_page_draw_progress();
        break;
    case 3:
        ui_page_draw_signal();      /* 信号条：按当前档位画 6 格（几何绘制，0 资源） */
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
 * @note  只在按下沿动作，长按不会连续触发；
 *        功能分配见 s_key_fn 表：0 号键循环切页（页4 之后回到页1，底部高亮随之同步），
 *        1/2 号键加减第 4 页的信号档位，4 号键（M）切 Mode。
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

    case KEY_FN_MODE_TOGGLE:
        mui_button_set_pressed(&s_mode, (uint8_t)(!s_mode.pressed));
        if (s_page == 0) {
            mui_button_draw(&s_mode);
        }
        break;

    case KEY_FN_SIGNAL_UP:
        ui_sig_level_step(1);       /* 只重画变色的那一格 */
        break;

    case KEY_FN_SIGNAL_DOWN:
        ui_sig_level_step(0);
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
    mui_button_set_text(&s_mode, "Mode:RED");
    mui_button_set_pressed(&s_mode, 1);
    mui_button_set_latch(&s_mode, 1);  /* 锁存：物理键/点击切换模式并保持 */

    ui_clock_layout();

    /* ---- 首次显示第 1 页（清内容区后完整绘制） ---- */
    ui_page_show(0);

    mui_screen_flush();    /* 缓冲后端：把首屏推给屏；直绘下为空操作 */
}

void app_ui_frame(void)
{
    static uint32_t s_last_sec = 0xFFFFFFFFu;   /* 初值保证首调重画 */
    uint32_t ms = bsp_system_tick_ms();
    uint32_t sec = ms / 1000;

    /* ---- 输入：物理按键（app_ui_key）/ 鼠标 / 触摸屏统一在此落地 ---- */
    ui_poll_input();

    /* ---- 各页专属的动态内容：不在本页就不更新，否则会画到别页上 ---- */

    /* 第 1 页：中部大号时间，每秒按脏区重画一次 */
    if (s_page == 0 && sec != s_last_sec) {
        s_last_sec = sec;
        ui_clock_update(sec);
    }

    /* 第 3 页：进度条动画循环播放（每 100ms 切一帧，11 帧循环） */
    if (s_page == 2 && ms - s_pro_tick >= 100) {
        const mui_image_t *img;

        s_pro_tick = ms;
        s_pro_frame = (uint8_t)((s_pro_frame + 1) % PRO_FRAME_NUM);
        img = s_pro_frames[s_pro_frame];
        mui_image_draw(PRO_X, PRO_Y, img->w, img->h, img->data);
    }

    mui_screen_flush();    /* 缓冲后端：本帧改动统一推屏；直绘下为空操作 */
}
