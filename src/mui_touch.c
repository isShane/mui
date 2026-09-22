/**
 * @file mui_touch.c
 * @brief MUI 触摸输入：状态机 + 事件队列（裸机友好，零动态内存）
 */

#include "mui.h"

#define TOUCH_QSIZE     4    /**< 事件队列深度 */
#define CLICK_TOL       8    /**< 点击判定：按下/抬起最大位移（像素） */
#define MOVE_TOL        2    /**< 移动事件触发阈值（像素） */

static mui_touch_event_t s_queue[TOUCH_QSIZE];  /**< 事件环形队列 */
static uint8_t s_qhead = 0;                     /**< 队列头 */
static uint8_t s_qcount = 0;                    /**< 队列计数 */

static uint8_t s_pressed = 0;                   /**< 当前按住状态 */
static int16_t s_x = 0, s_y = 0;                /**< 最新触点坐标 */
static int16_t s_down_x = 0, s_down_y = 0;      /**< 按下时刻坐标 */

/**
 * @brief 事件入队（满则丢弃最旧，保最新）
 * @param ev 事件类型
 */
static void queue_push(mui_touch_event_t ev)
{
    if (s_qcount >= TOUCH_QSIZE) {
        s_qhead = (uint8_t)((s_qhead + 1) % TOUCH_QSIZE);
        s_qcount--;
    }
    s_queue[(uint8_t)((s_qhead + s_qcount) % TOUCH_QSIZE)] = ev;
    s_qcount++;
}

void mui_touch_update(int16_t x, int16_t y, uint8_t pressed)
{
    uint8_t prev = s_pressed;
    int16_t dx, dy;

    s_x = x;
    s_y = y;
    s_pressed = pressed ? 1 : 0;

    if (!prev && s_pressed) {
        /* 按下 */
        s_down_x = x;
        s_down_y = y;
        queue_push(MUI_TOUCH_DOWN);
    } else if (prev && s_pressed) {
        /* 按住移动 */
        dx = (int16_t)(x - s_down_x);
        dy = (int16_t)(y - s_down_y);
        if (dx > MOVE_TOL || dx < -MOVE_TOL ||
            dy > MOVE_TOL || dy < -MOVE_TOL) {
            queue_push(MUI_TOUCH_MOVE);
        }
    } else if (prev && !s_pressed) {
        /* 抬起：位移小为点击，大为拖动结束 */
        dx = (int16_t)(x - s_down_x);
        dy = (int16_t)(y - s_down_y);
        if (dx <= CLICK_TOL && dx >= -CLICK_TOL &&
            dy <= CLICK_TOL && dy >= -CLICK_TOL) {
            queue_push(MUI_TOUCH_CLICK);
        } else {
            queue_push(MUI_TOUCH_UP);
        }
    }
    /* 无变化（未按下时的坐标刷新）不产生事件 */
}

mui_touch_event_t mui_touch_poll(void)
{
    mui_touch_event_t ev;

    if (s_qcount == 0) {
        return MUI_TOUCH_NONE;
    }
    ev = s_queue[s_qhead];
    s_qhead = (uint8_t)((s_qhead + 1) % TOUCH_QSIZE);
    s_qcount--;
    return ev;
}

void mui_touch_get_xy(int16_t *x, int16_t *y)
{
    if (x != NULL) {
        *x = s_x;
    }
    if (y != NULL) {
        *y = s_y;
    }
}

uint8_t mui_rect_contains(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t px, int16_t py)
{
    if (w <= 0 || h <= 0) {
        return 0;
    }
    return (uint8_t)(px >= x && px < (int16_t)(x + w) &&
                     py >= y && py < (int16_t)(y + h));
}
