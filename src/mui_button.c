/**
 * @file mui_button.c
 * @brief MUI 按钮控件实现（保留模式对象）
 */

#include "mui_button.h"
#include "mui_font.h"

/* -------- 默认配色 -------- */

const mui_button_style_t mui_button_style_default = {
    MUI_RGB565(0x3F, 0x77, 0xB8),   /* bg：参数框蓝 */
    MUI_RGB565(0x2A, 0x52, 0x80),   /* bg_press：深蓝 */
    MUI_WHITE,                      /* fg：白字 */
    MUI_BLACK,                      /* border：黑边 */
    MUI_BUTTON_SHAPE_ROUND,         /* shape：圆角 */
    0,                              /* radius：自动（h/4 限 8） */
    1,                              /* aa：默认开圆角抗锯齿 */
    MUI_WHITE                       /* screen_bg：默认白底 */
};

/**
 * @brief 按 style 的形状配置计算底板圆角半径
 */
static int16_t button_radius(int16_t h, const mui_button_style_t *s)
{
    int16_t r;

    switch (s->shape) {
    case MUI_BUTTON_SHAPE_RECT:
        return 0;
    case MUI_BUTTON_SHAPE_PILL:
        return (int16_t)(h / 2);
    default:    /* MUI_BUTTON_SHAPE_ROUND */
        r = s->radius > 0 ? s->radius : (int16_t)(h / 4);
        if (s->radius <= 0 && r > 8) {
            r = 8;
        }
        return r;
    }
}

/** @brief 该样式是否真的启用抗锯齿（叠上全局总开关 MUI_CFG_AA） */
static uint8_t button_aa(const mui_button_style_t *s)
{
#if MUI_CFG_AA
    /* screen_bg 未设置(0)时无法确定混色底色 → 自动退化硬边，避免出现黑边/黑环 */
    return (uint8_t)((s->aa && s->screen_bg != 0) ? 1 : 0);
#else
    (void)s;
    return 0;
#endif
}

/**
 * @brief 画"填充 + 1px 边"的圆角底板
 *
 * 抗锯齿时不单独描边：描边会把边线按页面底色混色，压在填充上形成一圈暗环
 * （尤其是胶囊这种全曲线外形）。改为——
 *   先用边框色铺满整块（AA 到页面底色），再用填充色画内缩 1px 的内块（AA 到边框色）。
 * 这样内外两条边各自混到正确底色，得到干净的描边。
 */
static void button_panel(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                         uint16_t bg, uint16_t border, uint16_t screen_bg,
                         uint8_t aa)
{
    if (!aa) {
        mui_round_rect_fill(x, y, w, h, r, bg);
        mui_round_rect_draw(x, y, w, h, r, border);
        return;
    }
    if (border == bg || w < 3 || h < 3) {       /* 无可见边：整块填充即可 */
        mui_round_rect_fill_aa(x, y, w, h, r, bg, screen_bg);
        return;
    }
    mui_round_rect_fill_aa(x, y, w, h, r, border, screen_bg);
    mui_round_rect_fill_aa((int16_t)(x + 1), (int16_t)(y + 1),
                           (int16_t)(w - 2), (int16_t)(h - 2),
                           (int16_t)(r > 1 ? r - 1 : 0), bg, border);
}

/**
 * @brief 绘制按钮底板（按 style 形状，可选圆角抗锯齿）
 */
static void button_base(int16_t x, int16_t y, int16_t w, int16_t h,
                        const mui_button_style_t *s, uint16_t bg)
{
    button_panel(x, y, w, h, button_radius(h, s), bg, s->border,
                 s->screen_bg, button_aa(s));
}

/* -------- OO 按钮：对象接口 -------- */

