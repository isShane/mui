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
    test_round_rect_aa();
    test_round_rect_aa_bg();

    render_demo();

    if (test_failed == 0 && sim_violations() == 0) {
        printf("全部测试通过，测试图已导出 test_output.png\n");
        return 0;
    }
    printf("测试失败数：%d（底层违规：%d）\n", test_failed, sim_violations());
    return 1;
}
