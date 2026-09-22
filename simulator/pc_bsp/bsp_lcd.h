/**
 * @file bsp_lcd.h
 * @brief 单片机 BSP 的 PC 替身：LCD 尺寸
 *
 * 单片机工程的 app/app_ui.c 原样复用到模拟器时，硬件差异全部由本目录吸收。
 * 目录内只放头文件（无 .c），两个替身各对应一个硬件符号：
 *   bsp_lcd.h    屏幕尺寸     → 取自模拟器配置 sim_config.h
 *   bsp_system.h 系统毫秒计数 → 由标准库 clock() 换算
 */

#ifndef SIM_PC_BSP_LCD_H
#define SIM_PC_BSP_LCD_H

#include "sim_config.h"

/** @brief LCD 宽度（像素）：模拟器取 SIM_W */
#define BSP_LCD_WIDTH   SIM_W
/** @brief LCD 高度（像素）：模拟器取 SIM_H */
#define BSP_LCD_HEIGHT  SIM_H

#endif /* SIM_PC_BSP_LCD_H */