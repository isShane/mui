/**
 * @file test_main.c
 * @brief MUI 自动化测试：边界裁剪安全性 + 图元几何正确性 + 渲染测试图
 */

#include <stdio.h>
#include <string.h>
#include <math.h>      /* 仅测试用：AA 参考实现用浮点 sqrt */
#include "mui.h"
#include "mui_font.h"
#include "mui_label.h"
#include "mui_button.h"
#include "mui_progressbar.h"
#include "mui_slider.h"
#include "mui_toggle.h"
#include "mui_anim.h"
#include "mui_layout.h"
#include "mui_gauge.h"
#include "mui_list.h"
#include "mui_popup.h"
#include "mui_dropdown.h"
#include "sim_helper.h"

/* 测试背景色：所有"图形外部"断言跟随此色，改背景只需改这一处 */
#define TEST_BG MUI_WHITE

static int test_failed = 0;

/** @brief 断言宏 */
#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("[FAIL] %s (line %d)\n", msg, __LINE__);         \
            test_failed++;                                          \
        }                                                           \
    } while (0)

/**
 * @brief 读取模拟屏像素
 * @param x 横坐标
 * @param y 纵坐标
 * @return RGB565 颜色
 */
static uint16_t px(int x, int y)
{
    mui_screen_flush();   /* 缓冲后端：先推屏再读；直绘下为空操作 */
    if (x < 0 || x >= SIM_W || y < 0 || y >= SIM_H) {
        return 0;  /* 越界读保护：返回黑色，断言将失败（属预期） */
    }
    return sim_get_fb()[y * SIM_W + x];
}

/* -------- 测试：越界参数全程安全 -------- */
static void test_clipping(void)
{
    /* 各种越界调用不应产生任何底层越界写入 */
    sim_clear_violations();
    mui_pixel_draw(-5, -5, MUI_WHITE);
    mui_pixel_draw(10000, 10000, MUI_WHITE);
    mui_rect_fill(-50, -50, 100, 100, MUI_WHITE);
    mui_rect_fill(300, 200, 100, 100, MUI_WHITE);
    mui_rect_fill(10, 10, -3, 5, MUI_WHITE);
    mui_rect_fill(10, 10, 5, -3, MUI_WHITE);
    mui_rect_draw(-100, -100, 500, 500, MUI_WHITE);
    mui_round_rect_fill(-30, -30, 60, 60, 20, MUI_WHITE);
    mui_round_rect_fill(310, 230, 50, 50, 100, MUI_WHITE);  /* r 自动限幅 */
    mui_round_rect_draw(-10, 200, 400, 60, 15, MUI_WHITE);
    mui_line_draw(-20, -20, 400, 300, MUI_WHITE);
    mui_circle_fill(-10, -10, 50, MUI_WHITE);
    mui_circle_fill(330, 250, 40, MUI_WHITE);
    mui_circle_draw(160, 120, 500, MUI_WHITE);
    mui_ellipse_fill(-5, -5, 30, 20, MUI_WHITE);
    mui_ellipse_draw(330, 250, 50, 40, MUI_WHITE);
    mui_triangle_fill(-30, -30, 350, 10, 160, 280, MUI_WHITE);
    mui_triangle_draw(-40, -40, 400, 100, 200, 300, MUI_WHITE);
    mui_screen_clear(MUI_WHITE);
    mui_screen_flush();   /* 缓冲后端：推送路径也要纳入越界检查 */
    CHECK(sim_violations() == 0, "越界绘制未触发底层违规");
}

/* -------- 测试：矩形 -------- */
static void test_rect(void)
{
    mui_screen_clear(MUI_WHITE);

    /* 实心矩形区域与边界 */
    mui_rect_fill(10, 10, 20, 15, MUI_RED);
    CHECK(px(10, 10) == MUI_RED, "fill_rect 左上角");
    CHECK(px(29, 24) == MUI_RED, "fill_rect 右下角");
    CHECK(px(9, 10) == TEST_BG, "fill_rect 左边界外");
    CHECK(px(30, 10) == TEST_BG, "fill_rect 右边界外");
    CHECK(px(10, 25) == TEST_BG, "fill_rect 下边界外");

    /* 空心矩形只描边 */
    mui_screen_clear(MUI_WHITE);
    mui_rect_draw(5, 5, 30, 20, MUI_GREEN);
    CHECK(px(5, 5) == MUI_GREEN, "draw_rect 左上角");
    CHECK(px(34, 24) == MUI_GREEN, "draw_rect 右下角");
    CHECK(px(6, 6) == TEST_BG, "draw_rect 内部为空");
    CHECK(px(20, 5) == MUI_GREEN, "draw_rect 上边");
    CHECK(px(20, 24) == MUI_GREEN, "draw_rect 下边");
    CHECK(px(5, 15) == MUI_GREEN, "draw_rect 左边");
    CHECK(px(34, 15) == MUI_GREEN, "draw_rect 右边");
}

/* -------- 测试：圆角矩形 -------- */
static void test_round_rect(void)
{
    mui_screen_clear(MUI_WHITE);

    /* r=0 等价于直角矩形 */
    mui_round_rect_fill(10, 10, 40, 30, 0, MUI_BLUE);
    CHECK(px(10, 10) == MUI_BLUE, "r=0 左上角填充");

    /* 圆角：角点不填充，弧线内侧填充，直边区填充 */
    mui_screen_clear(MUI_WHITE);
    mui_round_rect_fill(10, 10, 40, 30, 10, MUI_YELLOW);
    CHECK(px(10, 10) == TEST_BG, "左上角最外点不填充");
    CHECK(px(49, 39) == TEST_BG, "右下角最外点不填充");
    CHECK(px(14, 14) == MUI_YELLOW, "左上角弧内侧填充（距角圆心平方72<100）");
    CHECK(px(10, 25) == MUI_YELLOW, "左直边区填充");
    CHECK(px(25, 10) == MUI_YELLOW, "顶直边区填充");
    CHECK(px(49, 25) == MUI_YELLOW, "右直边区填充");
    CHECK(px(30, 39) == MUI_YELLOW, "底直边区填充");
    CHECK(px(30, 25) == MUI_YELLOW, "中心填充");

    /* r 超过短边一半时自动限幅（w=20 → r 上限10，胶囊形） */
    mui_screen_clear(MUI_WHITE);
    mui_round_rect_fill(100, 60, 20, 40, 99, MUI_CYAN);
    CHECK(px(100, 60) == TEST_BG, "胶囊形角点外不填充");
    CHECK(px(100, 70) == MUI_CYAN, "胶囊形圆心行最左点填充");
    CHECK(px(110, 60) == MUI_CYAN, "胶囊形顶点填充");
    CHECK(px(100, 89) == MUI_CYAN, "胶囊形下圆心行最左点填充");
}

/* -------- 测试：直线 -------- */
static void test_line(void)
{
    mui_screen_clear(MUI_WHITE);

    /* 水平线 */
    mui_line_draw(0, 5, 10, 5, MUI_RED);
    for (int x = 0; x <= 10; x++) {
        CHECK(px(x, 5) == MUI_RED, "水平线逐点");
    }
    CHECK(px(11, 5) == TEST_BG, "水平线终点外");

    /* 垂直线 */
    mui_line_draw(3, 0, 3, 8, MUI_GREEN);
    for (int y = 0; y <= 8; y++) {
        CHECK(px(3, y) == MUI_GREEN, "垂直线逐点");
    }

    /* 对角线：两端点必在 */
    mui_screen_clear(MUI_WHITE);
    mui_line_draw(0, 0, 100, 50, MUI_WHITE);
    CHECK(px(0, 0) == MUI_WHITE, "对角线起点");
    CHECK(px(100, 50) == MUI_WHITE, "对角线终点");

    /* 反向绘制同样生效 */
    mui_screen_clear(MUI_WHITE);
    mui_line_draw(100, 50, 0, 0, MUI_WHITE);
    CHECK(px(0, 0) == MUI_WHITE, "反向直线起点");
    CHECK(px(100, 50) == MUI_WHITE, "反向直线终点");
}

/* -------- 测试：圆 -------- */
static void test_circle(void)
{
    int cnt_in;
    int cnt_out;

    mui_screen_clear(MUI_WHITE);

    /* 实心圆 r=20：中心、四方向边缘内1px 均应填充 */
    mui_circle_fill(100, 100, 20, MUI_MAGENTA);
    CHECK(px(100, 100) == MUI_MAGENTA, "圆心");
    CHECK(px(80, 100) == MUI_MAGENTA, "圆最左点");
    CHECK(px(120, 100) == MUI_MAGENTA, "圆最右点");
    CHECK(px(100, 80) == MUI_MAGENTA, "圆最上点");
    CHECK(px(100, 120) == MUI_MAGENTA, "圆最下点");
    CHECK(px(79, 100) == TEST_BG, "圆外左侧");
    CHECK(px(121, 100) == TEST_BG, "圆外右侧");

    /* 空心圆 r=30：统计边界环带像素数量级 */
    mui_screen_clear(TEST_BG);
    mui_circle_draw(64, 64, 30, MUI_BLACK);
    cnt_in = 0;
    for (int y = 34; y <= 94; y++) {
        for (int x = 34; x <= 94; x++) {
            if (px(x, y) == MUI_BLACK) {
                cnt_in++;
            }
        }
    }
    /* 周长约 2*pi*30 ≈ 188，允许 ±30% 波动 */
    CHECK(cnt_in > 130 && cnt_in < 250, "空心圆周长像素数合理");

    /* 实心圆面积 ≈ pi*r^2 */
    mui_screen_clear(TEST_BG);
    mui_circle_fill(64, 64, 30, MUI_BLACK);
    cnt_in = 0;
    cnt_out = 0;
    for (int y = 34; y <= 94; y++) {
        for (int x = 34; x <= 94; x++) {
            if (px(x, y) == MUI_BLACK) {
                cnt_in++;
            } else {
                cnt_out++;
            }
        }
    }
    /* pi*900 ≈ 2827，检查面积在 10% 误差内 */
    CHECK(cnt_in > 2540 && cnt_in < 3100, "实心圆面积合理");
}

/* -------- 测试：椭圆 -------- */
static void test_ellipse(void)
{
    mui_screen_clear(MUI_WHITE);

    mui_ellipse_fill(64, 64, 40, 20, MUI_ORANGE);
    CHECK(px(64, 64) == MUI_ORANGE, "椭圆中心");
    CHECK(px(24, 64) == MUI_ORANGE, "椭圆最左点");
    CHECK(px(104, 64) == MUI_ORANGE, "椭圆最右点");
    CHECK(px(64, 44) == MUI_ORANGE, "椭圆最上点");
    CHECK(px(64, 84) == MUI_ORANGE, "椭圆最下点");
    CHECK(px(23, 64) == TEST_BG, "椭圆外左侧");
    CHECK(px(64, 43) == TEST_BG, "椭圆外上方");

    /* 空心椭圆四极值点在曲线上（用非背景色画，否则断言恒真） */
    mui_screen_clear(MUI_WHITE);
    mui_ellipse_draw(64, 64, 50, 25, MUI_CYAN);
    CHECK(px(64, 39) == MUI_CYAN, "空心椭圆顶点");
    CHECK(px(64, 89) == MUI_CYAN, "空心椭圆底点");
    CHECK(px(14, 64) == MUI_CYAN, "空心椭圆左点");
    CHECK(px(114, 64) == MUI_CYAN, "空心椭圆右点");
    CHECK(px(64, 64) == TEST_BG, "空心椭圆内部为空");

    /* rx==ry 时退化为圆 */
    mui_screen_clear(MUI_WHITE);
    mui_ellipse_draw(60, 60, 15, 15, MUI_CYAN);
    CHECK(px(60, 45) == MUI_CYAN, "rx==ry 顶点");
    CHECK(px(45, 60) == MUI_CYAN, "rx==ry 左点");
}

/* -------- 测试：三角形 -------- */
static void test_triangle(void)
{
    mui_screen_clear(MUI_WHITE);

    /* 实心三角形：三个顶点、形心应在内部，远离边的包围盒角落应在外 */
    mui_triangle_fill(30, 40, 110, 40, 70, 110, MUI_GREEN);
    CHECK(px(30, 40) == MUI_GREEN, "三角形顶点0");
    CHECK(px(110, 40) == MUI_GREEN, "三角形顶点1");
    CHECK(px(70, 110) == MUI_GREEN, "三角形顶点2");
    CHECK(px(70, 63) == MUI_GREEN, "三角形形心附近");
    CHECK(px(32, 105) == TEST_BG, "三角形下方左侧外");
    CHECK(px(108, 105) == TEST_BG, "三角形下方右侧外");

    /* 底边水平、顶点朝下的三角形 */
    mui_screen_clear(MUI_WHITE);
    mui_triangle_fill(20, 20, 60, 20, 40, 60, MUI_RED);
    CHECK(px(40, 20) == MUI_RED, "倒三角底边中点");
    CHECK(px(40, 60) == MUI_RED, "倒三角下顶点");
    CHECK(px(40, 40) == MUI_RED, "倒三角中轴");

    /* 完全退化为水平线 */
    mui_screen_clear(MUI_WHITE);
    mui_triangle_fill(10, 30, 50, 30, 30, 30, MUI_WHITE);
    CHECK(px(10, 30) == MUI_WHITE, "退化线起点");
    CHECK(px(30, 30) == MUI_WHITE, "退化线中点");
    CHECK(px(50, 30) == MUI_WHITE, "退化线终点");
    CHECK(px(30, 29) == TEST_BG, "退化线上方无像素");
}

/* -------- 测试：文本标签增量更新 -------- */

/** @brief 参考帧（整串一次性绘制的期望结果） */
static uint16_t label_expect[SIM_W * SIM_H];

/* -------- 测试用 LVGL 字体：3 个 3x5 字形，adv_w 各不相同（比例步进） -------- */

/** @brief 像素步进宽度转 adv_w（1/16 像素定点） */
#define LV_ADV(px) ((uint16_t)((px) * 16))

static const uint8_t lv_test_bitmap[] = {
    0, 255, 0, 255, 255, 0, 0, 255, 0, 0, 255, 0, 255, 255, 255,   /* '1' */
    255, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, /* '2' */
    255, 255, 0, 0, 0, 255, 0, 255, 0, 0, 0, 255, 255, 255, 0,     /* '3' */
};

static const mui_glyph_dsc_t lv_test_glyphs[] = {
    {0,  LV_ADV(3), 3, 5, 0, 0},
    {15, LV_ADV(4), 3, 5, 0, 0},
    {30, LV_ADV(6), 3, 5, 0, 0},
};

static const mui_font_cmap_t lv_test_cmap[] = {
    {'1', '3', 0},
};

static const mui_font_t lv_test_font = {
    lv_test_bitmap, lv_test_glyphs, lv_test_cmap, 1, 0, 0, 8, 2
};

/** @brief 等宽测试字体：三字形 adv_w 相同，用于命中整格覆盖快路径 */
static const mui_glyph_dsc_t lv_mono_glyphs[] = {
    {0,  LV_ADV(4), 3, 5, 0, 0},
    {15, LV_ADV(4), 3, 5, 0, 0},
    {30, LV_ADV(4), 3, 5, 0, 0},
};

static const mui_font_t lv_mono_font = {
    lv_test_bitmap, lv_mono_glyphs, lv_test_cmap, 1, 0, 0, 8, 2
};

/**
 * @brief 内部：逐像素比对当前帧缓冲与参考帧
 * @param msg 断言失败时的描述
 */
static void check_fb_equal(const char *msg)
{
    int32_t i;
    int32_t diff = 0;

    mui_screen_flush();   /* 缓冲后端：比对前先推屏 */
    for (i = 0; i < (int32_t)SIM_W * SIM_H; i++) {
        if (sim_get_fb()[i] != label_expect[i]) {
            diff++;
        }
    }
    if (diff != 0) {
        printf("[FAIL] %s（不一致像素 %d，line %d）\n", msg, (int)diff, __LINE__);
        test_failed++;
    }
}

/**
 * @brief 内部：对给定字体跑一组增量更新用例，与整串重绘逐像素比对
 * @param font  字体（测试用 mui_font_t）
 * @param cases 用例表，每项 [初值, 终值]
 * @param n     用例数
 * @param who   字体名（用于失败信息）
 */
static void label_diff_run(const void *font, const char *cases[][2],
                           size_t n, const char *who)
{
    const int16_t x = 40;
    const int16_t y = 40;
    mui_label_t lbl;
    mui_label_t ref;
    char msg[96];
    size_t i;

    for (i = 0; i < n; i++) {
        /* 参考：干净背景上一次性绘制最终文本 */
        mui_screen_clear(TEST_BG);
        mui_label_init(&ref, x, y, font, MUI_BLACK, TEST_BG, 1);
        mui_label_set_text(&ref, cases[i][1]);
        mui_screen_flush();   /* 缓冲后端：取参考帧前先推屏 */
        memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

        /* 实际：先画初值，再增量更新到最终文本 */
        mui_screen_clear(TEST_BG);
        mui_label_init(&lbl, x, y, font, MUI_BLACK, TEST_BG, 1);
        mui_label_set_text(&lbl, cases[i][0]);
        mui_label_set_text(&lbl, cases[i][1]);

        sprintf(msg, "label %s 用例%d 增量与整串不一致", who, (int)i);
        check_fb_equal(msg);
    }
}

