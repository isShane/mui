/**
 * @file mui_port_win32.c
 * @brief Win32 模拟器移植层：像素写入内存帧缓冲（尺寸跟随 mui_init）
 */

#include <stdlib.h>
#include <string.h>
#include "mui.h"
#include "win32_port.h"

static uint16_t *win32_fb = NULL;  /**< 模拟帧缓冲 */
static int win32_w = 0;            /**< 帧宽 */
static int win32_h = 0;            /**< 帧高 */

uint16_t *win32_get_fb(int *w, int *h)
{
    *w = win32_w;
    *h = win32_h;
    return win32_fb;
}

void mui_port_init(void)
{
    win32_w = mui_get_width();
    win32_h = mui_get_height();
    free(win32_fb);
    win32_fb = (uint16_t *)malloc((size_t)win32_w * win32_h * sizeof(uint16_t));
    if (win32_fb != NULL) {
        memset(win32_fb, 0, (size_t)win32_w * win32_h * sizeof(uint16_t));
    }
}

void mui_port_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (win32_fb == NULL || x < 0 || x >= win32_w || y < 0 || y >= win32_h) {
        return;
    }
    win32_fb[y * win32_w + x] = color;
}

void mui_port_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t color)
{
    int16_t i;
    int16_t j;

    if (win32_fb == NULL || w < 1 || h < 1 || x < 0 || y < 0
        || x + w > win32_w || y + h > win32_h) {
        return;
    }
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            win32_fb[(y + j) * win32_w + (x + i)] = color;
        }
    }
}

void mui_port_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                          const uint16_t *data, int16_t stride)
{
    int16_t j;

    if (win32_fb == NULL || w < 1 || h < 1 || x < 0 || y < 0
        || x + w > win32_w || y + h > win32_h || data == NULL) {
        return;
    }
    for (j = 0; j < h; j++) {
        memcpy(&win32_fb[(y + j) * win32_w + x],
               data + (size_t)j * stride,
               (size_t)w * sizeof(uint16_t));
    }
}
