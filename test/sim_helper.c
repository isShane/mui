/**
 * @file sim_helper.c
 * @brief 测试辅助实现
 */

#include <stdio.h>
#include "sim_helper.h"
#include "png_export.h"

static uint16_t sim_fb[SIM_W * SIM_H];  /**< 模拟帧缓冲 */
static int sim_violation_count = 0;     /**< 越界写入累计计数 */

uint16_t *sim_get_fb(void)
{
    return sim_fb;
}

int sim_violations(void)
{
    return sim_violation_count;
}

void sim_clear_violations(void)
{
    sim_violation_count = 0;
}

void sim_record_violation(void)
{
    sim_violation_count++;
}

void sim_clear_fb(void)
{
    for (int i = 0; i < SIM_W * SIM_H; i++) {
        sim_fb[i] = 0x0000;
    }
}

void sim_export_png(const char *path)
{
    if (png_export_rgb565(path, sim_fb, SIM_W, SIM_H) != 0) {
        printf("PNG 导出失败：%s\n", path);
    }
}