/**
 * @brief 测试：增量更新后的画面必须与整串重绘完全一致
 */
static void test_label_diff(void)
{
    /* LVGL 字体：adv_w 各异（比例步进），字符宽度不同，最容易暴露起点算错 */
    static const char *cases_lv[][2] = {
        {"123", "121"},
        {"123", "223"},
        {"1",   "123"},
        {"123", "1"},
        {"123", "113"},
        {"123", "123"},
        /* UTF-8：公共前缀停在多字节字符内部，须回退到首字节边界 */
        {"a\xE4\xBD\xA0", "a\xE4\xBB\xA0"},
        {"a\xE4\xBD\xA0" "b", "ab\xE4\xBD\xA0"},
    };
    /* 等宽字体 + 等长文本：命中 MUI_LABEL_OPA_COVER 整格覆盖快路径 */
    static const char *cases_mono[][2] = {
        {"123", "121"},
        {"123", "333"},
        {"113", "121"},
        {"123", "123"},
    };
    mui_label_t lbl;

    /* 空指针 / 未配字体安全：一律不崩、不绘制 */
    mui_label_set_text(NULL, "X");
    mui_label_init(&lbl, 40, 40, NULL, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, NULL);
    mui_label_set_text(&lbl, "123");
    mui_label_draw(&lbl);

    label_diff_run(&lv_test_font, cases_lv,
                   sizeof(cases_lv) / sizeof(cases_lv[0]), "LVGL");
    label_diff_run(&lv_mono_font, cases_mono,
                   sizeof(cases_mono) / sizeof(cases_mono[0]), "LVGL等宽");
}

/**
 * @brief 测试：增量更新不触碰差异区间之外的像素，相同文本不产生写入
 */
static void test_label_span(void)
{
    mui_label_t lbl;
    const int16_t x = 40;
    const int16_t y = 40;
    /* 等宽测试字体 "123" 宽 12px（每字 4px），哨兵放 x+13：
     * 增量擦只覆盖被改字符那一格（x+8..x+11），只有"整块擦"才会波及 x+13 */
    const int16_t sx = (int16_t)(x + 13);
    const int16_t sy = 43;

    mui_screen_clear(TEST_BG);
    mui_label_init(&lbl, x, y, &lv_mono_font, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, "123");

    mui_pixel_draw(sx, sy, MUI_GREEN);
    mui_label_set_text(&lbl, "121");
    CHECK(px(sx, sy) == MUI_GREEN, "增量更新未擦除差异区间外的像素");

    /* 文本完全相同时不擦不画 */
    mui_screen_clear(TEST_BG);
    mui_label_init(&lbl, x, y, &lv_mono_font, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, "123");
    mui_pixel_draw(sx, sy, MUI_GREEN);
    mui_label_set_text(&lbl, "123");
    CHECK(px(sx, sy) == MUI_GREEN, "相同文本不擦不画");
}

/**
 * @brief 测试：精确模式下只重画变化字符，不触碰前一个字符的区域
 */
static void test_label_exact_span(void)
{
#if MUI_LABEL_SAFE_SPAN
    /* 安全模式：重画起点本就回退一个字符，本用例不适用 */
    return;
#else
    mui_label_t lbl;
    const int16_t x = 40;
    const int16_t y = 40;
    /* 等宽测试字体 "123"→"121"：差异在末字符，其格为 x+8..x+11，
       擦除起点即 x+8。哨兵放在 x+7（前缀末字符格内）——
       精确模式自 x+8 起擦不波及，安全模式自 x+4 起会被擦掉 */
    const int16_t sx = (int16_t)(x + 7);

    mui_screen_clear(TEST_BG);
    mui_label_init(&lbl, x, y, &lv_mono_font, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, "123");

    mui_pixel_draw(sx, 43, MUI_GREEN);
    mui_label_set_text(&lbl, "121");
    CHECK(px(sx, 43) == MUI_GREEN, "精确模式未重画前缀字符");
#endif
}

/* -------- 测试：文字装饰（加粗 / 下划线 / 删除线） -------- */

/** @brief base_line = 0 的测试字体：下划线会落到行框外，专门覆盖那条分支 */
static const mui_font_t lv_base0_font = {
    lv_test_bitmap, lv_test_glyphs, lv_test_cmap, 1, 0, 0, 8, 0
};

/**
 * @brief 测试：flags = 0 必须与无装饰旧 API 逐像素一致（零行为变化）
 */
static void test_text_flags_zero(void)
{
    const int16_t x = 30;
    const int16_t y = 40;

    /* 稀疏绘制版 */
    mui_screen_clear(TEST_BG);
    mui_text_draw(x, y, "123", &lv_test_font, MUI_BLACK, TEST_BG, 1);
    mui_screen_flush();
    memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

    mui_screen_clear(TEST_BG);
    mui_text_draw_ex(x, y, "123", &lv_test_font, MUI_BLACK, TEST_BG, 1, 0);
    check_fb_equal("flags=0 与 mui_text_draw 不一致");

    /* 整格覆盖版 */
    mui_screen_clear(TEST_BG);
    mui_text_draw_cell(x, y, "123", &lv_test_font, MUI_BLACK, TEST_BG, 1);
    mui_screen_flush();
    memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

    mui_screen_clear(TEST_BG);
    mui_text_draw_cell_ex(x, y, "123", &lv_test_font, MUI_BLACK, TEST_BG, 1, 0);
    check_fb_equal("flags=0 与 mui_text_draw_cell 不一致");

    /* 空指针 / 非法 scale：不崩不画 */
    mui_text_draw_ex(x, y, "123", NULL, MUI_BLACK, TEST_BG, 1, MUI_TEXT_BOLD);
    mui_text_draw_ex(x, y, NULL, &lv_test_font, MUI_BLACK, TEST_BG, 1, MUI_TEXT_BOLD);
    mui_text_draw_ex(x, y, "123", &lv_test_font, MUI_BLACK, TEST_BG, 0, MUI_TEXT_BOLD);
    mui_text_draw_cell_ex(x, y, "123", NULL, MUI_BLACK, TEST_BG, 1, MUI_TEXT_UNDERLINE);
    mui_text_draw_cell_ex(x, y, NULL, &lv_test_font, MUI_BLACK, TEST_BG, 1, MUI_TEXT_UNDERLINE);
    CHECK(mui_text_height(NULL, 1, MUI_TEXT_UNDERLINE) == 0, "字体为 NULL 时高度应为 0");
    CHECK(mui_text_height(&lv_test_font, 0, MUI_TEXT_UNDERLINE) == 0, "scale=0 时高度应为 0");
    CHECK(mui_text_height(&lv_test_font, 1, 0) == lv_test_font.line_height,
          "无装饰高度应等于行框高");
}

/**
 * @brief 测试：加粗 = 形态学膨胀 —— 只加墨不减墨、不外扩出字形框、步进不变
 */
static void test_text_bold(void)
{
    const int16_t x = 30;
    const int16_t y = 40;
    const mui_font_t *f = &lv_mono_font;          /* 3x5 字形，box 3x5，adv 4px */
    const mui_glyph_dsc_t *g = &lv_mono_glyphs[0];/* '1' */
    const int16_t gx = (int16_t)(x + g->ofs_x);
    const int16_t gy = (int16_t)(y + (f->line_height - f->base_line
                                      - g->box_h - g->ofs_y));
    int ink_plain = 0;
    int ink_bold = 0;
    int16_t row, col;

    mui_screen_clear(TEST_BG);
    mui_text_draw_ex(x, y, "1", f, MUI_BLACK, TEST_BG, 1, 0);
    mui_screen_flush();
    memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

    mui_screen_clear(TEST_BG);
    mui_text_draw_ex(x, y, "1", f, MUI_BLACK, TEST_BG, 1, MUI_TEXT_BOLD);

    for (row = 0; row < g->box_h; row++) {
        for (col = -1; col <= g->box_w; col++) {
            int16_t cx = (int16_t)(gx + col);
            int16_t cy = (int16_t)(gy + row);
            uint16_t want = (col >= 0 && col < g->box_w)
                            ? label_expect[cy * SIM_W + cx] : TEST_BG;
            uint16_t got = px(cx, cy);

            if (want == MUI_BLACK) {
                ink_plain++;
                CHECK(got == MUI_BLACK, "加粗后原墨迹丢失");
            }
            if (got == MUI_BLACK) {
                ink_bold++;
            }
            if (col < 0 || col >= g->box_w) {
                /* 膨胀被裁到字形框内：框外一圈不允许出现新墨 */
                CHECK(got == TEST_BG, "加粗外扩出字形框");
            }
        }
    }
    CHECK(ink_bold > ink_plain, "加粗后墨迹像素数应增加");
    CHECK(ink_plain > 0, "参考字形应有墨（测试前提）");

    /* 步进不变：加粗的 "11" == 两次单字加粗（第二字起于 x + 步进） */
    mui_screen_clear(TEST_BG);
    mui_text_draw_ex(x, y, "11", f, MUI_BLACK, TEST_BG, 1, MUI_TEXT_BOLD);
    mui_screen_flush();
    memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

    mui_screen_clear(TEST_BG);
    mui_text_draw_ex(x, y, "1", f, MUI_BLACK, TEST_BG, 1, MUI_TEXT_BOLD);
    mui_text_draw_ex((int16_t)(x + 4), y, "1", f, MUI_BLACK, TEST_BG, 1, MUI_TEXT_BOLD);
    check_fb_equal("加粗改变了步进宽度（第二字起点漂移）");
}

/**
 * @brief 逐像素比对：除指定行区间外，其余像素必须与参考帧完全一致
 * @param row0 允许不同的起始行（含）
 * @param row1 允许不同的结束行（含）
 * @param msg  断言失败时的描述
 */
static void check_fb_equal_except_rows(int16_t row0, int16_t row1, const char *msg)
{
    int32_t diff = 0;
    int x, y;

    mui_screen_flush();   /* 缓冲后端：比对前先推屏 */
    for (y = 0; y < SIM_H; y++) {
        if (y >= row0 && y <= row1) {
            continue;
        }
        for (x = 0; x < SIM_W; x++) {
            if (sim_get_fb()[y * SIM_W + x] != label_expect[y * SIM_W + x]) {
                diff++;
            }
        }
    }
    if (diff != 0) {
        printf("[FAIL] %s（区间外不一致像素 %d，line %d）\n", msg, (int)diff, __LINE__);
        test_failed++;
    }
}

/**
 * @brief 测试：下划线 / 删除线的位置、宽度与覆盖行数
 * @note  位置口径（与实现同源的"排版意图"）：基线 = 行框顶 + (line_height - base_line)，
 *        下划线在基线下方 1px（随 scale 放大），删除线在行框顶与基线的中点。
 *        除装饰线所在行外，画面必须与无装饰时逐像素一致（证明没有多画/少画别的行）。
 */
static void test_text_decor_lines(void)
{
    static const struct {
        const mui_font_t *f;
        const char *name;
    } fonts[] = {
        {&lv_test_font,  "base_line=2"},
        {&lv_base0_font, "base_line=0"},
    };
    const int16_t x = 30;
    const int16_t y = 40;
    size_t i;

    for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        const mui_font_t *f = fonts[i].f;
        int16_t base = (int16_t)(f->line_height - f->base_line);
        int16_t w = mui_text_width("123", f, 1);
        int16_t uy = (int16_t)(y + base + 1);        /* 下划线：基线下方 1px */
        int16_t sy = (int16_t)(y + base / 2);        /* 删除线：行顶与基线中点 */
        int16_t h_ul = mui_text_height(f, 1, MUI_TEXT_UNDERLINE);
        int16_t c;

        CHECK(h_ul >= (int16_t)(uy - y + 1), "装饰高度应含下划线那一行");
        CHECK(h_ul >= f->line_height, "装饰高度不应小于行框高");
        CHECK(mui_text_height(f, 1, MUI_TEXT_STRIKE) == f->line_height,
              "删除线在行框内，不改变高度");

        /* 参考帧：同字体、同位置、无装饰 */
        mui_screen_clear(TEST_BG);
        mui_text_draw_ex(x, y, "123", f, MUI_BLACK, TEST_BG, 1, 0);
        mui_screen_flush();
        memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

        /* 只开下划线：整串一条连续线，两端各多 1px 无墨，线宽 1，其它行不变 */
        mui_screen_clear(TEST_BG);
        mui_text_draw_ex(x, y, "123", f, MUI_BLACK, TEST_BG, 1, MUI_TEXT_UNDERLINE);
        for (c = 0; c < w; c++) {
            CHECK(px((int16_t)(x + c), uy) == MUI_BLACK, "下划线不连续");
        }
        CHECK(px((int16_t)(x - 1), uy) == TEST_BG, "下划线左端越界");
        CHECK(px((int16_t)(x + w), uy) == TEST_BG, "下划线右端越界");
        CHECK(px(x, (int16_t)(uy + 1)) == TEST_BG, "下划线过宽（线宽应为 1）");
        check_fb_equal_except_rows(uy, uy, "只开下划线时有别的行被改动");

        /* 只开删除线：在行框内，其它行不变（含下划线那一行） */
        mui_screen_clear(TEST_BG);
        mui_text_draw_ex(x, y, "123", f, MUI_BLACK, TEST_BG, 1, MUI_TEXT_STRIKE);
        for (c = 0; c < w; c++) {
            CHECK(px((int16_t)(x + c), sy) == MUI_BLACK, "删除线不连续");
        }
        CHECK(sy < (int16_t)(y + f->line_height), "删除线应落在行框内");
        check_fb_equal_except_rows(sy, sy, "只开删除线时有别的行被改动");

        /* 两者同时开：两条线都在，且只改动这两行 */
        mui_screen_clear(TEST_BG);
        mui_text_draw_ex(x, y, "123", f, MUI_BLACK, TEST_BG, 1,
                         (uint8_t)(MUI_TEXT_UNDERLINE | MUI_TEXT_STRIKE));
        CHECK(px(x, uy) == MUI_BLACK, "下划线+删除线：下划线缺失");
        CHECK(px(x, sy) == MUI_BLACK, "下划线+删除线：删除线缺失");
        check_fb_equal_except_rows((int16_t)(sy < uy ? sy : uy),
                                   (int16_t)(sy < uy ? uy : sy),
                                   "两条装饰线同时开时改动了其它行");

        /* scale = 2：位置按 scale 放大、线宽 = scale */
        {
            int16_t uy2 = (int16_t)(y + base * 2 + 2);
            int16_t h2 = mui_text_height(f, 2, MUI_TEXT_UNDERLINE);

            CHECK(h2 >= (int16_t)(uy2 - y + 2), "scale=2 时下划线超出覆盖行数");
            mui_screen_clear(TEST_BG);
            mui_text_draw_ex(x, y, "123", f, MUI_BLACK, TEST_BG, 2, MUI_TEXT_UNDERLINE);
            CHECK(px(x, uy2) == MUI_BLACK, "scale=2 下划线首行缺失");
            CHECK(px(x, (int16_t)(uy2 + 1)) == MUI_BLACK, "scale=2 下划线应为 2px");
            CHECK(px(x, (int16_t)(uy2 + 2)) == TEST_BG, "scale=2 下划线过宽");

            /* 与 scale=2 无装饰帧比对（只有那两行允许不同） */
            mui_screen_clear(TEST_BG);
            mui_text_draw_ex(x, y, "123", f, MUI_BLACK, TEST_BG, 2, 0);
            mui_screen_flush();
            memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

            mui_screen_clear(TEST_BG);
            mui_text_draw_ex(x, y, "123", f, MUI_BLACK, TEST_BG, 2, MUI_TEXT_UNDERLINE);
            check_fb_equal_except_rows(uy2, (int16_t)(uy2 + 1),
                                       "scale=2 下划线改动了别的行");
        }
    }
}

/**
 * @brief 测试：整格覆盖版与稀疏版在带装饰时逐像素一致（两条路径可互换）
 */
static void test_text_cell_matches_sparse(void)
{
    static const uint8_t decors[] = {
        MUI_TEXT_BOLD,
        MUI_TEXT_UNDERLINE,
        MUI_TEXT_STRIKE,
        (uint8_t)(MUI_TEXT_BOLD | MUI_TEXT_UNDERLINE | MUI_TEXT_STRIKE),
    };
    static const struct {
        const mui_font_t *f;
        const char *name;
    } fonts[] = {
        {&lv_mono_font,  "等宽"},
        {&lv_base0_font, "base_line=0"},
    };
    static const int16_t scales[] = {1, 2};
    const int16_t x = 30;
    const int16_t y = 40;
    char msg[96];
    size_t i, j, k;

    for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        for (j = 0; j < sizeof(decors) / sizeof(decors[0]); j++) {
            for (k = 0; k < sizeof(scales) / sizeof(scales[0]); k++) {
                mui_screen_clear(TEST_BG);
                mui_text_draw_ex(x, y, "12", fonts[i].f, MUI_BLACK, TEST_BG,
                                 scales[k], decors[j]);
                mui_screen_flush();
                memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

                mui_screen_clear(TEST_BG);
                mui_text_draw_cell_ex(x, y, "12", fonts[i].f, MUI_BLACK, TEST_BG,
                                      scales[k], decors[j]);

                sprintf(msg, "整格覆盖与稀疏绘制不一致（%s decor=%d scale=%d）",
                        fonts[i].name, (int)decors[j], (int)scales[k]);
                check_fb_equal(msg);
            }
        }
    }
}