void mui_button_init(mui_button_t *btn, int16_t x, int16_t y, int16_t w, int16_t h,
                     const mui_button_style_t *style, const char *text,
                     const mui_image_mask_t *icon)
{
    if (btn == NULL) {
        return;
    }
    btn->x = x;
    btn->y = y;
    btn->w = w;
    btn->h = h;
    btn->style = style;       /* NULL 在 draw 时 fallback 默认 */
    btn->text = text;
    btn->icon = icon;
    btn->icon_img = NULL;
    btn->bg_img = NULL;       /* 默认用纯色圆角底板 */
    btn->bg_img_press = NULL;
    btn->bg_key = 0;
    btn->icon_key = 0;
    btn->bg_l = btn->bg_t = btn->bg_r = btn->bg_b = 0;
    btn->bg_nine = 0;
    btn->bg_key_on = 0;
    btn->icon_key_on = 0;
    btn->font = NULL;         /* 默认不画文字 */
    btn->pressed = 0;
    btn->enabled = 1;
    btn->visible = 1;
    btn->latch = 0;   /* 默认瞬态 */

    /* 绘制缓存：首次 draw 必画（快照清零，与任何有效状态都不相等） */
    btn->cache_valid = 0;
    btn->c_pressed = 0;
    btn->c_enabled = 0;
    btn->c_visible = 0;
    btn->c_x = 0;
    btn->c_y = 0;
    btn->c_text = NULL;
    btn->c_icon = NULL;
    btn->c_font = NULL;
}

/* 所有 set_* 都在"状态真正变化"时作废缓存，使每帧无脑 draw 只画变化的那几个 */

void mui_button_set_pressed(mui_button_t *btn, uint8_t pressed)
{
    uint8_t v = (uint8_t)(pressed ? 1 : 0);
    if (btn != NULL && btn->pressed != v) {
        btn->pressed = v;
        btn->cache_valid = 0;
    }
}

void mui_button_set_enabled(mui_button_t *btn, uint8_t enabled)
{
    uint8_t v = (uint8_t)(enabled ? 1 : 0);
    if (btn != NULL && btn->enabled != v) {
        btn->enabled = v;
        btn->cache_valid = 0;
    }
}

void mui_button_set_visible(mui_button_t *btn, uint8_t visible)
{
    uint8_t v = (uint8_t)(visible ? 1 : 0);
    if (btn != NULL && btn->visible != v) {
        btn->visible = v;
        btn->cache_valid = 0;
    }
}

void mui_button_set_text(mui_button_t *btn, const char *text)
{
    if (btn != NULL && btn->text != text) {
        btn->text = text;
        btn->cache_valid = 0;
    }
}

void mui_button_set_icon(mui_button_t *btn, const mui_image_mask_t *icon)
{
    if (btn != NULL && btn->icon != icon) {
        btn->icon = icon;
        btn->cache_valid = 0;
    }
}

void mui_button_set_bg_image(mui_button_t *btn, const mui_image_t *img)
{
    if (btn != NULL && btn->bg_img != img) {
        btn->bg_img = img;
        btn->cache_valid = 0;
    }
}

void mui_button_set_bg_image_press(mui_button_t *btn, const mui_image_t *img)
{
    if (btn != NULL && btn->bg_img_press != img) {
        btn->bg_img_press = img;
        btn->cache_valid = 0;
    }
}

void mui_button_set_bg_image_key(mui_button_t *btn, const mui_image_t *img,
                                 uint16_t key)
{
    if (btn != NULL) {
        btn->bg_img = img;
        btn->bg_key = key;
        btn->bg_key_on = (uint8_t)(img != NULL ? 1 : 0);
        btn->cache_valid = 0;
    }
}

void mui_button_set_bg_nine(mui_button_t *btn,
                            int16_t l, int16_t t, int16_t r, int16_t b)
{
    if (btn != NULL) {
        btn->bg_l = l; btn->bg_t = t; btn->bg_r = r; btn->bg_b = b;
        btn->bg_nine = 1;
        btn->cache_valid = 0;
    }
}

void mui_button_set_icon_image(mui_button_t *btn, const mui_image_t *img)
{
    if (btn != NULL) {
        btn->icon_img = img;
        btn->icon_key_on = 0;
        btn->cache_valid = 0;
    }
}

void mui_button_set_icon_image_key(mui_button_t *btn, const mui_image_t *img,
                                   uint16_t key)
{
    if (btn != NULL) {
        btn->icon_img = img;
        btn->icon_key = key;
        btn->icon_key_on = (uint8_t)(img != NULL ? 1 : 0);
        btn->cache_valid = 0;
    }
}

