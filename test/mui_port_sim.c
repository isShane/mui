/**
 * @file mui_port_sim.c
 * @brief PC 模拟移植层（仅用于测试：像素写入内存帧，带越界检查）
 */

#include "mui_port.h"
#include "sim_helper.h"

void mui_port_init(void)
{
    sim_clear_fb();
}

void mui_port_draw_pixel(int16_t x, int16_t y, uint16_t color)
{
    if (x < 0 || x >= SIM_W || y < 0 || y >= SIM_H) {
        sim_record_violation();
        return;
    }
    sim_get_fb()[y * SIM_W + x] = color;
}

void mui_port_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t color)
{
    if (w < 1 || h < 1 || x < 0 || y < 0 || x + w > SIM_W || y + h > SIM_H) {
        sim_record_violation();
        return;
    }
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            sim_get_fb()[(y + j) * SIM_W + (x + i)] = color;
        }
    }
}

void mui_port_draw_bitmap(int16_t x, int16_t y, int16_t w, int16_t h,
                          const uint16_t *data, int16_t stride)
{
    if (w < 1 || h < 1 || x < 0 || y < 0 || x + w > SIM_W || y + h > SIM_H
        || data == NULL) {
        sim_record_violation();
        return;
    }
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < w; i++) {
            sim_get_fb()[(y + j) * SIM_W + (x + i)] = data[j * stride + i];
        }
    }
}