/**
 * @brief 测试：标签带装饰时的增量更新必须与整串重绘一致
 * @note  scale=2 覆盖"整格覆盖行数 = line_height * scale"（含行框外的下划线行）
 */
static void label_decor_run(const mui_font_t *font, uint8_t decor, int16_t scale,
                            const char *who)
{
    static const char *cases[][2] = {
        {"123", "121"},
        {"123", "333"},
        {"113", "121"},
        {"1",   "123"},
        {"123", "1"},
    };
    const int16_t x = 40;
    const int16_t y = 40;
    mui_label_t lbl;
    mui_label_t ref;
    char msg[96];
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        /* 参考：干净背景上一次性绘制最终文本 */
        mui_screen_clear(TEST_BG);
        mui_label_init(&ref, x, y, font, MUI_BLACK, TEST_BG, scale);
        mui_label_set_decor(&ref, decor);
        mui_label_set_text(&ref, cases[i][1]);
        mui_screen_flush();
        memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

        /* 实际：先画初值，再增量更新到最终文本 */
        mui_screen_clear(TEST_BG);
        mui_label_init(&lbl, x, y, font, MUI_BLACK, TEST_BG, scale);
        mui_label_set_decor(&lbl, decor);
        mui_label_set_text(&lbl, cases[i][0]);
        mui_label_set_text(&lbl, cases[i][1]);

        sprintf(msg, "label %s decor=%d scale=%d 用例%d 增量与整串不一致",
                who, (int)decor, (int)scale, (int)i);
        check_fb_equal(msg);
    }
}

/**
 * @brief 测试：标签装饰相关
 */
static void test_label_decor(void)
{
    const int16_t x = 40;
    const int16_t y = 40;
    mui_label_t lbl;
    mui_label_t ref;

    /* 各装饰 × 两种字体（命中整格覆盖 / 精确增量两条路径）× 两种放大倍数 */
    label_decor_run(&lv_mono_font, 0, 1, "无装饰");
    label_decor_run(&lv_mono_font, MUI_TEXT_BOLD, 1, "加粗");
    label_decor_run(&lv_mono_font, MUI_TEXT_UNDERLINE, 1, "下划线");
    label_decor_run(&lv_mono_font, MUI_TEXT_STRIKE, 1, "删除线");
    label_decor_run(&lv_mono_font,
                    (uint8_t)(MUI_TEXT_BOLD | MUI_TEXT_UNDERLINE | MUI_TEXT_STRIKE), 2,
                    "全装饰");
    label_decor_run(&lv_test_font, MUI_TEXT_UNDERLINE, 1, "下划线");
    label_decor_run(&lv_base0_font, MUI_TEXT_UNDERLINE, 1, "下划线(行框外)");
    label_decor_run(&lv_base0_font, MUI_TEXT_UNDERLINE, 2, "下划线(行框外)");

    /* 先带下划线画，再关掉：旧装饰线必须被擦除（行框外那条尤其容易残留） */
    mui_screen_clear(TEST_BG);
    mui_label_init(&ref, x, y, &lv_base0_font, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&ref, "123");
    mui_screen_flush();
    memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

    mui_screen_clear(TEST_BG);
    mui_label_init(&lbl, x, y, &lv_base0_font, MUI_BLACK, TEST_BG, 1);
    mui_label_set_decor(&lbl, MUI_TEXT_UNDERLINE);
    mui_label_set_text(&lbl, "123");
    mui_label_set_decor(&lbl, 0);
    check_fb_equal("关闭下划线后旧装饰线未擦除");

    /* 装饰未变化时不应重绘（与 set_text 的"文本未变不擦不画"一致） */
    mui_screen_clear(TEST_BG);
    mui_label_init(&lbl, x, y, &lv_mono_font, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, "123");
    mui_pixel_draw((int16_t)(x + 13), 43, MUI_GREEN);
    mui_label_set_decor(&lbl, 0);
    CHECK(px((int16_t)(x + 13), 43) == MUI_GREEN, "装饰未变化时不应重绘");
}

/**
 * @brief 测试：改色 setter —— 结果必须与"直接以新色绘制"一致，装饰线跟着一起变
 * @note  set_fg 不动几何（同字形重画即覆盖旧墨）→ 可与"新色首绘"做全屏逐像素比对；
 *        set_colors 会换底色（擦除用新 bg），故只断言"覆盖区内不再有旧色 + 覆盖区外不动"。
 */
static void test_label_colors(void)
{
    static const mui_font_t *fonts[] = {&lv_mono_font, &lv_base0_font};
    static const uint8_t decors[] = {
        0,
        MUI_TEXT_UNDERLINE,
        (uint8_t)(MUI_TEXT_BOLD | MUI_TEXT_UNDERLINE | MUI_TEXT_STRIKE),
    };
    const uint16_t fg_new = MUI_RED;
    const uint16_t bg_new = MUI_LIGHTGREY;
    const int16_t x = 40;
    const int16_t y = 40;
    mui_label_t lbl;
    mui_label_t ref;
    char msg[96];
    size_t i, j;

    for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        for (j = 0; j < sizeof(decors) / sizeof(decors[0]); j++) {
            int16_t w, h, row, col;

            /* ---- 参考：干净底色上直接以新色首绘 ---- */
            mui_screen_clear(TEST_BG);
            mui_label_init(&ref, x, y, fonts[i], fg_new, TEST_BG, 1);
            mui_label_set_decor(&ref, decors[j]);
            mui_label_set_text(&ref, "123");
            mui_screen_flush();
            memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

            /* ---- 实际：旧色画好后 set_fg 换色（不擦除，靠同位置重画覆盖） ---- */
            mui_screen_clear(TEST_BG);
            mui_label_init(&lbl, x, y, fonts[i], MUI_BLACK, TEST_BG, 1);
            mui_label_set_decor(&lbl, decors[j]);
            mui_label_set_text(&lbl, "123");
            mui_label_set_fg(&lbl, fg_new);

            sprintf(msg, "set_fg 后与直接以新色绘制不一致（font%u decor=%d）",
                    (unsigned)i, (int)decors[j]);
            check_fb_equal(msg);

            /* ---- set_colors：底色也换 ---- */
            mui_screen_clear(TEST_BG);
            mui_label_init(&lbl, x, y, fonts[i], MUI_BLACK, TEST_BG, 1);
            mui_label_set_decor(&lbl, decors[j]);
            mui_label_set_text(&lbl, "123");
            w = lbl.last_w;
            h = lbl.last_h;
            mui_label_set_colors(&lbl, fg_new, bg_new);

            /* 覆盖区内只允许出现新底色与新字色（旧底色/旧字色说明擦除或重画漏了） */
            for (row = (int16_t)(y - 2); row < (int16_t)(y + h + 2); row++) {
                for (col = (int16_t)(x - 2); col < (int16_t)(x + w + 2); col++) {
                    uint16_t c = px(col, row);
                    CHECK(c == bg_new || c == fg_new, "set_colors 后覆盖区内残留旧色");
                }
            }
            /* 覆盖区外不被触碰 */
            CHECK(px((int16_t)(x - 4), (int16_t)(y - 4)) == TEST_BG,
                  "set_colors 越界修改了覆盖区外");
            CHECK(px((int16_t)(x + w + 3), (int16_t)(y + h + 3)) == TEST_BG,
                  "set_colors 越界修改了覆盖区外");

            /* 装饰线必须跟着字色一起变（用的是同一个 fg） */
            if (decors[j] & MUI_TEXT_UNDERLINE) {
                int16_t uy = (int16_t)(y + fonts[i]->line_height
                                       - fonts[i]->base_line + 1);
                for (col = 0; col < w; col++) {
                    CHECK(px((int16_t)(x + col), uy) == fg_new, "下划线没跟着字色变");
                }
            }
        }

        /* ---- 颜色未变：不重绘（哨兵像素不被触碰） ---- */
        mui_screen_clear(TEST_BG);
        mui_label_init(&lbl, x, y, fonts[i], MUI_BLACK, TEST_BG, 1);
        mui_label_set_text(&lbl, "1");
        mui_pixel_draw((int16_t)(x + 13), 43, MUI_GREEN);
        mui_label_set_fg(&lbl, MUI_BLACK);
        mui_label_set_colors(&lbl, MUI_BLACK, TEST_BG);
        CHECK(px((int16_t)(x + 13), 43) == MUI_GREEN, "颜色未变时不应重绘");
    }

    /* 空指针安全 */
    mui_label_set_fg(NULL, MUI_RED);
    mui_label_set_colors(NULL, MUI_RED, MUI_BLACK);
    mui_button_set_style(NULL, NULL);
}

/* -------- 测试：抗锯齿圆角矩形 -------- */

/** @brief 参考帧（存放参考实现渲染结果，用于逐像素比对） */
static uint16_t aa_expect[SIM_W * SIM_H];

/**
 * @brief 参考用覆盖率：独立用浮点 sqrt 计算（不受库内整数开方实现影响）
 * @note  与库内公式等价：覆盖率 = clamp(r + 1 - 像素中心到角圆心距离, 0, 1)
 *        （半径按 r+0.5 像素，再叠加半像素的边缘修正）
 */
static uint8_t aa_ref_alpha(int16_t ux, int16_t uy, int16_t r)
{
    double dist = 0.5 * sqrt((double)((int32_t)ux * ux + (int32_t)uy * uy));
    double cov = (double)(r + 1) - dist;

    if (cov <= 0.0) {
        return 0;
    }
    if (cov >= 1.0) {
        return 255;
    }
    return (uint8_t)(cov * 255.0 + 0.5);
}

/** @brief 参考实现：主体批量填充 + 四个角区逐像素混色（改造前的做法，浮点覆盖率） */
static void aa_ref_render(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                          uint16_t fg, uint16_t bg)
{
    int16_t limit = (w < h) ? (int16_t)(w / 2) : (int16_t)(h / 2);
    int16_t corners[4][2];

    if (r > limit) {
        r = limit;
    }
    mui_rect_fill((int16_t)(x + r), y, (int16_t)(w - 2 * r), h, fg);
    mui_rect_fill(x, (int16_t)(y + r), r, (int16_t)(h - 2 * r), fg);
    mui_rect_fill((int16_t)(x + w - r), (int16_t)(y + r), r,
                  (int16_t)(h - 2 * r), fg);

    corners[0][0] = (int16_t)(x + r);              corners[0][1] = (int16_t)(y + r);
    corners[1][0] = (int16_t)(x + w - 1 - r);      corners[1][1] = (int16_t)(y + r);
    corners[2][0] = (int16_t)(x + r);              corners[2][1] = (int16_t)(y + h - 1 - r);
    corners[3][0] = (int16_t)(x + w - 1 - r);      corners[3][1] = (int16_t)(y + h - 1 - r);

    for (int c = 0; c < 4; c++) {
        int16_t sqx = (c & 1) ? (int16_t)(x + w - r) : x;
        int16_t sqy = (c & 2) ? (int16_t)(y + h - r) : y;

        for (int16_t yy = sqy; yy < (int16_t)(sqy + r); yy++) {
            for (int16_t xx = sqx; xx < (int16_t)(sqx + r); xx++) {
                uint8_t a = aa_ref_alpha((int16_t)(2 * (xx - corners[c][0])),
                                         (int16_t)(2 * (yy - corners[c][1])), r);
                if (a != 0) {
                    mui_pixel_draw(xx, yy, (a == 255) ? fg : mui_color_mix(fg, bg, a));
                }
            }
        }
    }
}

/** @brief 两个颜色每个通道的差值是否都在 tol 以内（RGB565 分通道比较） */
static int color_close(uint16_t a, uint16_t b, int tol)
{
    int dr = (int)((a >> 11) & 0x1F) - (int)((b >> 11) & 0x1F);
    int dg = (int)((a >> 5) & 0x3F) - (int)((b >> 5) & 0x3F);
    int db = (int)(a & 0x1F) - (int)(b & 0x1F);

    if (dr < 0) { dr = -dr; }
    if (dg < 0) { dg = -dg; }
    if (db < 0) { db = -db; }
    return (dr <= tol && dg <= tol && db <= tol);
}

/**
 * @brief 把"黑前景 + 白背景"下的混色像素反推回 alpha（仅测试用）
 * @note  mui_color_mix(MUI_BLACK, MUI_WHITE, a) 的绿通道 ≈ 63*a/255，且单调
 */
static uint8_t alpha_of(uint16_t c)
{
    int g = (int)((c >> 5) & 0x3F);      /* 6 位绿通道 */

    if (g > 63) {
        g = 63;
    }
    return (uint8_t)((63 - g) * 255 / 63);
}

/**
 * @brief 测试：抗锯齿圆角矩形 —— 与"逐像素 + 浮点覆盖率"的参考实现逐像素一致
 * @note  允许每通道 ±1 的偏差：库内用整数开方（floor），参考用浮点，
 *        覆盖率换算后 alpha 最多差 1 级
 */
static void test_round_rect_aa(void)
{
    static const int16_t shapes[][5] = {
        {10, 10, 60, 40, 12},
        {30, 60, 50, 40, 15},
        {5,  90, 40, 30, 8},
        {70, 20, 30, 30, 4},
        {20, 20, 13, 7,  3},
        {100, 60, 25, 25, 12},
    };
    int32_t i;
    size_t s;

    for (s = 0; s < sizeof(shapes) / sizeof(shapes[0]); s++) {
        int16_t x = shapes[s][0], y = shapes[s][1], w = shapes[s][2];
        int16_t h = shapes[s][3], r = shapes[s][4];
        int bad = 0;
        char msg[96];

        mui_screen_clear(TEST_BG);
        aa_ref_render(x, y, w, h, r, MUI_BLUE, TEST_BG);
        mui_screen_flush();
        memcpy(aa_expect, sim_get_fb(), sizeof(aa_expect));

        mui_screen_clear(TEST_BG);
        mui_round_rect_fill_aa(x, y, w, h, r, MUI_BLUE, TEST_BG);
        mui_screen_flush();

        for (i = 0; i < (int32_t)SIM_W * SIM_H; i++) {
            if (!color_close(sim_get_fb()[i], aa_expect[i], 1)) {
                bad++;
            }
        }
        sprintf(msg, "AA圆角矩形 %dx%d r=%d 与参考实现不一致", (int)w, (int)h, (int)r);
        CHECK(bad == 0, msg);
    }
}

/**
 * @brief 测试：AA 边缘的底色来源 —— 缓冲后端应回读真实底色，直绘后端用传入 bg
 */
static void test_round_rect_aa_bg(void)
{
    const int16_t x = 30, y = 30, w = 60, h = 40, r = 14;
    int16_t fx = -1, fy = -1;
    uint8_t a = 0;
    uint16_t got;

    /* 找一个落在圆角过渡带上的像素（覆盖率 0<a<255） */
    for (int16_t yy = y; yy < (int16_t)(y + r) && fx < 0; yy++) {
        for (int16_t xx = x; xx < (int16_t)(x + r); xx++) {
            uint8_t t = aa_ref_alpha((int16_t)(2 * (xx - (x + r))),
                                     (int16_t)(2 * (yy - (y + r))), r);
            if (t > 0 && t < 255) {
                fx = xx;
                fy = yy;
                a = t;
                break;
            }
        }
    }
    CHECK(fx >= 0, "未找到圆角过渡带像素（用例本身有问题）");
    if (fx < 0) {
        return;
    }

    /* 红底 + 故意传错的 bg(黑)：缓冲后端该像素必须是"与红混合"，而不是"与黑混合" */
    mui_screen_clear(MUI_RED);
    mui_round_rect_fill_aa(x, y, w, h, r, MUI_WHITE, MUI_BLACK);
    mui_screen_flush();
    got = sim_get_fb()[fy * SIM_W + fx];

#if MUI_CFG_HAS_BUFFER
    CHECK(color_close(got, mui_color_mix(MUI_WHITE, MUI_RED, a), 1),
          "缓冲后端：AA 边缘未与屏幕真实底色混合");
    CHECK(got != mui_color_mix(MUI_WHITE, MUI_BLACK, a),
          "缓冲后端：AA 边缘误用了传入的 bg");
#else
    CHECK(color_close(got, mui_color_mix(MUI_WHITE, MUI_BLACK, a), 1),
          "直绘后端：AA 边缘应按传入 bg 混合");
#endif
}


/* -------- 测试：AA 空心圆角矩形（1px 描边，四角圆弧混色） -------- */

#define RRD_X (10)
#define RRD_Y (10)
#define RRD_W (60)
#define RRD_H (40)
#define RRD_R (12)

/** @brief 扫描区域：非背景像素的包围盒（相对坐标）与"混色像素"数量 */
static void rrd_scan(int x0, int y0, int w, int h, uint16_t fg, uint16_t bg,
                     int *bb, int *blend)
{
    int i, j;

    mui_screen_flush();
    bb[0] = w;
    bb[1] = h;
    bb[2] = -1;
    bb[3] = -1;
    *blend = 0;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint16_t c = sim_get_fb()[(y0 + j) * SIM_W + (x0 + i)];

            if (c == bg) {
                continue;
            }
            if (i < bb[0]) { bb[0] = i; }
            if (j < bb[1]) { bb[1] = j; }
            if (i > bb[2]) { bb[2] = i; }
            if (j > bb[3]) { bb[3] = j; }
            if (c != fg) {
                (*blend)++;
            }
        }
    }
}