void mui_button_set_font(mui_button_t *btn, const mui_font_t *font)
{
    if (btn != NULL && btn->font != font) {
        btn->font = font;
        btn->cache_valid = 0;
    }
}

void mui_button_set_style(mui_button_t *btn, const mui_button_style_t *style)
{
    if (btn != NULL && btn->style != style) {
        btn->style = style;   /* NULL 在 draw 时 fallback 默认 */
        btn->cache_valid = 0;
    }
}

void mui_button_set_pos(mui_button_t *btn, int16_t x, int16_t y)
{
    if (btn != NULL && (btn->x != x || btn->y != y)) {
        btn->x = x;
        btn->y = y;
        btn->cache_valid = 0;
    }
}

void mui_button_set_latch(mui_button_t *btn, uint8_t latch)
{
    if (btn != NULL) {
        btn->latch = (uint8_t)(latch ? 1 : 0);   /* 只影响交互，不影响外观 */
    }
}

void mui_button_invalidate(mui_button_t *btn)
{
    if (btn != NULL) {
        btn->cache_valid = 0;
    }
}

/**
 * @brief 绘制禁用遮罩（覆盖底板与内容，视觉为半透明灰色）
 */
static void button_draw_disabled_overlay(const mui_button_t *btn,
                                         const mui_button_style_t *s)
{
    uint16_t gray = MUI_RGB565(0xBB, 0xBB, 0xBB);   /* 中灰 */
    uint16_t dim_fg = MUI_RGB565(0x77, 0x77, 0x77); /* 前景变深灰 */

    /* 灰色底板直接覆盖（简化：禁用态不叠底，直接画灰底+深灰边） */
    button_panel(btn->x, btn->y, btn->w, btn->h, button_radius(btn->h, s),
                 gray, dim_fg, s->screen_bg, button_aa(s));
}

/** @brief 内部：把当前状态写入绘制缓存快照 */
static void button_cache_store(mui_button_t *btn)
{
    btn->c_pressed = btn->pressed;
    btn->c_enabled = btn->enabled;
    btn->c_visible = btn->visible;
    btn->c_x = btn->x;
    btn->c_y = btn->y;
    btn->c_text = btn->text;
    btn->c_icon = btn->icon;
    btn->c_font = (const void *)btn->font;
    btn->cache_valid = 1;
}

