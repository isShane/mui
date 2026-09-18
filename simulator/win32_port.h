/**
 * @file win32_port.h
 * @brief Win32 模拟器移植层对外接口（帧缓冲访问）
 */

#ifndef WIN32_PORT_H
#define WIN32_PORT_H

#include <stdint.h>

/**
 * @brief 获取 Win32 模拟帧缓冲
 * @param w 输出：缓冲宽度
 * @param h 输出：缓冲高度
 * @return 帧缓冲首地址（RGB565）
 */
uint16_t *win32_get_fb(int *w, int *h);

#endif /* WIN32_PORT_H */