static void test_round_rect_draw_aa(void)
{
    int bb_hard[4], bb_aa[4];
    int bl_hard, bl_aa;
    int c, bad = 0, bad_ink = 0;

    /* 黑描边 + 白底：混色像素的绿通道直接可反推 alpha（见 alpha_of） */
    mui_screen_clear(TEST_BG);
    mui_round_rect_draw(RRD_X, RRD_Y, RRD_W, RRD_H, RRD_R, MUI_BLACK);
    rrd_scan(RRD_X - 3, RRD_Y - 3, RRD_W + 6, RRD_H + 6, MUI_BLACK, TEST_BG,
             bb_hard, &bl_hard);

    mui_screen_clear(TEST_BG);
    mui_round_rect_draw_aa(RRD_X, RRD_Y, RRD_W, RRD_H, RRD_R, MUI_BLACK,
                           TEST_BG);
    rrd_scan(RRD_X - 3, RRD_Y - 3, RRD_W + 6, RRD_H + 6, MUI_BLACK, TEST_BG,
             bb_aa, &bl_aa);

    CHECK(bl_hard == 0, "硬边空心圆角矩形不应出现混色像素");
    CHECK(bl_aa > 20, "AA 空心圆角矩形未在四角产生混色像素");
    /* 轮廓范围必须一致：AA 只把台阶换成过渡色，不向外长一圈 */
    CHECK(bb_hard[0] == bb_aa[0] && bb_hard[1] == bb_aa[1]
          && bb_hard[2] == bb_aa[2] && bb_hard[3] == bb_aa[3],
          "AA 描边的轮廓范围与硬边版不一致");

    /* 四条直边必须是纯描边色（轴对齐、覆盖率恒为 1，不该混色） */
    mui_screen_flush();
    for (c = RRD_R + 1; c < RRD_W - RRD_R - 1; c++) {
        if (sim_get_fb()[RRD_Y * SIM_W + RRD_X + c] != MUI_BLACK) {
            bad++;
        }
        if (sim_get_fb()[(RRD_Y + RRD_H - 1) * SIM_W + RRD_X + c] != MUI_BLACK) {
            bad++;
        }
    }
    for (c = RRD_R + 1; c < RRD_H - RRD_R - 1; c++) {
        if (sim_get_fb()[(RRD_Y + c) * SIM_W + RRD_X] != MUI_BLACK) {
            bad++;
        }
        if (sim_get_fb()[(RRD_Y + c) * SIM_W + RRD_X + RRD_W - 1] != MUI_BLACK) {
            bad++;
        }
    }
    CHECK(bad == 0, "AA 描边的直边段不纯（不该混色或丢像素）");

    /* 圆角几何精度（两条独立性质，都在左上角）：
     *   ① 每列墨量质心贴合解析曲线 ρ(x)=sqrt(r²-x²)（Wu 的墨量质感上是"贴曲线"的）；
     *   ② 所有落墨像素都必须落在描边带内（|距离-r| ≤ 1.5px），不能有游离像素。
     * 只查避开直边所在的首末列。
     * 注：不校验"每列墨量和恒为 255"——Wu 每次迭代会把 255 分给**两个相邻列**，
     *     单列墨量本就随曲率在 255~360 间浮动（这是算法的固有性质，不是缺陷）。 */
    bad = 0;
    bad_ink = 0;
    for (c = 1; c <= RRD_R - 2; c++) {
        int cx = RRD_X + RRD_R;
        int cy = RRD_Y + RRD_R;
        int sum = 0, wsum = 0, dy;
        double rho = sqrt((double)RRD_R * RRD_R - (double)c * c);
        double cen;

        for (dy = 0; dy <= RRD_R + 2; dy++) {
            uint16_t col = sim_get_fb()[(cy - dy) * SIM_W + (cx - c)];
            int a;
            double d;

            if (col == TEST_BG) {
                continue;
            }
            a = alpha_of(col);
            sum += a;
            wsum += a * dy;
            d = sqrt((double)c * c + (double)dy * dy);
            if (fabs(d - (double)RRD_R) > 1.5) {
                bad_ink++;          /* 落墨跑到描边带外面去了 */
            }
        }
        if (sum == 0) {
            continue;
        }
        cen = (double)wsum / (double)sum;
        if (fabs(cen - rho) > 0.75) {
            bad++;
        }
    }
    CHECK(bad == 0, "AA 描边圆角的墨量质心偏离解析曲线超过 0.75px");
    CHECK(bad_ink == 0, "AA 描边圆角有超出描边带的游离像素");
}

/**
 * @brief 测试：未填充的那一段（槽）轮廓仍必须是抗锯齿的
 *
 * 两种到达方式都要查：
 *   a) 初次绘制就是部分进度（全量路径，erase = 0）；
 *   b) 先满进度再降到部分进度（增量路径，erase = 1 —— 擦除若按宽了/按纯槽色
 *      重画，就会把槽的圆角轮廓擦成硬台阶，低分辨率下一眼可见）。
 * @return 顶部角带内"混色像素"的个数（既非页面底色、也非纯槽色/纯填充色）
 */
static int pb_track_aa_blends(int16_t x, int16_t y, int16_t w, uint8_t first,
                              uint8_t then, uint8_t has_then)
{
    static const mui_progressbar_style_t st = {
        .bg = MUI_LIGHTGREY, .fg = MUI_BLUE, .border = MUI_LIGHTGREY,
        .radius = 0, .dir = MUI_PROGRESSBAR_VERTICAL, .aa = 1,
        .screen_bg = TEST_BG
    };
    static mui_progressbar_t pb;
    int row, i, blend = 0;

    mui_screen_clear(TEST_BG);
    mui_progressbar_init(&pb, x, y, w, 76, &st);
    mui_progressbar_set_value(&pb, first);
    if (has_then) {
        mui_progressbar_set_value(&pb, then);
    }
    mui_screen_flush();

    for (row = y; row < (int16_t)(y + 15); row++) {
        for (i = 0; i < w; i++) {
            uint16_t c = sim_get_fb()[row * SIM_W + x + i];

            if (c == TEST_BG || c == MUI_LIGHTGREY || c == MUI_BLUE) {
                continue;               /* 纯色：底/槽/填充 */
            }
            blend++;
        }
    }
    return blend;
}

static void test_progressbar_track_aa(void)
{
    int a = pb_track_aa_blends(20, 20, 30, 55, 0, 0);      /* 全量 → 部分 */
    int b = pb_track_aa_blends(20, 20, 30, 100, 55, 1);    /* 满 → 增量降到部分 */

    CHECK(a > 10, "初次即部分进度时，槽的圆角轮廓没有抗锯齿");
    CHECK(b > 10, "从满进度降档后，槽的圆角轮廓被擦成了硬台阶");
}

/**
 * @brief 测试：无边框进度条的填充必须"铺满底板"
 *
 * 这是"用几何 1:1 复刻位图版胶囊"的前提：位图版只有填充、没有槽，
 * 所以几何版在 border == bg（视觉无边框）时必须让填充覆盖整块底板，
 * 且轮廓与 mui_round_rect_fill_aa() 画出的同几何形状逐像素一致。
 * （曾经的 bug：填充按线宽退让 1px、角带只混 1 个像素 → 边沿一圈槽色。）
 */
static void test_progressbar_full_cover(void)
{
    static const mui_progressbar_style_t st = {
        .bg = MUI_LIGHTGREY, .fg = MUI_BLUE, .border = MUI_LIGHTGREY,
        .radius = 0, .dir = MUI_PROGRESSBAR_VERTICAL, .aa = 1,
        .screen_bg = TEST_BG
    };
    static mui_progressbar_t pb;
    const int16_t x = 20, y = 20, w = 30, h = 76, r = 15;  /* r = 自动值（限 w/2） */
    int i, bad = 0;

    /* 参考：同几何的实心 AA 圆角矩形 = 位图版胶囊的形状 */
    mui_screen_clear(TEST_BG);
    mui_round_rect_fill_aa(x, y, w, h, r, MUI_BLUE, TEST_BG);
    mui_screen_flush();
    memcpy(aa_expect, sim_get_fb(), sizeof(aa_expect));

    /* 进度条：无边框 + AA + 满进度 → 填充应把底板完全盖住 */
    mui_screen_clear(TEST_BG);
    mui_progressbar_init(&pb, x, y, w, h, &st);
    mui_progressbar_set_value(&pb, 100);
    mui_screen_flush();

    for (i = 0; i < SIM_W * SIM_H; i++) {
        if (!color_close(sim_get_fb()[i], aa_expect[i], 1)) {
            bad++;
        }
    }
    CHECK(bad == 0, "满进度 + 无边框时，填充未与底板（AA 圆角形状）逐像素吻合");
}

/* -------- 测试：进度条填充的圆角抗锯齿（同一几何，只差 style.aa） -------- */

#define PBAA_X_HARD (10)     /**< 硬边条左边界 */
#define PBAA_X_AA   (50)     /**< AA 条左边界 */
#define PBAA_Y      (10)
#define PBAA_W      (30)
#define PBAA_H      (76)

/**
 * @brief 扫描一根进度条的像素：颜色种数 / 中间色像素数 / 实际轮廓范围
 * @param x0,y0,w,h 扫描区域（比进度条大一圈也没关系，背景不去管）
 * @param out_d  颜色种数
 * @param out_mid 中间色像素数（既非白、非槽色、也非填充色 → 只可能来自混色）
 * @param bb     输出轮廓（x0,y0,x1,y1，相对扫描区域；全为背景时输出 (0,0,-1,-1)）
 */
static void pb_scan(int x0, int y0, int w, int h, int *out_d, int *out_mid,
                    int *bb)
{
    uint16_t seen[128];
    int n = 0;
    int i, j;
    int mid = 0;
    int x1 = -1, y1 = -1, sx = w, sy = h;

    mui_screen_flush();     /* 缓冲后端：先推屏再读 */

    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            uint16_t c = sim_get_fb()[(y0 + j) * SIM_W + (x0 + i)];
            int k, found = 0;

            for (k = 0; k < n; k++) {
                if (seen[k] == c) {
                    found = 1;
                    break;
                }
            }
            if (!found && n < 128) {
                seen[n++] = c;
            }
            if (c != TEST_BG) {
                if (i < sx) { sx = i; }
                if (j < sy) { sy = j; }
                if (i > x1) { x1 = i; }
                if (j > y1) { y1 = j; }
            }
            if (c != TEST_BG && c != MUI_LIGHTGREY && c != MUI_BLUE) {
                mid++;      /* 只可能来自"与背景混合后的边界像素" */
            }
        }
    }
    *out_d = n;
    *out_mid = mid;
    bb[0] = sx;
    bb[1] = sy;
    bb[2] = x1;
    bb[3] = y1;
}

static void test_progressbar_aa(void)
{
    static const mui_progressbar_style_t st_hard = {
        .bg = MUI_LIGHTGREY, .fg = MUI_BLUE, .border = MUI_LIGHTGREY,
        .radius = 0, .dir = MUI_PROGRESSBAR_VERTICAL, .aa = 0,
        .screen_bg = TEST_BG
    };
    static const mui_progressbar_style_t st_aa = {
        .bg = MUI_LIGHTGREY, .fg = MUI_BLUE, .border = MUI_LIGHTGREY,
        .radius = 0, .dir = MUI_PROGRESSBAR_VERTICAL, .aa = 1,
        .screen_bg = TEST_BG
    };
    static mui_progressbar_t pb_hard;
    static mui_progressbar_t pb_aa;
    int d_hard, d_aa, m_hard, m_aa;
    int bb_hard[4], bb_aa[4];
    int row, asym = 0;

    mui_screen_clear(TEST_BG);
    mui_progressbar_init(&pb_hard, PBAA_X_HARD, PBAA_Y, PBAA_W, PBAA_H, &st_hard);
    mui_progressbar_init(&pb_aa, PBAA_X_AA, PBAA_Y, PBAA_W, PBAA_H, &st_aa);
    mui_progressbar_set_value(&pb_hard, 50);
    mui_progressbar_set_value(&pb_aa, 50);

    pb_scan(PBAA_X_HARD, PBAA_Y, PBAA_W, PBAA_H, &d_hard, &m_hard, bb_hard);
    pb_scan(PBAA_X_AA, PBAA_Y, PBAA_W, PBAA_H, &d_aa, &m_aa, bb_aa);

    /* 1) 硬边版圆角是整数跨度，边界只能落在"槽色/填充色/背景色"三者上 */
    CHECK(m_hard == 0, "硬边进度条不应出现混色的中间像素");
    /* 2) AA 版必须在边界产生中间色（否则等于没抗锯齿） */
    CHECK(m_aa > 20, "AA 进度条未在圆角边界产生混色像素");
    CHECK(d_aa > d_hard, "AA 版的颜色种数应多于硬边版");
    /* 3) 抗锯齿只改边缘观感、不改几何：轮廓范围必须与硬边版逐项一致 */
    CHECK(bb_hard[0] == bb_aa[0] && bb_hard[1] == bb_aa[1]
          && bb_hard[2] == bb_aa[2] && bb_hard[3] == bb_aa[3],
          "AA 版轮廓范围与硬边版不一致（几何被改动了）");
    /* 4) 胶囊左右对称：同一行最左与最右的边界像素颜色必须相同 */
    mui_screen_flush();
    for (row = 0; row < bb_aa[3] - bb_aa[1] + 1; row++) {
        int y = PBAA_Y + bb_aa[1] + row;
        int lx = -1, i;
        uint16_t lc = 0, rc = 0;

        for (i = 0; i < PBAA_W; i++) {
            uint16_t c = sim_get_fb()[y * SIM_W + PBAA_X_AA + i];

            if (c != TEST_BG) {
                if (lx < 0) {
                    lx = i;
                    lc = c;
                }
                rc = c;
            }
        }
        if (lx >= 0 && lc != rc) {
            asym++;
        }
    }
    CHECK(asym == 0, "AA 版左右边界不对称（开方/内缩索引有误）");
}

static const char *demo_png_name(void)
{
#if MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL
    return "test_output_full.png";
#elif MUI_CFG_OUTPUT_MODE == MUI_OUTPUT_FULL_DOUBLE
    return "test_output_double.png";
#else
    return "test_output.png";
#endif
}

/* -------- 新控件目视总览：仪表盘 / 列表 / 下拉（展开） / 弹窗 -------- */
static void render_controls(void)
{
    static mui_gauge_t g;
    static mui_list_t lst;
    static mui_dropdown_t dd;
    static mui_popup_t pp;
    static const char *const items[4] = { "11", "22", "33", "12" };
    static const mui_gauge_style_t gst = {
        MUI_RGB565(0x33, 0x3D, 0x4E),   /* track */
        MUI_RGB565(0x04, 0xAA, 0xF4),   /* fill */
        MUI_BLACK,                      /* text */
        MUI_WHITE,                      /* bg */
        18,                             /* thick */
        135, 405,                       /* 270° 扫角 */
        1, 1, 1                         /* aa / show_value / percent */
    };
    static const mui_button_style_t bst = {
        MUI_RGB565(0x04, 0xAA, 0xF4), MUI_RGB565(0x0A, 0x2A, 0x5A), MUI_WHITE,
        MUI_WHITE, MUI_BUTTON_SHAPE_ROUND, 6, 1, MUI_WHITE
    };
    int i;

    mui_reset_clip();
    mui_screen_clear(MUI_WHITE);

    /* 仪表盘：62%，中心显示数值 */
    mui_gauge_init(&g, 90, 110, 70, 0, 100, 62, &gst);
    mui_gauge_set_font(&g, &lv_mono_font);
    mui_gauge_draw(&g);

    /* 列表：6 行、选中第 2 行 */
    mui_list_init(&lst, 200, 30, 120, 110, 24, 4);
    mui_list_set_rows(&lst, 6);
    mui_list_set_sel(&lst, 2);
    mui_list_draw_frame(&lst, MUI_WHITE, MUI_RGB565(0xCC, 0xCC, 0xCC));
    mui_list_begin(&lst);
    for (i = 0; i < 6; i++) {
        mui_rect_t r = mui_list_row_rect(&lst, i);
        uint16_t b = (i == 2) ? MUI_RGB565(0x04, 0xAA, 0xF4) : MUI_WHITE;
        mui_rect_fill((int16_t)(r.x + 2), r.y, (int16_t)(r.x2 - r.x - 4),
                      (int16_t)(r.y2 - r.y), b);
        mui_text_draw_rect((int16_t)(r.x + 6), r.y, (int16_t)(r.x2 - r.x - 12),
                           (int16_t)(r.y2 - r.y), "123", &lv_mono_font,
                           (i == 2) ? MUI_WHITE : MUI_BLACK, b, 1, MUI_ALIGN_LEFT);
    }
    mui_list_end(&lst);

    /* 下拉（展开态） */
    mui_dropdown_init(&dd, 340, 30, 110, 26, items, 4, 24, &lv_mono_font, &bst);
    mui_dropdown_set_colors(&dd, MUI_WHITE, MUI_RGB565(0x04, 0xAA, 0xF4),
                            MUI_BLACK, MUI_WHITE, MUI_BLACK);
    mui_dropdown_open(&dd);
    mui_dropdown_draw(&dd);

    /* 弹窗：画在最上层 */
    mui_popup_init(&pp, 60, 240, 360, 150, "123", "321", &lv_mono_font);
    mui_popup_set_colors(&pp, MUI_WHITE, MUI_BLACK, MUI_BLACK, MUI_BLACK);
    mui_popup_add_button(&pp, "11", &bst);
    mui_popup_add_button(&pp, "22", &bst);
    mui_popup_open(&pp);
    mui_popup_draw(&pp);

    mui_screen_flush();
    sim_export_png("test_output_controls.png");
}

