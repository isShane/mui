/**
 * @file test_main.c
 * @brief MUI 自动化测试：边界裁剪安全性 + 图元几何正确性 + 渲染测试图
 */

#include <stdio.h>
#include <string.h>
#include "mui.h"
#include "mui_font.h"
#include "mui_label.h"
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
    mui_draw_pixel(-5, -5, MUI_WHITE);
    mui_draw_pixel(10000, 10000, MUI_WHITE);
    mui_fill_rect(-50, -50, 100, 100, MUI_WHITE);
    mui_fill_rect(300, 200, 100, 100, MUI_WHITE);
    mui_fill_rect(10, 10, -3, 5, MUI_WHITE);
    mui_fill_rect(10, 10, 5, -3, MUI_WHITE);
    mui_draw_rect(-100, -100, 500, 500, MUI_WHITE);
    mui_fill_round_rect(-30, -30, 60, 60, 20, MUI_WHITE);
    mui_fill_round_rect(310, 230, 50, 50, 100, MUI_WHITE);  /* r 自动限幅 */
    mui_draw_round_rect(-10, 200, 400, 60, 15, MUI_WHITE);
    mui_draw_line(-20, -20, 400, 300, MUI_WHITE);
    mui_fill_circle(-10, -10, 50, MUI_WHITE);
    mui_fill_circle(330, 250, 40, MUI_WHITE);
    mui_draw_circle(160, 120, 500, MUI_WHITE);
    mui_fill_ellipse(-5, -5, 30, 20, MUI_WHITE);
    mui_draw_ellipse(330, 250, 50, 40, MUI_WHITE);
    mui_fill_triangle(-30, -30, 350, 10, 160, 280, MUI_WHITE);
    mui_draw_triangle(-40, -40, 400, 100, 200, 300, MUI_WHITE);
    mui_clear_screen(MUI_WHITE);
    CHECK(sim_violations() == 0, "越界绘制未触发底层违规");
}

/* -------- 测试：矩形 -------- */
static void test_rect(void)
{
    mui_clear_screen(MUI_WHITE);

    /* 实心矩形区域与边界 */
    mui_fill_rect(10, 10, 20, 15, MUI_RED);
    CHECK(px(10, 10) == MUI_RED, "fill_rect 左上角");
    CHECK(px(29, 24) == MUI_RED, "fill_rect 右下角");
    CHECK(px(9, 10) == TEST_BG, "fill_rect 左边界外");
    CHECK(px(30, 10) == TEST_BG, "fill_rect 右边界外");
    CHECK(px(10, 25) == TEST_BG, "fill_rect 下边界外");

    /* 空心矩形只描边 */
    mui_clear_screen(MUI_WHITE);
    mui_draw_rect(5, 5, 30, 20, MUI_GREEN);
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
    mui_clear_screen(MUI_WHITE);

    /* r=0 等价于直角矩形 */
    mui_fill_round_rect(10, 10, 40, 30, 0, MUI_BLUE);
    CHECK(px(10, 10) == MUI_BLUE, "r=0 左上角填充");

    /* 圆角：角点不填充，弧线内侧填充，直边区填充 */
    mui_clear_screen(MUI_WHITE);
    mui_fill_round_rect(10, 10, 40, 30, 10, MUI_YELLOW);
    CHECK(px(10, 10) == TEST_BG, "左上角最外点不填充");
    CHECK(px(49, 39) == TEST_BG, "右下角最外点不填充");
    CHECK(px(14, 14) == MUI_YELLOW, "左上角弧内侧填充（距角圆心平方72<100）");
    CHECK(px(10, 25) == MUI_YELLOW, "左直边区填充");
    CHECK(px(25, 10) == MUI_YELLOW, "顶直边区填充");
    CHECK(px(49, 25) == MUI_YELLOW, "右直边区填充");
    CHECK(px(30, 39) == MUI_YELLOW, "底直边区填充");
    CHECK(px(30, 25) == MUI_YELLOW, "中心填充");

    /* r 超过短边一半时自动限幅（w=20 → r 上限10，胶囊形） */
    mui_clear_screen(MUI_WHITE);
    mui_fill_round_rect(100, 100, 20, 40, 99, MUI_CYAN);
    CHECK(px(100, 100) == TEST_BG, "胶囊形角点外不填充");
    CHECK(px(100, 110) == MUI_CYAN, "胶囊形圆心行最左点填充");
    CHECK(px(110, 100) == MUI_CYAN, "胶囊形顶点填充");
    CHECK(px(100, 129) == MUI_CYAN, "胶囊形下圆心行最左点填充");
}

