/**
 * @file    demo.c
 * @brief   timingTask 使用示例
 *
 * @details 演示三种 count 语义和两种 time 参数。
 *
 * @author  timingTask
 * @version 3.0.0
 * @date    2026
 *
 * @copyright MIT License
 */

#include "timing_task.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*===========================================================================
 * 回调
 *===========================================================================*/

/** @brief 温度模拟上报（永久循环） */
static void report_temperature(void *arg)
{
    static double temp = 25.0;

    temp += 0.5;
    if (temp > 40.0)
    {
        temp = 20.0;
    }

    printf("  [温度] %.1f C\n", temp);
    (void) arg;
}

/** @brief 心跳上报（永久循环） */
static void report_heartbeat(void *arg)
{
    printf("  [心跳] uptime=%ld\n", (long) time(NULL));
    (void) arg;
}

/** @brief 快速计数（执行 5 次） */
static void burst_report(void *arg)
{
    printf("  [突发] 上报中...\n");
    (void) arg;
}

/** @brief 停止定时器 */
static void stop_cb(void *arg)
{
    TIMER *t = (TIMER *) arg;

    printf("\n--- 演示结束 ---\n");
    timing_stop(t);
}

/*===========================================================================
 * main
 *===========================================================================*/

int main(void)
{
    printf("=== timingTask 示例 ===\n");
    printf("(select + linkedList + threadPool, 50ms tick)\n\n");

    /* 创建定时器 */
    TIMER *t = timing_create(50, 2);

    if (NULL == t)
    {
        return EXIT_FAILURE;
    }

    /* count=-1: 每 1 秒上报温度，永久循环 */
    timing_add(t, 1000, 1000, -1, report_temperature, NULL);

    /* count=-1: 每 3 秒上报心跳 */
    timing_add(t, 3000, 3000, -1, report_heartbeat, NULL);

    /* count=5: 每 500ms 快速上报，共 5 次 */
    timing_add(t, 500, 500, 5, burst_report, NULL);

    /* count=1: 10 秒后停止 */
    timing_add(t, 10000, 0, 1, stop_cb, t);

    printf("任务已注册:\n");
    printf("  - 温度上报:  每 1s (永久)\n");
    printf("  - 心跳上报:  每 3s (永久)\n");
    printf("  - 突发上报:  每 500ms (共 5 次)\n");
    printf("  - 自动停止:  10s 后\n\n");

    timing_run(t);
    timing_destroy(t);

    return EXIT_SUCCESS;
}