static void render_demo(void)
{
    mui_screen_clear(MUI_WHITE);

    /* 顶部标题栏：圆角矩形 */
    mui_round_rect_fill(10, 10, 300, 40, 12, MUI_NAVY);
    mui_round_rect_draw(10, 10, 300, 40, 12, MUI_CYAN);

    /* 标题栏上叠两个空心圆角矩形作对照：左硬边 / 右 AA（白描边压在深蓝底上，
     * 缓冲后端会自动取回真实底色，直绘后端用传入的 bg） */
    mui_round_rect_draw(14, 14, 44, 28, 8, MUI_WHITE);
    mui_round_rect_draw_aa(70, 14, 44, 28, 8, MUI_WHITE, MUI_NAVY);

    /* 色块矩阵 */
    for (int i = 0; i < 6; i++) {
        mui_round_rect_fill((int16_t)(14 + i * 50), 60, 44, 30, 8,
                            (uint16_t)(0xF800 >> (i % 3))
                            | (uint16_t)(i * 0x0421));
    }

    /* 左侧：同心圆 */
    mui_circle_draw(70, 180, 45, MUI_GREEN);
    mui_circle_draw(70, 180, 30, MUI_CYAN);
    mui_circle_fill(70, 180, 15, MUI_RED);

    /* 中间：椭圆 + 三角形 */
    mui_ellipse_fill(170, 180, 40, 25, MUI_BLUE);
    mui_triangle_fill(230, 205, 270, 205, 250, 150, MUI_YELLOW);

    /* 右侧：渐变竖条 + 对角线 */
    for (int i = 0; i < 8; i++) {
        mui_rect_fill(288, (int16_t)(140 + i * 12), 24, 10,
                      (uint16_t)(0x001F + i * 0x0400));
    }
    mui_line_draw(230, 60, 310, 130, MUI_WHITE);

    /* 底部：进度条（大圆角，目检填充与边框贴合） */
    {
        static const mui_progressbar_style_t st = {
            MUI_RGB565(0xCC, 0xCC, 0xCC), MUI_BLUE, MUI_RED, 6,
            MUI_PROGRESSBAR_HORIZONTAL,
        };
        static mui_progressbar_t pb;

        mui_progressbar_init(&pb, 10, 112, 108, 12, &st);
        mui_progressbar_set_value(&pb, 70);
    }

    /* 中部空档：抗锯齿对照（同一几何、同一进度，只差 style.aa）
     *   上 = 硬边（圆角是整数台阶）   下 = AA（圆角边界按覆盖率混色）
     * r 显式取 h/2 → 两端半圆（胶囊），圆角占比大，肉眼与逐像素都好判断。 */
    {
        static const mui_progressbar_style_t st_hard = {
            MUI_LIGHTGREY, MUI_BLUE, MUI_LIGHTGREY, 5,
            MUI_PROGRESSBAR_HORIZONTAL, 0, MUI_WHITE,
        };
        static const mui_progressbar_style_t st_aa = {
            MUI_LIGHTGREY, MUI_BLUE, MUI_LIGHTGREY, 5,
            MUI_PROGRESSBAR_HORIZONTAL, 1, MUI_WHITE,
        };
        static mui_progressbar_t pb_hard;
        static mui_progressbar_t pb_aa;

        mui_progressbar_init(&pb_hard, 6, 90, 116, 10, &st_hard);
        mui_progressbar_init(&pb_aa, 6, 102, 116, 10, &st_aa);
        mui_progressbar_set_value(&pb_hard, 60);
        mui_progressbar_set_value(&pb_aa, 100);   /* 先满，再降档 → 走增量擦除路径
                                                   * （擦除若按纯槽色铺满，槽的圆角
                                                   *   轮廓会变成硬台阶，见
                                                   *   test_progressbar_track_aa） */
        mui_progressbar_set_value(&pb_aa, 60);
    }

    /* ---- 控件总览：贴图按钮 / 滑块 / 开关 / 复选 / 单选 / 圆环 ---- */
    mui_screen_clear(MUI_WHITE);
    {
        /* 贴图按钮（横向渐变底板） */
        static uint16_t g_face[140 * 34];
        static mui_button_t ib;
        mui_image_t face;
        int i;

        face.w = 140; face.h = 34; face.data = g_face;
        for (i = 0; i < 140 * 34; i++) {
            uint8_t t = (uint8_t)(((i % 140) * 255) / 139);
            g_face[i] = mui_color_mix(MUI_RGB565(0x0A, 0x2A, 0x5A),
                                      MUI_RGB565(0x2E, 0xCC, 0xFF), t);
        }
        mui_button_init(&ib, 20, 20, 140, 34, NULL, NULL, NULL);
        mui_button_set_bg_image(&ib, &face);
        mui_button_draw(&ib);
    }
    {
        /* 滑块 */
        static const mui_slider_style_t st = {
            MUI_RGB565(0x33, 0x3D, 0x4E), MUI_RGB565(0x04, 0xAA, 0xF4),
            MUI_WHITE, MUI_RGB565(0x04, 0xAA, 0xF4), MUI_WHITE,
            10, 15, MUI_SLIDER_HORIZONTAL, 1,
        };
        static mui_slider_t sl;
        mui_slider_init(&sl, 30, 90, 200, 30, 0, 100, 65, &st);
        mui_slider_draw(&sl);
    }
    {
        /* 开关 ON / OFF */
        static mui_switch_t a, b;
        mui_switch_init(&a, 30, 150, 80, 26, 1, NULL);
        mui_switch_set_text(&a, "WiFi");
        mui_switch_init(&b, 30, 190, 80, 26, 0, NULL);
        mui_switch_set_text(&b, "BT");
        mui_switch_draw(&a);
        mui_switch_draw(&b);
    }
    {
        /* 复选 */
        static mui_checkbox_t a, b;
        mui_checkbox_init(&a, 30, 240, 150, 26, 1, NULL);
        mui_checkbox_set_text(&a, "Check ON");
        mui_checkbox_init(&b, 30, 274, 150, 26, 0, NULL);
        mui_checkbox_set_text(&b, "Check OFF");
        mui_checkbox_draw(&a);
        mui_checkbox_draw(&b);
    }
    {
        /* 单选（互斥组） */
        static mui_radio_t a, b, *g[3];
        g[0] = &a; g[1] = &b; g[2] = NULL;
        mui_radio_init(&a, 30, 320, 150, 26, 1, NULL);
        mui_radio_set_text(&a, "Option A");
        mui_radio_init(&b, 30, 354, 150, 26, 0, NULL);
        mui_radio_set_text(&b, "Option B");
        mui_radio_set_group(&a, g);
        mui_radio_set_group(&b, g);
        mui_radio_draw(&a);
        mui_radio_draw(&b);
    }
    {
        /* 圆环进度（半程 / 满程） */
        const uint16_t track  = MUI_RGB565(0x33, 0x3D, 0x4E);
        const uint16_t accent = MUI_RGB565(0x04, 0xAA, 0xF4);
        const uint16_t acc2   = MUI_RGB565(0x2E, 0xCC, 0x71);

        mui_ring_fill_aa(360, 130, 30, 46, 0, 360, track, MUI_WHITE);
        mui_ring_fill_aa(360, 130, 30, 46, 135, 300, accent, MUI_WHITE);
        mui_ring_fill_aa(360, 300, 30, 46, 0, 360, track, MUI_WHITE);
        mui_ring_fill_aa(360, 300, 30, 46, 135, 405, acc2, MUI_WHITE);
    }

    mui_screen_flush();   /* 缓冲后端：推屏后才能导出完整画面 */
    sim_export_png(demo_png_name());

    render_controls();    /* 另行导出一张新控件总览图 */
}

/* -------- 测试：圆弧 / 圆环扇区 -------- */
static void test_ring(void)
{
    const int16_t cx = 100, cy = 100, rin = 30, rout = 50;
    int16_t mid = (int16_t)((rin + rout) / 2);   /* 环带中点半径 */

    /* 1) 越界/非法参数全程安全 */
    sim_clear_violations();
    mui_ring_fill(cx, cy, rout, rout, 0, 360, MUI_RED);   /* rin>=rout */
    mui_ring_fill(cx, cy, -10, 40, 0, 360, MUI_RED);     /* rin<0 视作 0 */
    mui_arc_draw(cx, cy, 40, 0, 360, 0, MUI_RED);        /* thickness<=0 */
    mui_ring_fill_aa(cx, cy, 30, 50, 0, 360, MUI_RED, MUI_WHITE);
    CHECK(sim_violations() == 0, "圆环图元非法参数未触发越界违规");

    /* 2) 整环几何：环带内填充、内孔与外圈外为背景 */
    mui_screen_clear(TEST_BG);
    mui_ring_fill(cx, cy, rin, rout, 0, 360, MUI_BLUE);
    CHECK(px(cx, cy - mid) == MUI_BLUE, "整环：上边环带中点");
    CHECK(px(cx, cy + mid) == MUI_BLUE, "整环：下边环带中点");
    CHECK(px(cx - mid, cy) == MUI_BLUE, "整环：左边环带中点");
    CHECK(px(cx + mid, cy) == MUI_BLUE, "整环：右边环带中点");
    CHECK(px(cx, cy - (rin - 5)) == TEST_BG, "整环：内孔未填充");
    CHECK(px(cx, cy - (rout + 5)) == TEST_BG, "整环：外圈外未填充");

    /* 3) 四分之一扇形（rin=0, 0~90°）：仅覆盖右下象限（x>0,y>0） */
    mui_screen_clear(TEST_BG);
    mui_ring_fill(cx, cy, 0, rout, 0, 90, MUI_GREEN);
    CHECK(px(cx + mid, cy) == MUI_GREEN, "扇形：0° 方向(+x)在扇区内");
    CHECK(px(cx, cy + mid) == MUI_GREEN, "扇形：90° 方向(+y)在扇区内");
    CHECK(px(cx + 28, cy + 28) == MUI_GREEN, "扇形：45°（半径约40，落在环带内）");
    CHECK(px(cx - mid, cy) == TEST_BG, "扇形：180° 在扇区外");
    CHECK(px(cx, cy - mid) == TEST_BG, "扇形：270° 在扇区外");

    /* 4) 跨 0° 的弧（315°~405°）：覆盖右半（x>0），左半圆在扇区外 */
    mui_screen_clear(TEST_BG);
    mui_ring_fill(cx, cy, rin, rout, 315, 405, MUI_RED);
    CHECK(px(cx + mid, cy) == MUI_RED, "跨0弧：0°(+x)在扇区内");
    CHECK(px(cx, cy + mid) == TEST_BG, "跨0弧：90°(+y)在扇区外");
    CHECK(px(cx, cy - mid) == TEST_BG, "跨0弧：270°(-y)在扇区外");
    CHECK(px(cx - mid, cy) == TEST_BG, "跨0弧：180° 在扇区外");

    /* 5) 整环 AA：开 AA 时内外边缘应产生混色像素；关 AA 时退化为纯色硬边 */
    {
        mui_screen_clear(TEST_BG);
        mui_ring_fill_aa(cx, cy, rin, rout, 0, 360, MUI_BLACK, MUI_WHITE);
        mui_screen_flush();
#if MUI_CFG_AA
        {
            int blend = 0, i;
            for (i = 0; i < (int)(SIM_W * SIM_H); i++) {
                uint16_t c = sim_get_fb()[i];
                if (c != MUI_WHITE && c != MUI_BLACK) {
                    blend++;
                }
            }
            CHECK(blend > 20, "AA 整环未在内外边缘产生混色像素");
        }
#endif
        CHECK(px(cx, cy - (rin - 5)) == MUI_WHITE, "AA 整环：内孔仍为背景");
    }

    /* 6) 大跨度弧（>180°）方向正确性：0~270° 覆盖除右上象限外的 3/4 圈 */
    mui_screen_clear(TEST_BG);
    mui_ring_fill(cx, cy, rin, rout, 0, 270, MUI_BLUE);
    CHECK(px(cx + mid, cy) == MUI_BLUE, "大跨度弧：0°(+x)在内");
    CHECK(px(cx, cy + mid) == MUI_BLUE, "大跨度弧：90°(+y)在内");
    CHECK(px(cx - mid, cy) == MUI_BLUE, "大跨度弧：180°(-x)在内");
    CHECK(px(cx, cy - mid) == MUI_BLUE, "大跨度弧：270°(-y)在内");
    CHECK(px(cx + 28, cy - 28) == TEST_BG, "大跨度弧：315° 在外（缺口）");

    /* 7) 进度缩小不留残影：先画满 270°，再重画轨道 + 缩到 180° */
    {
        const uint16_t card  = MUI_RGB565(0x22, 0x2B, 0x3A);
        const uint16_t track = MUI_RGB565(0x33, 0x3D, 0x4E);
        const uint16_t acc   = MUI_RGB565(0x04, 0xAA, 0xF4);
        const int16_t gx = 240, gy = 60;

        mui_rect_fill(180, 0, 140, 120, card);
        mui_ring_fill_aa(gx, gy, rin, rout, 0, 360, track, card);
        mui_ring_fill_aa(gx, gy, rin, rout, 0, 270, acc, card);
        CHECK(px(gx - 28, gy - 28) == acc, "残影：225° 处填满");

        mui_ring_fill_aa(gx, gy, rin, rout, 0, 360, track, card);   /* 重画轨道 */
        mui_ring_fill_aa(gx, gy, rin, rout, 0, 180, acc, card);     /* 缩到 180° */
        CHECK(px(gx - 28, gy - 28) == track, "缩弧后 225° 处残留了填充色");
        CHECK(px(gx, gy + mid) == acc, "缩弧后 90° 处仍为填充色");
    }

    /* 8) 大跨度弧内部不得有杂线：135~405 时，225°/315° 处应为纯填充色 */
    mui_screen_clear(TEST_BG);
    mui_ring_fill_aa(cx, cy, rin, rout, 135, 405, MUI_BLACK, MUI_WHITE);
    CHECK(px(cx - 28, cy - 28) == MUI_BLACK, "大弧内部 225° 处有杂线（非纯色）");
    CHECK(px(cx + 28, cy - 28) == MUI_BLACK, "大弧内部 315° 处有杂线（非纯色）");
}

/* -------- 测试：按钮底板圆角抗锯齿 -------- */
static void test_button_aa(void)
{
    static const mui_button_style_t st = {
        .bg = MUI_BLUE, .bg_press = MUI_NAVY, .fg = MUI_WHITE,
        .border = MUI_BLUE, .shape = MUI_BUTTON_SHAPE_PILL, .radius = 0,
        .aa = 1, .screen_bg = MUI_WHITE,
    };
    static mui_button_t btn;
    int blend = 0, i;

    mui_screen_clear(MUI_WHITE);
    mui_button_init(&btn, 20, 20, 140, 48, &st, NULL, NULL);
    mui_button_draw(&btn);
    mui_screen_flush();
    for (i = 0; i < (int)(SIM_W * SIM_H); i++) {
        uint16_t c = sim_get_fb()[i];
        if (c != MUI_WHITE && c != MUI_BLUE) {
            blend++;
        }
    }
    CHECK(blend > 10, "按钮 AA 未在圆角边缘产生混色像素");
}

/* -------- 测试：按钮底板贴图（全彩） -------- */
static void test_button_image(void)
{
    static const uint16_t face[16] = {
        MUI_GREEN, MUI_GREEN, MUI_GREEN, MUI_GREEN,
        MUI_GREEN, MUI_GREEN, MUI_GREEN, MUI_GREEN,
        MUI_GREEN, MUI_GREEN, MUI_GREEN, MUI_GREEN,
        MUI_GREEN, MUI_GREEN, MUI_GREEN, MUI_GREEN,
    };
    static const mui_image_t img = { 4, 4, face };
    static mui_button_t btn;

    mui_screen_clear(MUI_WHITE);
    mui_button_init(&btn, 10, 10, 4, 4, NULL, NULL, NULL);
    mui_button_set_bg_image(&btn, &img);
    mui_button_draw(&btn);
    mui_screen_flush();
    CHECK(px(10, 10) == MUI_GREEN, "图片按钮：贴图左上角");
    CHECK(px(13, 13) == MUI_GREEN, "图片按钮：贴图右下角");
    CHECK(px(14, 14) == MUI_WHITE, "图片按钮：贴图之外不绘制");
}