/* -------- 测试：直线 -------- */
static void test_line(void)
{
    mui_clear_screen(MUI_WHITE);

    /* 水平线 */
    mui_draw_line(0, 5, 10, 5, MUI_RED);
    for (int x = 0; x <= 10; x++) {
        CHECK(px(x, 5) == MUI_RED, "水平线逐点");
    }
    CHECK(px(11, 5) == TEST_BG, "水平线终点外");

    /* 垂直线 */
    mui_draw_line(3, 0, 3, 8, MUI_GREEN);
    for (int y = 0; y <= 8; y++) {
        CHECK(px(3, y) == MUI_GREEN, "垂直线逐点");
    }

    /* 对角线：两端点必在 */
    mui_clear_screen(MUI_WHITE);
    mui_draw_line(0, 0, 100, 50, MUI_WHITE);
    CHECK(px(0, 0) == MUI_WHITE, "对角线起点");
    CHECK(px(100, 50) == MUI_WHITE, "对角线终点");

    /* 反向绘制同样生效 */
    mui_clear_screen(MUI_WHITE);
    mui_draw_line(100, 50, 0, 0, MUI_WHITE);
    CHECK(px(0, 0) == MUI_WHITE, "反向直线起点");
    CHECK(px(100, 50) == MUI_WHITE, "反向直线终点");
}

