/**
 * @file    timing_task.h
 * @brief   嵌入式定时器 — select + linkedList + threadPool 实现
 *
 * @details 内部持有线程池，回调异步执行，不阻塞事件循环。
 *          count: 1=单次, N=N次, -1=永久。
 *          first_delay_ms / period_ms 两个参数控制触发时间。
 *          CLOCK_MONOTONIC 绝对时间比较，回调耗时不影响精度。
 *
 * @author  timingTask
 * @version 3.0.0
 * @date    2026
 *
 * @copyright MIT License
 */

#ifndef TIMING_TASK_H
#define TIMING_TASK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*===========================================================================
 * 类型定义
 *===========================================================================*/

/** @brief 定时器句柄（不透明指针） */
typedef struct _TIMER_ TIMER;

/** @brief 定时任务句柄（不透明指针，用于取消任务） */
typedef struct _TIMER_TASK_ TIMER_TASK;

/** @brief 定时任务回调 */
typedef void (*timer_cb_t)(void *arg);

/*===========================================================================
 * 定时器操作
 *===========================================================================*/

/**
 * @brief 创建定时器
 *
 * 内部创建线程池（num_threads 个工作线程），回调在线程池中异步执行。
 *
 * @param[in] interval_ms  轮询间隔（毫秒），select 固定 timeout
 * @param[in] num_threads  线程池工作线程数，必须 >= 1
 * @return 成功返回定时器指针，失败返回 NULL
 */
TIMER *timing_create(uint32_t interval_ms, int32_t num_threads);

/**
 * @brief 销毁定时器，释放所有资源
 *
 * 等待所有正在执行的回调完成后释放线程池和任务链表。
 *
 * @param[in] t 定时器指针，传 NULL 无操作
 */
void timing_destroy(TIMER *t);

/**
 * @brief 启动事件循环（阻塞）
 *
 * 每 interval_ms 唤醒一次，扫描到期任务并提交到线程池执行。
 * 循环直到 timing_stop() 被调用或发生错误。
 *
 * @param[in] t 定时器指针
 * @return 正常退出返回 0，出错返回 -1
 */
int32_t timing_run(TIMER *t);

/**
 * @brief 停止事件循环
 *
 * 可从任何回调中安全调用，最多延迟 interval_ms。
 *
 * @param[in] t 定时器指针
 * @return 成功返回 0，t 为 NULL 返回 -1
 */
int32_t timing_stop(TIMER *t);

/*===========================================================================
 * 定时任务操作
 *===========================================================================*/

/**
 * @brief 添加定时任务
 *
 * @param[in] t              定时器指针
 * @param[in] first_delay_ms 首次触发延迟（毫秒），0 = 下个 tick 立即触发
 * @param[in] period_ms      重复间隔（毫秒），0 = 每个 tick 都触发
 * @param[in] count          执行次数: -1=永久, 1=单次, N=N次
 * @param[in] cb             回调函数
 * @param[in] arg            用户上下文指针
 *
 * @return 成功返回任务句柄（可用于 timing_cancel），失败返回 NULL
 */
TIMER_TASK *timing_add(TIMER     *t,
                       uint32_t   first_delay_ms,
                       uint32_t   period_ms,
                       int32_t    count,
                       timer_cb_t cb,
                       void      *arg);

/**
 * @brief 取消定时任务
 *
 * 只对尚未触发的任务有效。已提交到线程池执行中的回调不受影响。
 * 调用后 task 指针立即失效。
 *
 * @param[in] task 任务句柄
 * @return 成功返回 0，task 为 NULL 或已取消返回 -1
 */
int32_t timing_cancel(TIMER_TASK *task);

#ifdef __cplusplus
}
#endif

#endif /* TIMING_TASK_H */