/* -------- 测试：缩放贴图 / 九宫格 -------- */
static void test_image_scale_nine(void)
{
    static const uint16_t src[4] = { MUI_RED, MUI_GREEN, MUI_BLUE, MUI_YELLOW };
    static const mui_image_t img = { 2, 2, src };

    mui_screen_clear(MUI_WHITE);
    mui_image_draw_scaled(10, 10, 40, 20, img.w, img.h, img.data);
    mui_screen_flush();
    CHECK(px(12, 12) == MUI_RED, "缩放贴图：左上");
    CHECK(px(45, 12) == MUI_GREEN, "缩放贴图：右上");
    CHECK(px(12, 28) == MUI_BLUE, "缩放贴图：左下");

    mui_screen_clear(MUI_WHITE);
    mui_image_draw_nine(10, 10, 40, 40, img.w, img.h, img.data, 1, 1, 1, 1);
    mui_screen_flush();
    CHECK(px(10, 10) == MUI_RED, "九宫格：左上角");
    CHECK(px(49, 10) == MUI_GREEN, "九宫格：右上角");
    CHECK(px(10, 49) == MUI_BLUE, "九宫格：左下角");
    CHECK(px(49, 49) == MUI_YELLOW, "九宫格：右下角");
}

/* -------- 测试：滑块 -------- */
static void test_slider(void)
{
    static mui_slider_t sl;

    mui_slider_init(&sl, 10, 10, 100, 20, 0, 100, 0, NULL);
    mui_touch_update(60, 20, 1);                 /* 按下到中点附近 */
    (void)mui_slider_touch(&sl, MUI_TOUCH_DOWN);
    CHECK(mui_slider_get_value(&sl) == 50, "滑块：点击中点应为 50");

    mui_slider_set_value(&sl, 30);
    CHECK(mui_slider_get_value(&sl) == 30, "滑块：set_value");
    mui_slider_set_value(&sl, 999);
    CHECK(mui_slider_get_value(&sl) == 100, "滑块：超范围自动夹取");

    mui_slider_draw(&sl);
    mui_touch_update(0, 0, 0);
    while (mui_touch_poll() != MUI_TOUCH_NONE) { }
}

/* -------- 测试：滑块拖动不留拖影 -------- */
static void test_slider_trail(void)
{
    static const mui_slider_style_t st = {
        .track = MUI_RGB565(0x33, 0x3D, 0x4E), .fill = MUI_BLUE,
        .knob = MUI_RED, .knob_border = MUI_RED, .screen_bg = MUI_WHITE,
        .thickness = 10, .knob_r = 14, .dir = MUI_SLIDER_HORIZONTAL, .aa = 1,
    };
    static mui_slider_t sl;

    mui_screen_clear(MUI_WHITE);
    mui_slider_init(&sl, 20, 100, 200, 30, 0, 100, 90, &st);
    mui_slider_draw(&sl);
    CHECK(px(199, 105) == MUI_RED, "滑块：90% 处有滑块");

    mui_slider_set_value(&sl, 10);
    mui_slider_draw(&sl);
    CHECK(px(199, 105) == MUI_WHITE, "滑块：改值后旧滑块被擦除（无拖影）");
    CHECK(px(39, 105) == MUI_RED, "滑块：10% 处为新滑块");
}

/* -------- 测试：开关 / 复选 / 单选 -------- */
static void test_toggle(void)
{
    static mui_switch_t sw;
    static mui_checkbox_t cb;
    static mui_radio_t r1, r2, *grp[3];

    mui_switch_init(&sw, 10, 10, 60, 24, 0, NULL);
    mui_touch_update(20, 20, 1);
    CHECK(mui_switch_touch(&sw, MUI_TOUCH_CLICK) == 1 && mui_switch_get_on(&sw) == 1,
          "开关：点击切换");
    mui_switch_draw(&sw);
    mui_touch_update(0, 0, 0);
    while (mui_touch_poll() != MUI_TOUCH_NONE) { }

    mui_checkbox_init(&cb, 10, 50, 100, 24, 0, NULL);
    mui_touch_update(20, 60, 1);
    CHECK(mui_checkbox_touch(&cb, MUI_TOUCH_CLICK) == 1 && mui_checkbox_get_checked(&cb) == 1,
          "复选：点击勾选");
    mui_checkbox_draw(&cb);
    mui_touch_update(0, 0, 0);
    while (mui_touch_poll() != MUI_TOUCH_NONE) { }

    grp[0] = &r1; grp[1] = &r2; grp[2] = NULL;
    mui_radio_init(&r1, 10, 90, 100, 24, 1, NULL);
    mui_radio_init(&r2, 10, 120, 100, 24, 0, NULL);
    mui_radio_set_group(&r1, grp);
    mui_radio_set_group(&r2, grp);
    mui_touch_update(20, 130, 1);
    CHECK(mui_radio_touch(&r2, MUI_TOUCH_CLICK) == 1
          && mui_radio_get_selected(&r2) == 1 && mui_radio_get_selected(&r1) == 0,
          "单选：互斥选中");
    mui_radio_draw(&r1);
    mui_radio_draw(&r2);
    mui_touch_update(0, 0, 0);
    while (mui_touch_poll() != MUI_TOUCH_NONE) { }
}

/* -------- 测试：绘制裁剪区 -------- */
static void test_clip_region(void)
{
    /* 0) 初始 / 复位：裁剪区为全屏，整屏填充照旧 */
    mui_reset_clip();
    mui_screen_clear(TEST_BG);

    /* 1) 裁剪区内填充生效、区外不动 */
    mui_set_clip(20, 20, 10, 10);
    mui_rect_fill(0, 0, 100, 100, MUI_RED);        /* 只应写入 [20,30)x[20,30) */
    mui_reset_clip();
    CHECK(px(20, 20) == MUI_RED, "裁剪：区内左上角填充");
    CHECK(px(29, 29) == MUI_RED, "裁剪：区内右下角填充");
    CHECK(px(19, 25) == TEST_BG, "裁剪：区外左侧未写");
    CHECK(px(30, 25) == TEST_BG, "裁剪：区外右侧未写");
    CHECK(px(25, 19) == TEST_BG, "裁剪：区外上方未写");
    CHECK(px(25, 30) == TEST_BG, "裁剪：区外下方未写");

    /* 2) 完全在裁剪区外的绘制：无写入、无底层越界 */
    sim_clear_violations();
    mui_set_clip(200, 200, 20, 20);
    mui_rect_fill(0, 0, 10, 10, MUI_GREEN);         /* 完全在区外，应丢弃 */
    mui_pixel_draw(205, 205, MUI_GREEN);            /* 区内，应写入 */
    mui_reset_clip();
    CHECK(sim_violations() == 0, "裁剪：区外绘制触发底层越界");
    CHECK(px(5, 5) == TEST_BG, "裁剪：完全在区外的矩形未写");
    CHECK(px(205, 205) == MUI_GREEN, "裁剪：区内像素已写");

    /* 3) 跨裁剪区边界：只写交集 */
    mui_screen_clear(TEST_BG);
    mui_set_clip(30, 30, 10, 10);
    mui_rect_fill(25, 25, 20, 20, MUI_BLUE);        /* 交集 = [30,40)x[30,40) */
    mui_reset_clip();
    CHECK(px(30, 30) == MUI_BLUE, "裁剪：跨边界交集左上");
    CHECK(px(39, 39) == MUI_BLUE, "裁剪：跨边界交集右下");
    CHECK(px(29, 29) == TEST_BG, "裁剪：跨边界只写交集(左上外侧)");
    CHECK(px(40, 40) == TEST_BG, "裁剪：跨边界只写交集(右下外侧)");

    /* 4) 空裁剪区：全部丢弃 */
    mui_screen_clear(TEST_BG);
    sim_clear_violations();
    mui_set_clip(10, 10, 0, 0);
    mui_rect_fill(0, 0, 50, 50, MUI_RED);
    mui_pixel_draw(12, 12, MUI_RED);
    mui_reset_clip();
    CHECK(px(12, 12) == TEST_BG, "裁剪：空裁剪区丢弃全部绘制");
    CHECK(sim_violations() == 0, "裁剪：空裁剪区触发底层越界");

    /* 5) 裁剪区自动与屏幕求交：越界参数安全 */
    mui_screen_clear(TEST_BG);
    sim_clear_violations();
    mui_set_clip(-50, -50, 100, 100);               /* 只应保留 [0,50)x[0,50) */
    mui_rect_fill(-100, -100, 300, 300, MUI_YELLOW);
    mui_reset_clip();
    CHECK(sim_violations() == 0, "裁剪：越界裁剪区触发底层越界");
    CHECK(px(49, 49) == MUI_YELLOW, "裁剪：越界裁剪区与屏幕求交后仍有效");
    CHECK(px(50, 50) == TEST_BG, "裁剪：越界裁剪区外未写");

    /* 6) 嵌套 save / restore */
    mui_screen_clear(TEST_BG);
    mui_reset_clip();
    {
        mui_rect_t outer = mui_clip_save();          /* 全屏 */
        mui_set_clip(10, 10, 40, 40);
        {
            mui_rect_t inner = mui_clip_save();      /* [10,50)x[10,50) */
            mui_set_clip(20, 20, 10, 10);
            mui_rect_fill(0, 0, 100, 100, MUI_YELLOW);  /* 只 [20,30)x[20,30) */
            mui_clip_restore(inner);
        }
        mui_rect_fill(0, 0, 100, 100, MUI_GREEN);    /* 恢复外层裁剪后重绘 */
        mui_clip_restore(outer);
    }
    CHECK(px(45, 45) == MUI_GREEN, "裁剪：恢复外层后在范围内填充");
    CHECK(px(35, 35) == MUI_GREEN, "裁剪：内层区域被外层重绘覆盖");
    CHECK(px(9, 9) == TEST_BG, "裁剪：外层边界外未写");
    CHECK(px(50, 50) == TEST_BG, "裁剪：外层边界外(右下)未写");

    /* 7) 各图元在裁剪区下都不越界，区外像素保持背景 */
    mui_screen_clear(TEST_BG);
    mui_set_clip(100, 100, 30, 30);
    sim_clear_violations();
    mui_circle_fill(90, 90, 60, MUI_RED);
    mui_circle_draw(90, 90, 60, MUI_RED);
    mui_circle_draw_aa(90, 90, 60, MUI_RED, TEST_BG);
    mui_circle_fill_aa(90, 90, 60, MUI_RED, TEST_BG);
    mui_ellipse_fill(90, 90, 70, 50, MUI_RED);
    mui_ellipse_draw(90, 90, 70, 50, MUI_RED);
    mui_triangle_fill(60, 60, 220, 100, 100, 220, MUI_RED);
    mui_triangle_draw(60, 60, 220, 100, 100, 220, MUI_RED);
    mui_line_draw(60, 60, 220, 220, MUI_RED);
    mui_line_draw_aa(60, 60, 220, 220, MUI_RED, TEST_BG);
    mui_round_rect_fill_aa(60, 60, 200, 200, 20, MUI_RED, TEST_BG);
    mui_round_rect_draw_aa(60, 60, 200, 200, 20, MUI_RED, TEST_BG);
    mui_ring_fill(150, 150, 20, 40, 0, 270, MUI_RED);
    mui_ring_fill_aa(150, 150, 20, 40, 0, 270, MUI_RED, TEST_BG);
    mui_arc_draw(150, 150, 60, 0, 270, 10, MUI_RED);
    mui_arc_draw_aa(150, 150, 60, 0, 270, 10, MUI_RED, TEST_BG);
    mui_reset_clip();
    CHECK(sim_violations() == 0, "裁剪：各图元在裁剪区下触发底层越界");
    CHECK(px(99, 115) == TEST_BG, "裁剪：图元裁剪区外(左)未写");
    CHECK(px(130, 115) == TEST_BG, "裁剪：图元裁剪区外(右)未写");
    CHECK(px(115, 99) == TEST_BG, "裁剪：图元裁剪区外(上)未写");
    CHECK(px(115, 130) == TEST_BG, "裁剪：图元裁剪区外(下)未写");

    /* 8) 裁剪区影响整屏矩形填充（清屏只清裁剪区内） */
    mui_screen_clear(TEST_BG);
    mui_set_clip(60, 60, 10, 10);
    mui_rect_fill(0, 0, 480, 480, MUI_NAVY);
    mui_reset_clip();
    CHECK(px(65, 65) == MUI_NAVY, "裁剪：整屏填充只落在裁剪区内");
    CHECK(px(59, 65) == TEST_BG && px(70, 65) == TEST_BG,
          "裁剪：整屏填充未越出裁剪区");

    mui_reset_clip();
}

/* -------- 测试：裁剪下的 alpha 蒙版（行宽不能按裁剪后宽度反推） -------- */
static void test_clip_mask(void)
{
    /* 20 宽的蒙版：源列 0..9 透明(0)，10..19 不透明(255)；两行同规律 */
    static const uint8_t mask[2][20] = {
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255 },
    };
    int c;

    mui_reset_clip();
    mui_screen_clear(TEST_BG);

    /* 裁剪到屏幕列 6..15：源列 6..15 可见，其中 >=10 不透明 → 屏幕 10..15 变黑 */
    mui_set_clip(6, 0, 10, 2);
    mui_image_draw_mask(0, 0, 20, 2, &mask[0][0], MUI_BLACK, TEST_BG);
    mui_reset_clip();

    for (c = 0; c < 20; c++) {
        uint16_t want = ((c >= 10 && c < 16)) ? MUI_BLACK : TEST_BG;
        CHECK(px(c, 0) == want, "裁剪蒙版：第 0 行像素错位（行宽被裁剪后宽度污染）");
        CHECK(px(c, 1) == want, "裁剪蒙版：第 1 行像素错位（行宽被裁剪后宽度污染）");
    }
    CHECK(px(16, 0) == TEST_BG, "裁剪蒙版：裁剪区外未写(右)");
    CHECK(px(5, 0) == TEST_BG, "裁剪蒙版：裁剪区外未写(左)");
}

/* -------- 测试：脏矩形跟踪 -------- */

#if MUI_CFG_DIRTY_N > 0

/** @brief 点是否落在脏区并集内 */
static int point_in_dirty_union(int16_t x, int16_t y)
{
    uint8_t n = mui_dirty_count(), i;
    mui_rect_t r;

    for (i = 0; i < n; i++) {
        if (!mui_dirty_get(i, &r)) {
            continue;
        }
        if (x >= r.x && x < r.x2 && y >= r.y && y < r.y2) {
            return 1;
        }
    }
    return 0;
}

/** @brief 核心不变式：所有被实际写入的像素都必须落在脏区并集内 */
static void check_dirty_invariant(void)
{
    int x, y, miss = 0;

    mui_reset_clip();
    mui_screen_clear(TEST_BG);
    mui_dirty_clear();          /* 清屏的脏已消费，从零开始记 */

    mui_rect_fill(5, 5, 20, 20, MUI_RED);
    mui_circle_fill(100, 100, 30, MUI_GREEN);
    mui_line_draw(200, 10, 300, 60, MUI_BLUE);
    mui_round_rect_fill(350, 350, 60, 60, 15, MUI_YELLOW);
    mui_circle_draw(60, 400, 25, MUI_CYAN);
    mui_triangle_fill(400, 100, 460, 200, 420, 20, MUI_MAGENTA);
    mui_ring_fill(130, 300, 10, 25, 0, 270, MUI_ORANGE);

    mui_screen_flush();
    for (y = 0; y < SIM_H; y++) {
        for (x = 0; x < SIM_W; x++) {
            if (sim_get_fb()[y * SIM_W + x] != TEST_BG
                && !point_in_dirty_union((int16_t)x, (int16_t)y)) {
                miss++;
            }
        }
    }
    CHECK(miss == 0, "脏矩形：存在未被脏区覆盖的已写像素（漏报）");
}

#endif /* MUI_CFG_DIRTY_N > 0 */

static void test_dirty(void)
{
#if MUI_CFG_DIRTY_N > 0
    mui_rect_t r;
    uint8_t i;

    /* 1) 同一区域重复标记 → 合并为 1 个 */
    mui_dirty_clear();
    for (i = 0; i < 5; i++) {
        mui_dirty_mark(10, 10, 20, 20);
    }
    CHECK(mui_dirty_count() == 1, "脏矩形：重复标记应合并为 1 个");

    /* 2) bounds / get 正确 */
    CHECK(mui_dirty_bounds(&r) == 1 && r.x == 10 && r.y == 10
          && r.x2 == 30 && r.y2 == 30, "脏矩形：bounds 内容错误");
    CHECK(mui_dirty_get(0, &r) == 1, "脏矩形：get(0) 应成功");
    CHECK(mui_dirty_get(1, &r) == 0, "脏矩形：越界 get 应失败");

    /* 3) 零散区域超过槽位上限 → 数量有界，且并集仍覆盖全部 */
    mui_dirty_clear();
    for (i = 0; i < 10; i++) {
        mui_dirty_mark((int16_t)(i * 30), 0, 10, 10);
    }
    CHECK(mui_dirty_count() <= MUI_CFG_DIRTY_N, "脏矩形：数量超过槽位上限");
    for (i = 0; i < 10; i++) {
        CHECK(point_in_dirty_union((int16_t)(i * 30 + 5), 5),
              "脏矩形：并集未覆盖已标记的零散区域");
    }

    /* 4) 标记受裁剪区限制 */
    mui_reset_clip();
    mui_dirty_clear();
    mui_set_clip(50, 50, 10, 10);
    mui_dirty_mark(0, 0, 100, 100);
    mui_reset_clip();
    CHECK(mui_dirty_count() == 1 && mui_dirty_bounds(&r) == 1
          && r.x == 50 && r.y == 50 && r.x2 == 60 && r.y2 == 60,
          "脏矩形：标记未被裁剪到裁剪区");

    /* 5) 空裁剪区 → 不产生脏区 */
    mui_dirty_clear();
    mui_set_clip(10, 10, 0, 0);
    mui_dirty_mark(0, 0, 100, 100);
    mui_reset_clip();
    CHECK(mui_dirty_count() == 0, "脏矩形：空裁剪区不应产生脏区");

    /* 6) 清空 */
    mui_dirty_clear();
    CHECK(mui_dirty_count() == 0 && mui_dirty_bounds(&r) == 0,
          "脏矩形：clear 后应为空");

    /* 7) 核心不变式 */
    check_dirty_invariant();
#else
    mui_rect_t r;

    mui_dirty_clear();
    mui_dirty_mark(0, 0, 10, 10);
    CHECK(mui_dirty_count() == 0, "脏矩形：功能关闭时 count 应恒为 0");
    CHECK(mui_dirty_bounds(&r) == 0, "脏矩形：功能关闭时 bounds 应恒为 0");
    CHECK(mui_dirty_get(0, &r) == 0, "脏矩形：功能关闭时 get 应恒返回 0");
#endif
}

