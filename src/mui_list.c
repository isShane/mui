/**
 * @file mui_list.c
 * @brief MUI 可滚动列表实现
 */

#include "mui_list.h"

/** @brief 行节距（行高 + 行间距） */
#define LIST_PITCH(l)   ((int16_t)((l)->row_h + (l)->gap))

/** @brief 内部：视口矩形 */
static mui_rect_t list_view(const mui_list_t *l)
{
    mui_rect_t r;

    r.x = l->box.x;
    r.y = l->box.y;
    r.x2 = (int16_t)(l->box.x + l->box.w);
    r.y2 = (int16_t)(l->box.y + l->box.h);
    return r;
}

/** @brief 内部：内容总高（rows 行 + 行间距） */
static int16_t list_content_h(const mui_list_t *l)
{
    if (l->rows == 0) {
        return 0;
    }
    return (int16_t)((int32_t)l->rows * LIST_PITCH(l) - l->gap);
}

/** @brief 内部：把内容高度同步给容器（用于钳制滚动） */
static void list_sync_content(mui_list_t *l)
{
    mui_container_set_content(&l->box, 0, list_content_h(l));
}

void mui_list_init(mui_list_t *l, int16_t x, int16_t y, int16_t w, int16_t h,
                   int16_t row_h, int16_t gap)
{
    if (l == NULL) {
        return;
    }
    mui_container_init(&l->box, x, y, w, h);
    l->row_h = (row_h > 0) ? row_h : 1;
    l->gap = (gap < 0) ? 0 : gap;
    l->rows = 0;
    l->sel = MUI_LIST_SEL_NONE;
    l->dragging = 0;
    l->moved = 0;
    l->drag_y = 0;
    l->drag_scroll = 0;
}

void mui_list_set_bounds(mui_list_t *l, int16_t x, int16_t y, int16_t w, int16_t h)
{
    if (l == NULL) {
        return;
    }
    l->box.x = x;
    l->box.y = y;
    l->box.w = w;
    l->box.h = h;
    list_sync_content(l);
}

void mui_list_set_rows(mui_list_t *l, uint8_t rows)
{
    if (l == NULL) {
        return;
    }
    l->rows = rows;
    if (l->sel != MUI_LIST_SEL_NONE && l->sel >= rows) {
        l->sel = MUI_LIST_SEL_NONE;
    }
    list_sync_content(l);
}

uint8_t mui_list_get_rows(const mui_list_t *l)
{
    return (l != NULL) ? l->rows : 0;
}

void mui_list_set_sel(mui_list_t *l, uint8_t sel)
{
    if (l == NULL) {
        return;
    }
    if (sel != MUI_LIST_SEL_NONE && sel >= l->rows) {
        sel = (uint8_t)(l->rows > 0 ? (uint8_t)(l->rows - 1) : MUI_LIST_SEL_NONE);
    }
    l->sel = sel;
}

uint8_t mui_list_get_sel(const mui_list_t *l)
{
    return (l != NULL) ? l->sel : MUI_LIST_SEL_NONE;
}

int16_t mui_list_max_scroll(const mui_list_t *l)
{
    return (l != NULL) ? mui_container_scroll_max_y(&l->box) : 0;
}

void mui_list_set_scroll(mui_list_t *l, int16_t scroll)
{
    if (l == NULL) {
        return;
    }
    mui_container_set_scroll(&l->box, 0, scroll);
}

void mui_list_scroll_by(mui_list_t *l, int16_t dy)
{
    if (l == NULL) {
        return;
    }
    mui_list_set_scroll(l, (int16_t)(l->box.sy + dy));
}

int16_t mui_list_get_scroll(const mui_list_t *l)
{
    return (l != NULL) ? l->box.sy : 0;
}