/* -------- 测试：圆 -------- */
static void test_circle(void)
{
    int cnt_in;
    int cnt_out;

    mui_clear_screen(MUI_WHITE);

    /* 实心圆 r=20：中心、四方向边缘内1px 均应填充 */
    mui_fill_circle(100, 100, 20, MUI_MAGENTA);
    CHECK(px(100, 100) == MUI_MAGENTA, "圆心");
    CHECK(px(80, 100) == MUI_MAGENTA, "圆最左点");
    CHECK(px(120, 100) == MUI_MAGENTA, "圆最右点");
    CHECK(px(100, 80) == MUI_MAGENTA, "圆最上点");
    CHECK(px(100, 120) == MUI_MAGENTA, "圆最下点");
    CHECK(px(79, 100) == TEST_BG, "圆外左侧");
    CHECK(px(121, 100) == TEST_BG, "圆外右侧");

    /* 空心圆 r=30：统计边界环带像素数量级 */
    mui_clear_screen(TEST_BG);
    mui_draw_circle(160, 100, 30, MUI_BLACK);
    cnt_in = 0;
    for (int y = 70; y <= 130; y++) {
        for (int x = 130; x <= 190; x++) {
            if (px(x, y) == MUI_BLACK) {
                cnt_in++;
            }
        }
    }
    /* 周长约 2*pi*30 ≈ 188，允许 ±30% 波动 */
    CHECK(cnt_in > 130 && cnt_in < 250, "空心圆周长像素数合理");

    /* 实心圆面积 ≈ pi*r^2 */
    mui_clear_screen(TEST_BG);
    mui_fill_circle(160, 100, 30, MUI_BLACK);
    cnt_in = 0;
    cnt_out = 0;
    for (int y = 70; y <= 130; y++) {
        for (int x = 130; x <= 190; x++) {
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
    mui_clear_screen(MUI_WHITE);

    mui_fill_ellipse(100, 100, 40, 20, MUI_ORANGE);
    CHECK(px(100, 100) == MUI_ORANGE, "椭圆中心");
    CHECK(px(60, 100) == MUI_ORANGE, "椭圆最左点");
    CHECK(px(140, 100) == MUI_ORANGE, "椭圆最右点");
    CHECK(px(100, 80) == MUI_ORANGE, "椭圆最上点");
    CHECK(px(100, 120) == MUI_ORANGE, "椭圆最下点");
    CHECK(px(59, 100) == TEST_BG, "椭圆外左侧");
    CHECK(px(100, 79) == TEST_BG, "椭圆外上方");

    /* 空心椭圆四极值点在曲线上 */
    mui_clear_screen(MUI_WHITE);
    mui_draw_ellipse(200, 100, 50, 25, MUI_WHITE);
    CHECK(px(200, 75) == MUI_WHITE, "空心椭圆顶点");
    CHECK(px(200, 125) == MUI_WHITE, "空心椭圆底点");
    CHECK(px(150, 100) == MUI_WHITE, "空心椭圆左点");
    CHECK(px(250, 100) == MUI_WHITE, "空心椭圆右点");
    CHECK(px(200, 100) == TEST_BG, "空心椭圆内部为空");

    /* rx==ry 时退化为圆 */
    mui_clear_screen(MUI_WHITE);
    mui_draw_ellipse(60, 60, 15, 15, MUI_CYAN);
    CHECK(px(60, 45) == MUI_CYAN, "rx==ry 顶点");
    CHECK(px(45, 60) == MUI_CYAN, "rx==ry 左点");
}

/* -------- 测试：三角形 -------- */
static void test_triangle(void)
{
    mui_clear_screen(MUI_WHITE);

    /* 实心三角形：三个顶点、形心应在内部，远离边的包围盒角落应在外 */
    mui_fill_triangle(50, 50, 150, 50, 100, 120, MUI_GREEN);
    CHECK(px(50, 50) == MUI_GREEN, "三角形顶点0");
    CHECK(px(150, 50) == MUI_GREEN, "三角形顶点1");
    CHECK(px(100, 120) == MUI_GREEN, "三角形顶点2");
    CHECK(px(100, 70) == MUI_GREEN, "三角形形心附近");
    CHECK(px(52, 115) == TEST_BG, "三角形下方左侧外");
    CHECK(px(148, 115) == TEST_BG, "三角形下方右侧外");

    /* 底边水平、顶点朝下的三角形 */
    mui_clear_screen(MUI_WHITE);
    mui_fill_triangle(20, 20, 60, 20, 40, 60, MUI_RED);
    CHECK(px(40, 20) == MUI_RED, "倒三角底边中点");
    CHECK(px(40, 60) == MUI_RED, "倒三角下顶点");
    CHECK(px(40, 40) == MUI_RED, "倒三角中轴");

    /* 完全退化为水平线 */
    mui_clear_screen(MUI_WHITE);
    mui_fill_triangle(10, 30, 50, 30, 30, 30, MUI_WHITE);
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

static const mui_lv_font_cmap_t lv_test_cmap[] = {
    {'1', '3', 0},
};

static const mui_lv_font_t lv_test_font = {
    lv_test_bitmap, lv_test_glyphs, lv_test_cmap, 1, 0, 0, 8, 2
};

/** @brief 等宽测试字体：三字形 adv_w 相同，用于命中整格覆盖快路径 */
static const mui_glyph_dsc_t lv_mono_glyphs[] = {
    {0,  LV_ADV(4), 3, 5, 0, 0},
    {15, LV_ADV(4), 3, 5, 0, 0},
    {30, LV_ADV(4), 3, 5, 0, 0},
};

static const mui_lv_font_t lv_mono_font = {
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
 * @param font  字体（NULL 用 5x7 点阵）
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
        mui_clear_screen(TEST_BG);
        mui_label_init(&ref, x, y, font, MUI_BLACK, TEST_BG, 1);
        mui_label_set_text(&ref, cases[i][1]);
        memcpy(label_expect, sim_get_fb(), sizeof(label_expect));

        /* 实际：先画初值，再增量更新到最终文本 */
        mui_clear_screen(TEST_BG);
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
    /* 覆盖：同位替换 / 前缀变化 / 变长 / 变短 / 全变 / 中间插入 / 不变 / 清空 */
    static const char *cases5x7[][2] = {
        {"12:01", "12:02"},
        {"12:01", "12:00"},
        {"9:59",  "10:00"},
        {"1",     "12:34"},
        {"12:34", "1"},
        {"12:01", "22:11"},
        {"12:09", "12:10"},
        {"AD",    "A:D"},
        {"12:01", "12:01"},
        {"12:34", ""},
        {"",      "12:34"},
    };
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

    /* 空指针安全 */
    mui_label_set_text(NULL, "X");
    mui_label_init(&lbl, 40, 40, NULL, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, NULL);

    label_diff_run(NULL, cases5x7,
                   sizeof(cases5x7) / sizeof(cases5x7[0]), "5x7");
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
    /* "12:01" 宽 29px，哨兵放在 x+30：整块擦会波及，增量擦不波及 */
    const int16_t sx = (int16_t)(x + 30);
    const int16_t sy = 43;

    mui_clear_screen(TEST_BG);
    mui_label_init(&lbl, x, y, NULL, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, "12:01");

    mui_draw_pixel(sx, sy, MUI_GREEN);
    mui_label_set_text(&lbl, "12:02");
    CHECK(px(sx, sy) == MUI_GREEN, "增量更新未擦除差异区间外的像素");

    /* 文本完全相同时不擦不画 */
    mui_clear_screen(TEST_BG);
    mui_label_init(&lbl, x, y, NULL, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, "12:01");
    mui_draw_pixel(sx, sy, MUI_GREEN);
    mui_label_set_text(&lbl, "12:01");
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
    /* "12:01"：'0' 占 x+18..x+23，'1' 占 x+24..x+28。
       哨兵放在 '0' 区间内——精确模式擦除自 x+24 起不波及，
       安全模式擦除自 x+18 起会被擦掉 */
    const int16_t sx = (int16_t)(x + 20);

    mui_clear_screen(TEST_BG);
    mui_label_init(&lbl, x, y, NULL, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&lbl, "12:01");

    mui_draw_pixel(sx, 43, MUI_GREEN);
    mui_label_set_text(&lbl, "12:02");
    CHECK(px(sx, 43) == MUI_GREEN, "精确模式未重画前缀字符");
#endif
}

/* -------- 渲染测试图并导出 PNG -------- */
static void render_demo(void)
{
    mui_clear_screen(MUI_WHITE);

    /* 顶部标题栏：圆角矩形 */
    mui_fill_round_rect(10, 10, 300, 40, 12, MUI_NAVY);
    mui_draw_round_rect(10, 10, 300, 40, 12, MUI_CYAN);

    /* 色块矩阵 */
    for (int i = 0; i < 6; i++) {
        mui_fill_round_rect((int16_t)(14 + i * 50), 60, 44, 30, 8,
                            (uint16_t)(0xF800 >> (i % 3))
                            | (uint16_t)(i * 0x0421));
    }

    /* 左侧：同心圆 */
    mui_draw_circle(70, 180, 45, MUI_GREEN);
    mui_draw_circle(70, 180, 30, MUI_CYAN);
    mui_fill_circle(70, 180, 15, MUI_RED);

    /* 中间：椭圆 + 三角形 */
    mui_fill_ellipse(170, 180, 40, 25, MUI_BLUE);
    mui_fill_triangle(230, 205, 270, 205, 250, 150, MUI_YELLOW);

    /* 右侧：渐变竖条 + 对角线 */
    for (int i = 0; i < 8; i++) {
        mui_fill_rect(288, (int16_t)(140 + i * 12), 24, 10,
                      (uint16_t)(0x001F + i * 0x0400));
    }
    mui_draw_line(230, 60, 310, 130, MUI_WHITE);

    sim_export_png("test_output.png");
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

    render_demo();

    if (test_failed == 0 && sim_violations() == 0) {
        printf("全部测试通过，测试图已导出 test_output.png\n");
        return 0;
    }
    printf("测试失败数：%d（底层违规：%d）\n", test_failed, sim_violations());
    return 1;
}