/* -------- 测试：按钮按需重绘（状态缓存 + invalidate） -------- */
static void test_button_cache(void)
{
    static mui_button_t b;
    const uint16_t bg_up = mui_button_style_default.bg;
    const uint16_t bg_dn = mui_button_style_default.bg_press;

    mui_reset_clip();
    mui_screen_clear(MUI_WHITE);
    mui_button_init(&b, 20, 20, 100, 40, NULL, NULL, NULL);
    mui_button_draw(&b);                        /* 首次：必画 */
    CHECK(px(35, 35) == bg_up, "按钮缓存：首次绘制生效");

    /* 状态不变再次 draw：应跳过（预先涂红的区域保留） */
    mui_rect_fill(30, 30, 10, 10, MUI_RED);
    mui_button_draw(&b);
    CHECK(px(35, 35) == MUI_RED, "按钮缓存：状态不变时应跳过重绘");

    /* 状态改变：应重绘并覆盖红色 */
    mui_button_set_pressed(&b, 1);
    mui_button_draw(&b);
    CHECK(px(35, 35) == bg_dn, "按钮缓存：按下态变化后应重绘");

    /* 未变化时再次跳过 */
    mui_rect_fill(30, 30, 10, 10, MUI_RED);
    mui_button_draw(&b);
    CHECK(px(35, 35) == MUI_RED, "按钮缓存：未变化时再次跳过");

    /* invalidate 强制重绘 */
    mui_button_invalidate(&b);
    mui_button_draw(&b);
    CHECK(px(35, 35) == bg_dn, "按钮缓存：invalidate 后应强制重绘");

    /* 移位置应被检测到（旧位置留白、新位置出现按钮） */
    mui_button_set_pos(&b, 220, 200);
    mui_button_draw(&b);
    CHECK(px(235, 215) == bg_dn, "按钮缓存：移位置后应重绘到新位置");

    /* 禁用态切换也应触发重绘 */
    mui_button_set_enabled(&b, 0);
    mui_button_draw(&b);
    CHECK(px(235, 215) != bg_dn, "按钮缓存：禁用态应重绘（换灰底）");

    mui_reset_clip();
}

/* -------- 测试：动画（时间基 / 缓动 / 补间） -------- */
static void test_anim(void)
{
    static const uint8_t curves[] = {
        MUI_EASE_LINEAR, MUI_EASE_IN_QUAD, MUI_EASE_OUT_QUAD,
        MUI_EASE_IN_OUT_QUAD, MUI_EASE_IN_CUBIC, MUI_EASE_OUT_CUBIC,
        MUI_EASE_IN_OUT_CUBIC, MUI_EASE_OUT_BACK,
    };
    uint8_t i;

    /* 1) 所有曲线端点：0 → 0，256 → 256 */
    for (i = 0; i < (uint8_t)(sizeof(curves) / sizeof(curves[0])); i++) {
        CHECK(mui_ease(curves[i], 0) == 0, "缓动：t=0 应为 0");
        CHECK(mui_ease(curves[i], 256) == 256, "缓动：t=256 应为 256");
    }
    /* 2) 越界钳制 */
    CHECK(mui_ease(MUI_EASE_LINEAR, -100) == 0, "缓动：负进度应钳到 0");
    CHECK(mui_ease(MUI_EASE_LINEAR, 9999) == 256, "缓动：超界进度应钳到 256");
    /* 3) 形状：缓入慢于线性、缓出快于线性 */
    CHECK(mui_ease(MUI_EASE_IN_QUAD, 128) < 128, "缓动：in_quad 中点应慢于线性");
    CHECK(mui_ease(MUI_EASE_OUT_QUAD, 128) > 128, "缓动：out_quad 中点应快于线性");
    CHECK(mui_ease(MUI_EASE_IN_OUT_QUAD, 128) == 128, "缓动：in_out_quad 中点应恰过半");
    /* 4) 回弹应过冲 */
    {
        int16_t peak = 0, t;
        for (t = 0; t <= 256; t += 8) {
            int16_t v = mui_ease(MUI_EASE_OUT_BACK, t);
            if (v > peak) { peak = v; }
        }
        CHECK(peak > 256, "缓动：out_back 应有过冲（>256）");
    }

    /* 5) 补间 */
    {
        static mui_anim_t a;
        mui_anim_init(&a, 0, MUI_EASE_LINEAR);
        mui_anim_start(&a, 0, 100, 100);
        CHECK(!mui_anim_done(&a) && mui_anim_value(&a) == 0, "补间：起始状态");
        mui_anim_update(&a, 50);
        CHECK(mui_anim_value(&a) == 50, "补间：线性半程应为 50");
        mui_anim_update(&a, 50);
        CHECK(mui_anim_done(&a) && mui_anim_value(&a) == 100, "补间：结束应到位并完成");

        mui_anim_init(&a, 0, MUI_EASE_IN_QUAD);
        mui_anim_start(&a, 0, 100, 100);
        mui_anim_update(&a, 50);
        CHECK(mui_anim_value(&a) < 50, "补间：缓入半程应偏小");

        mui_anim_to(&a, 200, 100);                 /* 从当前值出发改目标 */
        CHECK(!mui_anim_done(&a), "补间：改目标后应重新运行");
        mui_anim_update(&a, 100);
        CHECK(mui_anim_value(&a) == 200, "补间：改目标后应到达 200");

        mui_anim_start(&a, 0, 55, 0);              /* 零时长立即到位 */
        CHECK(mui_anim_done(&a) && mui_anim_value(&a) == 55, "补间：零时长应立即到位");

        mui_anim_start(&a, 0, 100, 100);
        mui_anim_update(&a, 30);
        mui_anim_stop(&a);
        CHECK(mui_anim_done(&a), "补间：stop 后应完成");
        mui_anim_jump(&a, 7);
        CHECK(mui_anim_value(&a) == 7, "补间：jump 应立即设值");
    }

    /* 6) 时间基（含 32 位回绕） */
    mui_tick_update(1000);
    mui_tick_update(1033);
    CHECK(mui_tick_delta() == 33, "时间基：毫秒差应为 33");
    CHECK(mui_time_ms() == 1033, "时间基：当前时刻应为 1033");
    mui_tick_update(0xFFFFFFF0u);
    mui_tick_update(0x00000010u);                  /* 回绕：+32ms */
    CHECK(mui_tick_delta() == 32, "时间基：32 位回绕后差值应为 32");
}

/* -------- 测试：文本居中 / 对齐 -------- */
static void test_text_align(void)
{
    int16_t tw = mui_text_width("12", &lv_mono_font, 1);   /* 等宽 4px/字 → 8 */

    mui_reset_clip();
    CHECK(tw == 8, "对齐：测试字体 '12' 宽应为 8");
    CHECK(mui_text_align_x(10, 40, tw, MUI_ALIGN_LEFT) == 10, "对齐：左");
    CHECK(mui_text_align_x(10, 40, tw, MUI_ALIGN_CENTER) == 26, "对齐：居中");
    CHECK(mui_text_align_x(10, 40, tw, MUI_ALIGN_RIGHT) == 42, "对齐：右");
    CHECK(mui_text_align_x(10, 5, 20, MUI_ALIGN_CENTER) == 10,
          "对齐：文本超框时应退化为左对齐");
    CHECK(mui_text_align_y(10, 20, 8) == 16, "对齐：垂直居中");

    /* 左对齐：字形首像素列在 tx+1（'1' 第 0 行 = {0,255,0}），首行在 ty+1 = 17 */
    mui_screen_clear(TEST_BG);
    mui_text_draw_rect(10, 10, 40, 20, "12", &lv_mono_font,
                       MUI_BLACK, TEST_BG, 1, MUI_ALIGN_LEFT);
    CHECK(px(11, 17) == MUI_BLACK, "对齐：左对齐字形落点");
    CHECK(px(11, 16) == TEST_BG, "对齐：垂直居中上方留白");
    CHECK(px(27, 17) == TEST_BG, "对齐：左对齐不应出现在居中位置");

    /* 居中 */
    mui_screen_clear(TEST_BG);
    mui_text_draw_rect(10, 10, 40, 20, "12", &lv_mono_font,
                       MUI_BLACK, TEST_BG, 1, MUI_ALIGN_CENTER);
    CHECK(px(27, 17) == MUI_BLACK, "对齐：居中字形落点");
    CHECK(px(11, 17) == TEST_BG, "对齐：居中时左侧应为空");

    /* 右对齐 */
    mui_screen_clear(TEST_BG);
    mui_text_draw_rect(10, 10, 40, 20, "12", &lv_mono_font,
                       MUI_BLACK, TEST_BG, 1, MUI_ALIGN_RIGHT);
    CHECK(px(43, 17) == MUI_BLACK, "对齐：右对齐字形落点");

    /* 标签对齐：改文本后重新居中，且旧落点被擦除 */
    {
        static mui_label_t lbl;
        mui_screen_clear(TEST_BG);
        mui_label_init(&lbl, 10, 10, &lv_mono_font, MUI_BLACK, TEST_BG, 1);
        mui_label_set_align(&lbl, MUI_ALIGN_CENTER, 40);
        mui_label_set_text(&lbl, "12");            /* 居中起点 26 */
        CHECK(px(27, 11) == MUI_BLACK, "对齐：标签居中落点");

        mui_label_set_text(&lbl, "123");           /* 宽 12 → 居中起点 24 */
        CHECK(px(25, 11) == MUI_BLACK, "对齐：标签变长后应重新居中");
        CHECK(px(27, 11) == TEST_BG, "对齐：旧落点应被擦除");
    }

    mui_reset_clip();
}

/* -------- 测试：容器与布局 -------- */
static void test_layout(void)
{
    static mui_container_t c;
    static mui_container_t a, b;
    mui_layout_t l;
    mui_rect_t r;

    /* 容器：裁剪到矩形 */
    mui_reset_clip();
    mui_screen_clear(TEST_BG);
    mui_container_init(&c, 50, 50, 40, 30);
    mui_container_begin(&c);
    mui_rect_fill(0, 0, 480, 480, MUI_RED);      /* 只应落在容器内 */
    mui_container_end(&c);

    CHECK(px(50, 50) == MUI_RED, "容器：区域内绘制生效");
    CHECK(px(89, 79) == MUI_RED, "容器：区域右下角");
    CHECK(px(49, 60) == TEST_BG, "容器：左侧区域外未写");
    CHECK(px(90, 60) == TEST_BG, "容器：右侧区域外未写");
    CHECK(px(60, 80) == TEST_BG, "容器：下方区域外未写");

    /* 容器：滚动钳制 */
    mui_container_set_content(&c, 0, 100);        /* 内容 100 > 视口 30 */
    CHECK(mui_container_scroll_max_y(&c) == 70, "容器：最大滚动量");
    CHECK(mui_container_scroll_max_x(&c) == 0, "容器：宽度不滚");
    mui_container_set_scroll(&c, 0, 999);
    CHECK(mui_container_oy(&c) == (int16_t)(50 - 70), "容器：滚动被钳制");

    /* 嵌套容器：内层不得越出外层 */
    mui_screen_clear(TEST_BG);
    mui_container_init(&a, 100, 100, 60, 60);
    mui_container_init(&b, 150, 150, 60, 60);    /* 与外层部分重叠 */
    mui_container_begin(&a);
    mui_container_begin(&b);
    mui_rect_fill(0, 0, 480, 480, MUI_BLUE);
    mui_container_end(&b);
    mui_container_end(&a);
    CHECK(px(150, 150) == MUI_BLUE, "容器：嵌套交集内生效");
    CHECK(px(159, 159) == MUI_BLUE, "容器：嵌套交集右下角");
    CHECK(px(160, 160) == TEST_BG, "容器：外层边界外未写");

    /* 行布局 + 换行 */
    mui_layout_init(&l, 0, 0, 100, 100, MUI_LAYOUT_ROW, 10);
    r = mui_layout_next(&l, 40, 20);
    CHECK(r.x == 0 && r.y == 0 && r.x2 == 40 && r.y2 == 20, "布局：行首格");
    r = mui_layout_next(&l, 40, 20);
    CHECK(r.x == 50 && r.y == 0, "布局：行第二格（含间距）");
    r = mui_layout_next(&l, 40, 20);             /* 50+40 > 100 → 换行 */
    CHECK(r.x == 0 && r.y == 30 && r.y2 == 50, "布局：放不下时换行");

    /* 列布局 + 占满宽度 */
    mui_layout_init(&l, 10, 10, 80, 60, MUI_LAYOUT_COL, 4);
    r = mui_layout_next(&l, 0, 20);
    CHECK(r.x == 10 && (r.x2 - r.x) == 80 && (r.y2 - r.y) == 20,
          "布局：列布局占满宽度");

    /* 超出区域 → 空矩形 */
    CHECK(mui_layout_next(&l, 0, 999).x2 == 0, "布局：放不下返回空");

    /* 等分 */
    r = mui_layout_cols(0, 0, 100, 10, 4, 0, 0);
    CHECK(r.x == 0 && r.x2 == 25, "布局：四等分第 0 列");
    r = mui_layout_cols(0, 0, 100, 10, 4, 3, 0);
    CHECK(r.x == 75 && r.x2 == 100, "布局：四等分末列贴右边");
    r = mui_layout_rows(0, 0, 10, 90, 3, 1, 0);
    CHECK(r.y == 30 && r.y2 == 60, "布局：三等分中间行");

    /* 网格 */
    r = mui_layout_grid(0, 0, 100, 100, 2, 2, 1, 1, 10);
    CHECK(r.x == 55 && r.y == 55 && r.x2 == 100 && r.y2 == 100, "布局：网格右下单元");

    /* 矩形工具 */
    {
        mui_rect_t box;
        box.x = 10; box.y = 10; box.x2 = 110; box.y2 = 60;
        r = mui_rect_pad(box, 5);
        CHECK(r.x == 15 && r.y == 15 && r.x2 == 105 && r.y2 == 55, "布局：内缩");
        r = mui_rect_align(box, 20, 10, MUI_ALIGN_CENTER);
        CHECK(r.x == 50 && r.y == 30 && r.x2 == 70 && r.y2 == 40, "布局：居中放置");
        r = mui_rect_align(box, 20, 10, MUI_ALIGN_RIGHT);
        CHECK(r.x2 == 110, "布局：右对齐贴右边");
        CHECK(mui_rect_hit(box, 10, 10) == 1 && mui_rect_hit(box, 110, 10) == 0,
              "布局：命中测试（开区间）");
    }

    mui_reset_clip();
}

/* -------- 测试：仪表盘 -------- */
static void test_gauge(void)
{
    static const mui_gauge_style_t st = {
        MUI_RGB565(0x33, 0x3D, 0x4E),   /* track */
        MUI_RED,                        /* fill */
        MUI_WHITE,                      /* text */
        MUI_WHITE,                      /* bg */
        20,                             /* thick */
        0, 360,                         /* a0 / a1：整圆便于断言 */
        0,                              /* aa 关：纯色好断言（两种构型一致） */
        0, 0                            /* 不显示数值 */
    };
    static mui_gauge_t g;
    const uint16_t track = MUI_RGB565(0x33, 0x3D, 0x4E);

    mui_reset_clip();
    mui_screen_clear(MUI_WHITE);
    mui_gauge_init(&g, 100, 100, 60, 0, 100, 50, &st);
    mui_gauge_draw(&g);

    /* 外半径 60、环宽 20 → 环带 [40,60)；50% = 扫到 180°（下半平面） */
    CHECK(px(100, 150) == MUI_RED, "仪表盘：50% 时 90° 方向应为填充色");
    CHECK(px(150, 100) == MUI_RED, "仪表盘：50% 时 0° 方向应为填充色");
    CHECK(px(100, 50) == track, "仪表盘：270° 方向应为轨道色");
    CHECK(px(100, 100) == MUI_WHITE, "仪表盘：内孔不填充");
    CHECK(px(100, 165) == MUI_WHITE, "仪表盘：环外不填充");

    /* 值 → 角度映射 */
    CHECK(mui_gauge_value_angle(&g, 0) == 0, "仪表盘：0 值 → 0°");
    CHECK(mui_gauge_value_angle(&g, 50) == 180, "仪表盘：50 值 → 180°");
    CHECK(mui_gauge_value_angle(&g, 100) == 360, "仪表盘：100 值 → 360°");
    CHECK(mui_gauge_value_angle(&g, 999) == 360, "仪表盘：超范围值被夹取");

    /* 值未变 → 跳过重绘 */
    mui_rect_fill(145, 100, 10, 10, MUI_GREEN);
    mui_gauge_draw(&g);
    CHECK(px(150, 105) == MUI_GREEN, "仪表盘：值未变时应跳过重绘");

    /* 值改变 → 重绘 */
    mui_gauge_set_value(&g, 100);
    mui_gauge_draw(&g);
    CHECK(px(100, 50) == MUI_RED, "仪表盘：值满时应整环填充");

    mui_reset_clip();
}

