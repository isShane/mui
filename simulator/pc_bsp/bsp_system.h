/**
 * @file bsp_system.h
 * @brief 单片机 BSP 的 PC 替身：系统毫秒计数
 */

#ifndef SIM_PC_BSP_SYSTEM_H
#define SIM_PC_BSP_SYSTEM_H

#include <stdint.h>
#include <time.h>

#if (CLOCKS_PER_SEC < 1000)
#error "本替身假定 CLOCKS_PER_SEC >= 1000"
#endif

/**
 * @brief 获取系统毫秒计数
 * @return 自进程启动以来的毫秒数
 * @note  MinGW 下 clock() 即墙上时间，粒度约 15ms，足以驱动界面动画
 */
static inline uint32_t bsp_system_tick_ms(void)
{
    return (uint32_t)(clock() / (CLOCKS_PER_SEC / 1000));
}

#endif /* SIM_PC_BSP_SYSTEM_H */