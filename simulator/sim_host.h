/**
 * @file sim_host.h
 * @brief 模拟器宿主接口：以回调方式承载任意界面层
 *
 * 宿主（sim_win32.c）不感知任何具体界面，界面通过本接口注入，
 * 两者的唯一连接点是装配文件 sim_app_bind.c。
 */

#ifndef SIM_HOST_H
#define SIM_HOST_H

#include <stdint.h>

/** @brief 界面层回调集合 */
typedef struct {
    void (*frame)(void);                        /**< 每帧绘制回调（不可为 NULL） */
    void (*key)(uint8_t key_id, uint8_t down);  /**< 物理按键回调（可为 NULL） */
} sim_app_ops_t;

/**
 * @brief 启动模拟器窗口与消息循环（阻塞至窗口关闭）
 * @param ops 界面回调集合
 * @return 进程退出码
 */
int sim_host_run(const sim_app_ops_t *ops);

#endif /* SIM_HOST_H */