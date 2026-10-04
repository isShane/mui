/**
 * @file test_strip_main.c
 * @brief 条带缓冲后端（MUI_OUTPUT_STRIP）回归测试
 *
 * 核心不变量：**带高不影响结果**。
 * 同一份绘制代码，无论带高取 1 行（每行都是带边界，最苛刻）还是整屏，
 * 最终屏上的像素必须完全一致 —— 因为每个带都会被完整重画一遍。
 *
 * 覆盖点：
 *   1) 绝对断言：不依赖"参照帧"是否正确，直接核对若干关键像素；
 *   2) 带高 1/2/3/5/7/13/16/32 与上限逐像素比对（含跨带大块、跨带位图、容器裁剪）；
 *   3) 同一帧连画两次结果一致（重放幂等，无缓存残留）；
 *   4) 帧外调用 set_* 不得写屏，但下一帧必须体现新状态；
 *   5) 全程零越界违规（移植层坐标校验）。
 *
 * 产物：无（纯断言）；构建目标 mui_test_strip。
 */

#include <stdio.h>
#include <string.h>
#include "mui.h"
#include "mui_font.h"
#include "mui_label.h"
#include "mui_button.h"
#include "mui_progressbar.h"
#include "mui_slider.h"
#include "mui_gauge.h"
#include "mui_layout.h"
#include "sim_helper.h"

#define TEST_BG     MUI_WHITE
#define FB_PIXELS   ((int32_t)SIM_W * (int32_t)SIM_H)
#define FB_BYTES    (sizeof(uint16_t) * (size_t)FB_PIXELS)

static int test_failed = 0;

/** @brief 断言宏（与 test_main.c 同构） */
#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            printf("[FAIL] %s (line %d)\n", msg, __LINE__);         \
            test_failed++;                                          \
        }                                                           \
    } while (0)

/* ---------------- 迷你字体（3 个 3x5 字形，只含 '1'~'3'） ---------------- */

#define LV_ADV(px) ((uint16_t)((px) * 16))

static const uint8_t s_glyph_bmp[] = {
    0, 255, 0, 255, 255, 0, 0, 255, 0, 0, 255, 0, 255, 255, 255,   /* '1' */
    255, 255, 255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, /* '2' */
    255, 255, 0, 0, 0, 255, 0, 255, 0, 0, 0, 255, 255, 255, 0,     /* '3' */
};

static const mui_glyph_dsc_t s_glyphs[] = {
    {0,  LV_ADV(3), 3, 5, 0, 0},
    {15, LV_ADV(4), 3, 5, 0, 0},
    {30, LV_ADV(6), 3, 5, 0, 0},
};

static const mui_font_cmap_t s_cmap[] = {
    {'1', '3', 0},
};

static const mui_font_t s_font = {
    s_glyph_bmp, s_glyphs, s_cmap, 1, 0, 0, 8, 2
};

/* ---------------- 测试对象与素材 ---------------- */

static mui_button_t        s_btn;
static mui_progressbar_t   s_pb;
static mui_label_t         s_lbl;
static mui_gauge_t         s_gauge;
static mui_slider_t        s_sl, s_vsl;
static mui_container_t     s_box;

#define IMG_W  3
#define IMG_H  20
static uint16_t s_img_px[IMG_W * IMG_H];    /**< 小位图：竖着跨带 */
static uint8_t  s_mask[IMG_W * IMG_H];      /**< 8bpp 蒙版：0 / 半透明 / 全不透明 */

/** @brief 竖排滑块样式（默认样式是水平的） */
static const mui_slider_style_t s_vsl_style = {
    MUI_DARKGREY, MUI_BLUE, MUI_WHITE, MUI_BLUE, TEST_BG, 0, 0,
    MUI_SLIDER_VERTICAL, 1
};

/** @brief 仪表盘样式：白底上必须用深色字，否则看不出来 */
static const mui_gauge_style_t s_gauge_style = {
    MUI_DARKGREY, MUI_BLUE, MUI_BLACK, TEST_BG, 12, 135, 405, 1, 1, 0
};

static void init_assets(void)
{
    int i;

    for (i = 0; i < IMG_W * IMG_H; i++) {
        /* 三列不同颜色，方便定位"哪一列错位" */
        s_img_px[i] = (uint16_t)((i % IMG_W) == 0 ? MUI_ORANGE
                                : (i % IMG_W) == 1 ? MUI_PURPLE : MUI_GREEN);
        s_mask[i] = (uint8_t)((i % IMG_W) == 0 ? 0
                             : (i % IMG_W) == 1 ? 128 : 255);
    }
}

