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

    mui_screen_flush();   /* 缓冲后端：推屏后才能导出完整画面 */
    sim_export_png(demo_png_name());
}

int main(void)
{
    mui_init(SIM_W, SIM_H);

    test_clipping();
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
    test_round_rect_aa();
    test_round_rect_aa_bg();
    test_round_rect_draw_aa();
    test_progressbar_aa();
    test_progressbar_track_aa();
    test_progressbar_full_cover();

    render_demo();

    if (test_failed == 0 && sim_violations() == 0) {
        printf("全部测试通过，测试图已导出 test_output.png\n");
        return 0;
    }
    printf("测试失败数：%d（底层违规：%d）\n", test_failed, sim_violations());
    return 1;
}