/* -------- 测试：可滚动列表 -------- */
static void test_list_widget(void)
{
    static mui_list_t l;
    uint8_t first = 0, count = 0, idx = 0, chg;

    mui_reset_clip();
    mui_screen_clear(TEST_BG);
    mui_list_init(&l, 0, 0, 100, 60, 20, 0);       /* 节距 20 → 视口内约 3 行 */
    mui_list_set_rows(&l, 10);

    CHECK(mui_list_get_rows(&l) == 10, "列表：行数");
    CHECK(mui_list_max_scroll(&l) == 140, "列表：最大滚动量");
    CHECK(mui_list_visible(&l, &first, &count) == 3 && first == 0,
          "列表：初始可见 3 行");

    mui_list_set_scroll(&l, 25);
    CHECK(mui_list_get_scroll(&l) == 25, "列表：滚动设置");
    mui_list_visible(&l, &first, &count);
    CHECK(first == 1 && count == 4, "列表：滚动后可见范围（与视口相交的行）");

    mui_list_set_scroll(&l, 9999);
    CHECK(mui_list_get_scroll(&l) == mui_list_max_scroll(&l), "列表：滚动越界被钳制");
    mui_list_set_scroll(&l, -100);
    CHECK(mui_list_get_scroll(&l) == 0, "列表：负滚动被钳制");

    /* 行矩形随滚动偏移 */
    {
        mui_rect_t r = mui_list_row_rect(&l, 1);
        CHECK(r.y == 20 && r.y2 == 40, "列表：行矩形");
        mui_list_set_scroll(&l, 10);
        r = mui_list_row_rect(&l, 1);
        CHECK(r.y == 10, "列表：滚动后行矩形上移");
        mui_list_set_scroll(&l, 0);
    }

    /* 命中 */
    CHECK(mui_list_row_at(&l, 50, 25, &idx) == 1 && idx == 1, "列表：命中第 1 行");
    CHECK(mui_list_row_at(&l, 50, 200, &idx) == 0, "列表：视口外不命中");

    /* 触摸：点按选中 */
    mui_list_set_sel(&l, MUI_LIST_SEL_NONE);
    mui_touch_update(50, 25, 1);
    CHECK(mui_list_touch(&l, MUI_TOUCH_DOWN) == 0, "列表：按下不改选中");
    mui_touch_update(50, 25, 0);
    chg = mui_list_touch(&l, MUI_TOUCH_CLICK);
    CHECK((chg & MUI_LIST_CHANGED_SEL) != 0 && mui_list_get_sel(&l) == 1,
          "列表：点按选中第 1 行");

    /* 触摸：拖动滚动 */
    mui_list_set_scroll(&l, 50);
    mui_touch_update(50, 40, 1);
    (void)mui_list_touch(&l, MUI_TOUCH_DOWN);
    mui_touch_update(50, 20, 1);                   /* 上滑 20px */
    chg = mui_list_touch(&l, MUI_TOUCH_MOVE);
    CHECK((chg & MUI_LIST_CHANGED_SCROLL) != 0 && mui_list_get_scroll(&l) == 70,
          "列表：上滑使内容上移");
    mui_touch_update(50, 20, 0);
    (void)mui_list_touch(&l, MUI_TOUCH_UP);

    while (mui_touch_poll() != MUI_TOUCH_NONE) { }
    mui_reset_clip();
}

/* -------- 测试：模态弹窗 -------- */
static void test_popup(void)
{
    static mui_popup_t p;

    mui_reset_clip();
    mui_screen_clear(MUI_WHITE);
    mui_popup_init(&p, 60, 60, 200, 120, NULL, NULL, NULL);
    mui_popup_set_colors(&p, MUI_NAVY, MUI_WHITE, MUI_WHITE, MUI_WHITE);
    CHECK(mui_popup_add_button(&p, "OK", NULL) == 0, "弹窗：加按钮 0");
    CHECK(mui_popup_add_button(&p, "NO", NULL) == 1, "弹窗：加按钮 1");
    CHECK(mui_popup_is_open(&p) == 0, "弹窗：初始关闭");

    mui_popup_open(&p);
    CHECK(mui_popup_is_open(&p) == 1, "弹窗：已打开");
    mui_popup_draw(&p);
    CHECK(px(160, 80) == MUI_NAVY, "弹窗：面板已绘制");
    CHECK(px(20, 20) == MUI_WHITE, "弹窗：面板外不受影响");

    /* 点第二个按钮 → 返回索引 1 并关闭 */
    {
        int8_t r;
        mui_touch_update(210, 160, 1);             /* 底部按钮行右侧 */
        (void)mui_popup_touch(&p, MUI_TOUCH_DOWN);
        mui_touch_update(210, 160, 0);
        r = mui_popup_touch(&p, MUI_TOUCH_CLICK);
        CHECK(r == 1, "弹窗：点第二个按钮应返回索引 1");
        CHECK(mui_popup_is_open(&p) == 0, "弹窗：点按钮后关闭");
    }

    /* 点面板外 → MUI_POPUP_CLOSED */
    mui_popup_open(&p);
    mui_touch_update(10, 10, 1);
    mui_touch_update(10, 10, 0);
    CHECK(mui_popup_touch(&p, MUI_TOUCH_CLICK) == MUI_POPUP_CLOSED,
          "弹窗：点面板外应取消关闭");
    CHECK(mui_popup_is_open(&p) == 0, "弹窗：点外部后关闭");

    while (mui_touch_poll() != MUI_TOUCH_NONE) { }
    mui_reset_clip();
}

/* -------- 测试：下拉框 -------- */
static void test_dropdown(void)
{
    static mui_dropdown_t d;
    static const char *const items[4] = { "A", "B", "C", "D" };

    mui_reset_clip();
    mui_screen_clear(MUI_WHITE);
    mui_dropdown_init(&d, 20, 20, 120, 24, items, 4, 20, NULL, NULL);
    CHECK(mui_dropdown_is_open(&d) == 0 && mui_dropdown_selected(&d) == 0,
          "下拉：初始状态");

    /* 点按钮 → 展开 */
    mui_touch_update(60, 30, 1);
    mui_touch_update(60, 30, 0);
    CHECK(mui_dropdown_touch(&d, MUI_TOUCH_CLICK) == MUI_DROPDOWN_CHANGED,
          "下拉：点按钮应展开");
    CHECK(mui_dropdown_is_open(&d) == 1, "下拉：已展开");

    /* 展开列表矩形有效 */
    {
        mui_rect_t r;
        CHECK(mui_dropdown_popup_rect(&d, &r) == 1, "下拉：展开矩形有效");
        CHECK(r.y == 44 && (r.y2 - r.y) == 80, "下拉：展开列表位于按钮下方");
    }

    /* 点第 3 项（索引 2）→ 选中并收起 */
    {
        mui_rect_t r;
        int16_t py;
        uint8_t res;
        (void)mui_dropdown_popup_rect(&d, &r);
        py = (int16_t)(r.y + 2 * 20 + 10);
        mui_touch_update(60, py, 1);
        (void)mui_dropdown_touch(&d, MUI_TOUCH_DOWN);
        mui_touch_update(60, py, 0);
        res = mui_dropdown_touch(&d, MUI_TOUCH_CLICK);
        CHECK(res == MUI_DROPDOWN_CLOSED, "下拉：选中后应收起");
        CHECK(mui_dropdown_selected(&d) == 2, "下拉：选中的是第 3 项");
        CHECK(mui_dropdown_is_open(&d) == 0, "下拉：已收起");
    }

    /* 点外部 → 收起 */
    mui_dropdown_open(&d);
    mui_touch_update(400, 400, 1);
    mui_touch_update(400, 400, 0);
    CHECK(mui_dropdown_touch(&d, MUI_TOUCH_CLICK) == MUI_DROPDOWN_CLOSED,
          "下拉：点外部应收起");

    while (mui_touch_poll() != MUI_TOUCH_NONE) { }
    mui_reset_clip();
}

/* -------- 测试：滑块圆头与填充的交界（底色必须按方向分开判） -------- */

/**
 * 旧实现把"圆头像素底下压的是什么颜色"按水平条的逻辑（圆心左=填充、右=轨道）
 * 硬套到垂直条上。垂直条是**自下而上**填充，圆头正下方才是填充色，于是圆头
 * 正下方整条带被当成"轨道色/页面底色"，与填充的交界混出一条暗线。
 *
 * 这里用"填充色 == 圆头边色"的配色（demo 里就是这么配的）做精确断言：
 * 交界处若没有混错底色，混色结果必须仍等于填充色本身。
 */
static void test_slider_rim(void)
{
    mui_slider_style_t st = mui_slider_style_default;
    mui_slider_t s;
    mui_rect_t b;
    const uint16_t fill = MUI_RGB565(0x2E, 0xCC, 0x71);
    int16_t kr = 13, cx, cy;

    st.track       = MUI_RGB565(0x33, 0x3D, 0x4E);
    st.fill        = fill;
    st.knob        = MUI_WHITE;
    st.knob_border = fill;        /* 与填充同色：交界处应完全无缝 */
    st.screen_bg   = MUI_BLACK;   /* 与填充反差极大，混错底色立刻现形 */
    st.thickness   = 10;
    st.knob_r      = kr;
    st.aa          = 1;

    /* ---- 垂直：圆头下边缘必须仍是填充色（不得混出暗线） ---- */
    st.dir = MUI_SLIDER_VERTICAL;
    mui_screen_clear(MUI_BLACK);
    mui_slider_init(&s, 100, 100, 30, 96, 0, 100, 40, &st);
    mui_slider_draw(&s);

    cx = (int16_t)(s.x + s.w / 2);
    cy = (int16_t)(s.y + (s.h - 1 - 38));   /* 值 40：pos 距顶部 (96-1)*40/100 行 */

    CHECK(mui_slider_get_bounds(&s, &b) == 1, "滑块：get_bounds 应返回成功");
    CHECK(b.x == (int16_t)(cx - kr - 1) && b.y == (int16_t)(s.y - kr) &&
          b.x2 == (int16_t)(b.x + 2 * kr + 3) &&
          b.y2 == (int16_t)(s.y + s.h + kr),
          "滑块：占位矩形未按圆头半径外扩（相邻控件会被擦掉一条）");

    CHECK(px(cx, (int16_t)(cy + kr)) == fill,
          "垂直滑块：圆头下边缘与填充之间混出暗线（底色按左右分而非上下分）");
    CHECK(px((int16_t)(cx + 2), (int16_t)(cy + kr)) == fill,
          "垂直滑块：圆头右下角与填充之间混出暗线");

    /* ---- 水平：左侧交界仍是填充色（该方向本来就是对的，防止被改坏） ---- */
    st.dir = MUI_SLIDER_HORIZONTAL;
    mui_screen_clear(MUI_BLACK);
    mui_slider_init(&s, 100, 100, 104, 30, 0, 100, 65, &st);
    mui_slider_draw(&s);

    cx = (int16_t)(s.x + (104 - 1) * 65 / 100);
    cy = (int16_t)(s.y + s.h / 2);
    CHECK(px((int16_t)(cx - kr), cy) == fill,
          "水平滑块：圆头左边缘与填充之间混出暗线");
}

/* -------- 测试：仪表盘中心数值重绘必须擦掉旧值 -------- */

/** @brief 统计矩形内非背景色像素数（只用于内孔这种小区域） */
static int count_non_bg(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        uint16_t bg)
{
    int16_t x, y;
    int n = 0;

    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            if (px(x, y) != bg) {
                n++;
            }
        }
    }
    return n;
}

/**
 * 中心内孔在圆环的覆盖范围之外，重画圆环擦不掉旧数字。旧实现每次改值都
 * 直接把新数字画上去，于是新旧数值叠成一团（数字宽度一变尤其明显）。
 *
 * 用例：先画 "12"（两位）再画 "3"（一位），内孔里的非底色像素数必须与
 * "干净底上直接画 3"完全一致 —— 多出来的就是没擦掉的旧值残留。
 */
static void test_gauge_value_repaint(void)
{
    mui_gauge_style_t st = mui_gauge_style_default;
    mui_gauge_t g;
    const uint16_t bg = MUI_BLACK;
    const int16_t cx = 200, cy = 200, r = 60;
    int16_t rin = 45;            /* r - thick */
    int ref, got;

    st.track      = MUI_RGB565(0x33, 0x3D, 0x4E);
    st.fill       = MUI_RGB565(0x04, 0xAA, 0xF4);
    st.text       = MUI_WHITE;
    st.bg         = bg;
    st.thick      = 15;
    st.aa         = 0;           /* 关 AA：边缘颜色确定，统计更干净 */
    st.show_value = 1;
    st.percent    = 0;           /* 走原值分支：测试字体只有 '1'~'3' */

    /* 参考：干净底上直接画 "3" */
    mui_screen_clear(bg);
    mui_gauge_init(&g, cx, cy, r, 0, 99, 3, &st);
    mui_gauge_set_font(&g, &lv_mono_font);
    mui_gauge_draw(&g);
    ref = count_non_bg((int16_t)(cx - 30), (int16_t)(cy - 30),
                       (int16_t)(cx + 30), (int16_t)(cy + 30), bg);

    /* 先 "12" 后 "3"：旧值必须被擦掉 */
    mui_screen_clear(bg);
    mui_gauge_init(&g, cx, cy, r, 0, 99, 12, &st);
    mui_gauge_set_font(&g, &lv_mono_font);
    mui_gauge_draw(&g);
    mui_gauge_set_value(&g, 3);
    mui_gauge_draw(&g);
    got = count_non_bg((int16_t)(cx - 30), (int16_t)(cy - 30),
                       (int16_t)(cx + 30), (int16_t)(cy + 30), bg);

    CHECK(rin > 30, "仪表盘：用例前提被破坏（内孔太小，取样区会碰到圆环）");
    CHECK(ref > 0, "仪表盘：参考数值没画出来（测试字体用例失效）");
    CHECK(got == ref, "仪表盘：改值后中心残留旧数字（内孔未擦，新旧数值叠字）");

    /* 关掉数值显示后，旧数字也要被清掉 */
    st.show_value = 0;
    mui_gauge_set_style(&g, &st);    /* 顺带作废绘制状态，强制走全流程 */
    mui_gauge_draw(&g);
    CHECK(count_non_bg((int16_t)(cx - 30), (int16_t)(cy - 30),
                       (int16_t)(cx + 30), (int16_t)(cy + 30), bg) == 0,
          "仪表盘：关掉数值显示后中心仍残留旧数字");
}

int main(void)
{
    mui_init(SIM_W, SIM_H);

    test_clipping();
    test_clip_region();
    test_clip_mask();
    test_dirty();
    test_slider_rim();
    test_gauge_value_repaint();
    test_button_cache();
    test_anim();
    test_text_align();
    test_layout();
    test_gauge();
    test_list_widget();
    test_popup();
    test_dropdown();
    test_rect();
    test_round_rect();
    test_line();
    test_circle();
    test_ellipse();
    test_triangle();
    test_label_diff();
    test_label_span();
    test_label_exact_span();
    test_text_flags_zero();
    test_text_bold();
    test_text_decor_lines();
    test_text_cell_matches_sparse();
    test_label_decor();
    test_label_colors();
#if MUI_CFG_AA
    test_round_rect_aa();
    test_round_rect_aa_bg();
    test_round_rect_draw_aa();
    test_progressbar_aa();
    test_progressbar_track_aa();
    test_progressbar_full_cover();
    test_button_aa();
#else
    /* 全局关 AA 时跳过所有"抗锯齿形状"相关断言；引用一下避免"定义未使用"告警 */
    (void)test_round_rect_aa; (void)test_round_rect_aa_bg;
    (void)test_round_rect_draw_aa; (void)test_progressbar_aa;
    (void)test_progressbar_track_aa; (void)test_progressbar_full_cover;
    (void)test_button_aa;
#endif
    test_ring();
    test_button_image();
    test_image_scale_nine();
    test_slider();
    test_slider_trail();
    test_toggle();

    render_demo();

    if (test_failed == 0 && sim_violations() == 0) {
        printf("全部测试通过，测试图已导出 test_output.png\n");
        return 0;
    }
    printf("测试失败数：%d（底层违规：%d）\n", test_failed, sim_violations());
    return 1;
}