static void setup_objects(void)
{
    mui_label_init(&s_lbl, 400, 430, &s_font, MUI_BLACK, TEST_BG, 1);
    mui_label_set_text(&s_lbl, "12");

    mui_button_init(&s_btn, 800, 520, 180, 40, NULL, "12", NULL);
    mui_button_set_font(&s_btn, &s_font);

    mui_progressbar_init(&s_pb, 400, 480, 220, 24, NULL);
    mui_progressbar_set_value(&s_pb, 65);

    mui_gauge_init(&s_gauge, 200, 460, 60, 0, 100, 40, &s_gauge_style);
    mui_gauge_set_font(&s_gauge, &s_font);

    mui_slider_init(&s_sl, 700, 420, 200, 32, 0, 100, 60, NULL);
    mui_slider_init(&s_vsl, 950, 320, 32, 200, 0, 100, 30, &s_vsl_style);
}

/* ---------------- 绘制场景（必须可重放：先铺满底色再画全部内容） ---------------- */

static void scene_draw(void *ctx)
{
    uint8_t i;

    (void)ctx;

    mui_reset_clip();                       /* 条带后端下"全屏"== 当前带 */
    mui_screen_clear(TEST_BG);

    /* 基础图元 */
    mui_rect_fill(10, 10, 120, 40, MUI_RED);
    mui_rect_draw(200, 10, 120, 40, MUI_BLACK);
    mui_circle_fill(360, 40, 35, MUI_GREEN);
    mui_ellipse_fill(480, 40, 52, 30, MUI_NAVY);
    mui_line_draw(0, 120, 620, 186, MUI_MAROON);
    mui_round_rect_fill_aa(700, 10, 120, 50, 16, MUI_BLUE, TEST_BG);
    mui_circle_draw_aa(890, 40, 34, MUI_PURPLE, TEST_BG);
    mui_triangle_fill(300, 120, 420, 120, 360, 180, MUI_OLIVE);

    /* 故意跨多条带的大块与竖条 */
    mui_rect_fill(0, 200, SIM_W, 60, MUI_YELLOW);
    mui_rect_fill(640, 190, 16, 140, MUI_CYAN);

    /* 位图（含 8bpp 蒙版混色），同样跨带 */
    mui_image_draw(20, 300, IMG_W, IMG_H, s_img_px);
    mui_image_draw_mask(40, 300, IMG_W, IMG_H, s_mask, MUI_RED, TEST_BG);

    /* 容器：超出容器矩形的部分必须被裁掉（与条带裁剪叠加） */
    mui_container_init(&s_box, 800, 200, 200, 120);
    mui_container_set_content(&s_box, 200, 400);
    mui_container_set_scroll(&s_box, 0, 40);
    mui_container_begin(&s_box);
    mui_rect_fill(800, 140, 200, 400, MUI_MAGENTA);
    mui_container_end(&s_box);

    /* 控件 */
    mui_label_draw(&s_lbl);
    mui_button_draw(&s_btn);
    mui_progressbar_draw(&s_pb);
    mui_gauge_draw(&s_gauge);
    mui_slider_draw(&s_sl);
    mui_slider_draw(&s_vsl);

    /* 零散像素：验证逐像素路径也按带平移 */
    for (i = 0; i < 8; i++) {
        mui_pixel_draw((int16_t)(600 + i * 10), 540, MUI_ORANGE);
    }
}

/* ---------------- 工具 ---------------- */

static uint16_t s_ref[FB_PIXELS];
static uint16_t s_cur[FB_PIXELS];

/** @brief 按指定带高渲染一帧，并把"屏幕"内容拷到 out */
static void render_once(int16_t h, uint16_t *out)
{
    mui_strip_set_height(h);
    sim_clear_fb();
    mui_screen_frame(scene_draw, NULL);
    if (out != NULL) {
        memcpy(out, sim_get_fb(), FB_BYTES);
    }
}

/** @brief 两帧差异：返回 1 并输出首个差异像素坐标 */
static int fb_diff(const uint16_t *a, const uint16_t *b, int16_t *dx, int16_t *dy)
{
    int32_t i;

    for (i = 0; i < FB_PIXELS; i++) {
        if (a[i] != b[i]) {
            if (dx != NULL) { *dx = (int16_t)(i % SIM_W); }
            if (dy != NULL) { *dy = (int16_t)(i / SIM_W); }
            return 1;
        }
    }
    return 0;
}

/** @brief 取某像素（越界返回 0xFFFF 便于断言失败） */
static uint16_t at(const uint16_t *fb, int16_t x, int16_t y)
{
    if (x < 0 || y < 0 || x >= SIM_W || y >= SIM_H) {
        return 0xFFFF;
    }
    return fb[(int32_t)y * SIM_W + x];
}