void mui_button_draw(mui_button_t *btn)
{
    const mui_button_style_t *s;
    uint16_t bg;
    int16_t iw;
    int16_t tw;
    int16_t gap;
    int16_t total;
    int16_t sx;
    int16_t cy;

    if (btn == NULL || !btn->visible) {
        return;
    }

    /* 状态与上次绘制完全一致 → 直接返回，不写屏（按需重绘）。
     * 条带后端下每帧按带重放绘制，屏上旧内容不复存在，缓存失去意义 → 一律全量重绘。 */
#if !MUI_CFG_IS_STRIP
    if (btn->cache_valid
        && btn->c_pressed == btn->pressed
        && btn->c_enabled == btn->enabled
        && btn->c_visible == btn->visible
        && btn->c_x == btn->x
        && btn->c_y == btn->y
        && btn->c_text == btn->text
        && btn->c_icon == btn->icon
        && btn->c_font == (const void *)btn->font) {
        return;
    }
#endif

    s = btn->style ? btn->style : &mui_button_style_default;
    bg = btn->pressed ? s->bg_press : s->bg;

    /* 底板：自定义贴图优先，其次纯色圆角底板 */
    {
        const mui_image_t *face = btn->bg_img;

        if (face != NULL && btn->pressed && btn->bg_img_press != NULL) {
            face = btn->bg_img_press;
        }
        if (face != NULL) {
            if (btn->bg_nine) {
                if (btn->bg_key_on) {
                    mui_image_draw_nine_key(btn->x, btn->y, btn->w, btn->h,
                                            face->w, face->h, face->data,
                                            btn->bg_l, btn->bg_t, btn->bg_r,
                                            btn->bg_b, btn->bg_key);
                } else {
                    mui_image_draw_nine(btn->x, btn->y, btn->w, btn->h,
                                        face->w, face->h, face->data,
                                        btn->bg_l, btn->bg_t, btn->bg_r,
                                        btn->bg_b);
                }
            } else if (btn->bg_key_on) {
                mui_image_draw_key(btn->x, btn->y, face->w, face->h,
                                   face->data, btn->bg_key);
            } else {
                mui_image_draw(btn->x, btn->y, face->w, face->h, face->data);
            }
        } else {
            button_base(btn->x, btn->y, btn->w, btn->h, s, bg);
        }
    }

    /* 禁用遮罩（覆盖底板，不再画内容） */
    if (!btn->enabled) {
        button_draw_disabled_overlay(btn, s);
        button_cache_store(btn);
        return;
    }

    /* 文字像素宽（无字体则不画文字） */
    tw = (btn->text != NULL && btn->text[0] != '\0' && btn->font != NULL)
         ? mui_text_width(btn->text, btn->font, 1) : 0;

    /* 计算图标 + 文字组合尺寸（内容始终居中，按下仅变底色） */
    iw = btn->icon_img ? btn->icon_img->w : (btn->icon ? btn->icon->w : 0);
    gap = (iw > 0 && tw > 0) ? 4 : 0;
    total = (int16_t)(iw + gap + tw);
    sx = (int16_t)(btn->x + (btn->w - total) / 2);
    cy = (int16_t)(btn->y + btn->h / 2);

    /* 图标：全彩贴图优先，其次 alpha 蒙版 */
    if (btn->icon_img != NULL) {
        int16_t ix = sx;
        int16_t iy = (int16_t)(cy - btn->icon_img->h / 2);

        if (btn->icon_key_on) {
            mui_image_draw_key(ix, iy, btn->icon_img->w, btn->icon_img->h,
                               btn->icon_img->data, btn->icon_key);
        } else {
            mui_image_draw(ix, iy, btn->icon_img->w, btn->icon_img->h,
                           btn->icon_img->data);
        }
    } else if (btn->icon != NULL) {
        mui_image_draw_mask(sx, (int16_t)(cy - btn->icon->h / 2),
                              btn->icon->w, btn->icon->h,
                              btn->icon->data, s->fg, bg);
    }

    /* 文字：行框垂直居中（要求字体 line_height ≤ 按钮高） */
    if (tw > 0) {
        int16_t tx = (int16_t)(sx + iw + gap);
        int16_t ty = (int16_t)(btn->y + (btn->h - btn->font->line_height) / 2);

        mui_text_draw(tx, ty, btn->text, btn->font, s->fg, bg, 1);
    }

    button_cache_store(btn);
}

uint8_t mui_button_touch(mui_button_t *btn, mui_touch_event_t ev)
{
    int16_t x, y;
    uint8_t hit;

    if (btn == NULL || ev == MUI_TOUCH_NONE || !btn->enabled) {
        return 0;
    }

    mui_touch_get_xy(&x, &y);
    hit = mui_rect_contains(btn->x, btn->y, btn->w, btn->h, x, y);

    switch (ev) {
    case MUI_TOUCH_DOWN:
        /* 瞬态：命中即高亮反馈；锁存不在此改状态（切换只在 CLICK 完成） */
        if (hit && !btn->latch) {
            mui_button_set_pressed(btn, 1);
        }
        break;

    case MUI_TOUCH_CLICK:
        if (hit) {
            if (btn->latch) {
                /* 锁存（开关）：点击切换最终状态并保持 */
                mui_button_set_pressed(btn, btn->pressed ? 0 : 1);
            } else {
                /* 瞬态：点击后弹起 */
                mui_button_set_pressed(btn, 0);
            }
            return 1;
        }
        break;

    case MUI_TOUCH_UP:
        /* 瞬态撤销 DOWN 的临时高亮；锁存状态由 CLICK 持久管理，UP 不动它 */
        if (!btn->latch && btn->pressed) {
            mui_button_set_pressed(btn, 0);
        }
        break;

    default:
        break;
    }
    return 0;
}