uint8_t mui_list_visible(const mui_list_t *l, uint8_t *first, uint8_t *count)
{
    int16_t pitch;
    int32_t f, last, n;

    if (first != NULL) { *first = 0; }
    if (count != NULL) { *count = 0; }
    if (l == NULL || l->rows == 0 || l->box.h < 1) {
        return 0;
    }
    pitch = LIST_PITCH(l);
    f = (int32_t)l->box.sy / pitch;
    last = ((int32_t)l->box.sy + l->box.h - 1) / pitch;
    if (last > (int32_t)l->rows - 1) {
        last = (int32_t)l->rows - 1;
    }
    if (f > last) {
        return 0;
    }
    n = last - f + 1;
    if (first != NULL) { *first = (uint8_t)f; }
    if (count != NULL) { *count = (uint8_t)n; }
    return (uint8_t)n;
}

mui_rect_t mui_list_row_rect(const mui_list_t *l, uint8_t index)
{
    mui_rect_t r;
    int32_t y;

    r.x = 0; r.y = 0; r.x2 = 0; r.y2 = 0;
    if (l == NULL || index >= l->rows) {
        return r;
    }
    y = (int32_t)mui_container_oy(&l->box) + (int32_t)index * LIST_PITCH(l);
    r.x = l->box.x;
    r.y = (int16_t)y;
    r.x2 = (int16_t)(l->box.x + l->box.w);
    r.y2 = (int16_t)(y + l->row_h);
    return r;
}

uint8_t mui_list_row_at(const mui_list_t *l, int16_t px, int16_t py, uint8_t *index)
{
    int16_t pitch;
    int32_t y, idx, off;

    if (l == NULL || l->rows == 0) {
        return 0;
    }
    if (!mui_rect_hit(list_view(l), px, py)) {
        return 0;
    }
    y = (int32_t)py - l->box.y + l->box.sy;      /* 内容坐标 */
    if (y < 0) {
        return 0;
    }
    pitch = LIST_PITCH(l);
    idx = y / pitch;
    off = y - idx * pitch;
    if (idx >= (int32_t)l->rows || off >= l->row_h) {
        return 0;                                 /* 超出范围 / 落在行间距上 */
    }
    if (index != NULL) {
        *index = (uint8_t)idx;
    }
    return 1;
}

uint8_t mui_list_touch(mui_list_t *l, mui_touch_event_t ev)
{
    uint8_t chg = 0;
    int16_t x, y;

    if (l == NULL || ev == MUI_TOUCH_NONE) {
        return 0;
    }
    mui_touch_get_xy(&x, &y);

    switch (ev) {
    case MUI_TOUCH_DOWN:
        if (mui_rect_hit(list_view(l), x, y)) {
            l->dragging = 1;
            l->moved = 0;
            l->drag_y = y;
            l->drag_scroll = l->box.sy;
        }
        break;

    case MUI_TOUCH_MOVE:
        if (l->dragging) {
            int16_t dy = (int16_t)(l->drag_y - y);   /* 手指上滑 → 内容上移 */
            if (dy > 4 || dy < -4) {
                l->moved = 1;
            }
            if (l->moved) {
                int16_t old = l->box.sy;
                mui_list_set_scroll(l, (int16_t)(l->drag_scroll + dy));
                if (l->box.sy != old) {
                    chg |= MUI_LIST_CHANGED_SCROLL;
                }
            }
        }
        break;

    case MUI_TOUCH_CLICK:                        /* 未发生拖动 → 视为点按选中 */
        if (l->dragging && !l->moved) {
            uint8_t idx;
            if (mui_list_row_at(l, x, y, &idx) && l->sel != idx) {
                l->sel = idx;
                chg |= MUI_LIST_CHANGED_SEL;
            }
        }
        l->dragging = 0;
        break;

    case MUI_TOUCH_UP:
        l->dragging = 0;
        break;

    default:
        break;
    }
    return chg;
}

void mui_list_begin(mui_list_t *l)
{
    if (l != NULL) {
        mui_container_begin(&l->box);
    }
}

void mui_list_end(mui_list_t *l)
{
    if (l != NULL) {
        mui_container_end(&l->box);
    }
}

void mui_list_draw_frame(const mui_list_t *l, uint16_t bg, uint16_t border)
{
    if (l == NULL) {
        return;
    }
    mui_rect_fill(l->box.x, l->box.y, l->box.w, l->box.h, bg);
    if (border != bg) {
        mui_rect_draw(l->box.x, l->box.y, l->box.w, l->box.h, border);
    }
}