int main(void)
{
    static const int16_t heights[] = { 1, 2, 3, 5, 7, 13, 16, 32 };
    const uint16_t *fb;
    int16_t dx = 0, dy = 0;
    size_t i;
    int bad_heights = 0;

    mui_init(SIM_W, SIM_H);
    init_assets();
    setup_objects();

    /* ---- 0) 前提：条带后端与带高接口生效 ---- */
    CHECK(mui_strip_get_height() > 0, "条带：未运行在条带后端（mui_strip_get_height()==0）");
    mui_strip_set_height((int16_t)MUI_CFG_STRIP_H);
    CHECK(mui_strip_get_height() == (int16_t)MUI_CFG_STRIP_H, "条带：带高被夹到上限失败");
    mui_strip_set_height(9999);
    CHECK(mui_strip_get_height() == (int16_t)MUI_CFG_STRIP_H, "条带：带高未夹到编译期上限");
    mui_strip_set_height(0);
    CHECK(mui_strip_get_height() == 1, "条带：带高下限未夹到 1");

    /* ---- 1) 参照帧（带高 = 编译期上限） ---- */
    render_once((int16_t)MUI_CFG_STRIP_H, s_ref);
    fb = sim_get_fb();

    /* ---- 2) 绝对断言：不依赖参照帧本身 ---- */
    CHECK(at(fb, 20, 25) == MUI_RED,        "条带：矩形内部");
    CHECK(at(fb, 4, 4) == TEST_BG,          "条带：矩形外部");
    CHECK(at(fb, 500, 220) == MUI_YELLOW,   "条带：跨带大块内部");
    CHECK(at(fb, 500, 199) == TEST_BG,      "条带：跨带大块上边界外");
    CHECK(at(fb, 500, 259) == MUI_YELLOW,   "条带：跨带大块下边界内");
    CHECK(at(fb, 500, 260) == TEST_BG,      "条带：跨带大块下边界外");
    CHECK(at(fb, 647, 250) == MUI_CYAN,     "条带：跨带竖条内部");
    CHECK(at(fb, 20, 305) == s_img_px[5 * IMG_W], "条带：位图第 0 列");
    CHECK(at(fb, 21, 305) == s_img_px[5 * IMG_W + 1], "条带：位图第 1 列（防列偏移）");
    CHECK(at(fb, 22, 305) == s_img_px[5 * IMG_W + 2], "条带：位图第 2 列（防列偏移）");
    CHECK(at(fb, 900, 250) == MUI_MAGENTA,  "条带：容器内(滚动后)应可见");
    CHECK(at(fb, 900, 190) == TEST_BG,      "条带：容器上方应被裁掉");
    CHECK(at(fb, 600, 540) == MUI_ORANGE,   "条带：零散像素");
    CHECK(at(fb, 610, 540) == MUI_ORANGE,   "条带：零散像素 2");
    CHECK(at(fb, 620, 539) == TEST_BG,      "条带：零散像素之间");

    /* ---- 3) 带高无关性：与参照帧逐像素一致 ---- */
    for (i = 0; i < sizeof(heights) / sizeof(heights[0]); i++) {
        if (heights[i] > (int16_t)MUI_CFG_STRIP_H) {
            continue;
        }
        render_once(heights[i], s_cur);
        if (fb_diff(s_ref, s_cur, &dx, &dy)) {
            printf("[FAIL] 条带：带高 %d 与 %d 结果不一致，首个差异 (%d,%d)\n",
                   (int)heights[i], (int)MUI_CFG_STRIP_H, (int)dx, (int)dy);
            test_failed++;
            bad_heights++;
        }
    }
    CHECK(bad_heights == 0, "条带：带高影响了渲染结果");

    /* ---- 4) 重放幂等：同一带高连画两帧结果必须相同 ---- */
    render_once(7, s_cur);
    mui_strip_set_height(7);
    sim_clear_fb();
    mui_screen_frame(scene_draw, NULL);
    CHECK(!fb_diff(s_cur, sim_get_fb(), &dx, &dy),
          "条带：同一帧重放两次结果不一致（首帧有残留状态）");

    /* ---- 5) 帧外 set_* 不得写屏；下一帧必须体现新状态 ---- */
    render_once(16, s_cur);
    mui_progressbar_set_value(&s_pb, 90);
    mui_label_set_text(&s_lbl, "3");
    mui_gauge_set_value(&s_gauge, 30);
    mui_slider_set_value(&s_sl, 80);
    mui_button_set_pressed(&s_btn, 1);
    CHECK(!fb_diff(s_cur, sim_get_fb(), &dx, &dy),
          "条带：帧外调用 set_* 写了屏（应只改状态）");

    mui_screen_frame(scene_draw, NULL);
    CHECK(fb_diff(s_cur, sim_get_fb(), &dx, &dy),
          "条带：下一帧未体现新状态");

    /* ---- 6) 全程零越界违规 ---- */
    CHECK(sim_violations() == 0, "条带：出现移植层越界违规（坐标平移有误）");

    if (test_failed == 0 && sim_violations() == 0) {
        printf("条带后端测试全部通过（带高 1~%d 结果逐像素一致）\n",
               (int)MUI_CFG_STRIP_H);
        return 0;
    }
    printf("条带后端测试失败数：%d（底层违规：%d）\n", test_failed, sim_violations());
    return 1;
}
