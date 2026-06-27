/**
 * @file    timing_task.c
 * @brief   嵌入式定时器实现 — CLOCK_MONOTONIC + linkedList + threadPool
 *
 * @details 核心设计:
 *          - linkedList 管理所有定时任务，append-only + O(n) 扫描
 *          - select 固定 timeout = interval_ms，每次唤醒扫描
 *          - next_ms 存 CLOCK_MONOTONIC 绝对时间，与 now_ms() 比较
 *          - 触发后回调提交到内部线程池异步执行，不阻塞事件循环
 *          - 任务的 free/重新入链在提交后立即决定，不等回调返回
 *
 * @author  timingTask
 * @version 3.0.0
 * @date    2026
 *
 * @copyright MIT License
 */

#include "timing_task.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/select.h>
#include <time.h>

#include "list.h"
#include "thread_pool.h"

/*===========================================================================
 * 内部数据结构
 *===========================================================================*/

/**
 * @brief 回调派发上下文
 *
 * 在 thread_pool_submit 时分配，dispatch_cb 中释放。
 * 独立于 TIMER_TASK，task 释放后 ctx 仍可安全访问。
 */
typedef struct _DISPATCH_CTX_
{
    timer_cb_t cb;   /**< 用户回调函数 */
    void      *arg;  /**< 用户上下文 */
} DISPATCH_CTX;

/**
 * @brief 定时任务
 *
 * next_ms 存 CLOCK_MONOTONIC 绝对时间，与 now_ms() 比较判断是否到期。
 * list_node == NULL 表示已从链表取出（正在线程池中执行或已取消）。
 *
 * 字段按 8→4→1 字节排序，尾部 _res[3] 补齐 4 字节对齐。
 */
typedef struct _TIMER_TASK_
{
    uint64_t   next_ms;       /**< 下次触发的 CLOCK_MONOTONIC 毫秒时间 */
    timer_cb_t cb;            /**< 回调函数指针 */
    void      *arg;           /**< 用户上下文指针 */
    LIST_NODE *list_node;     /**< 在链表中的节点，NULL = 已取出 */
    TIMER     *timer;         /**< 所属定时器 */
    uint32_t   period_ms;     /**< 触发后 next_ms 递增的量 */
    int32_t    count;         /**< 剩余执行次数，-1=永久 */
    int8_t     cancelled;     /**< 取消标记 */
    uint8_t    _res[3];       /**< 对齐保留 */
} TIMER_TASK;

/**
 * @brief 定时器
 *
 * 持有任务链表、事件循环状态以及内部线程池。
 */
typedef struct _TIMER_
{
    LIST_HEAD  *task_list;       /**< linkedList，存放 TIMER_TASK * */
    THREAD_POOL *pool;           /**< 内部线程池，用于异步执行回调 */
    uint32_t    interval_ms;     /**< select timeout，也是到期检查精度 */
    int8_t      running;         /**< 事件循环运行标志，非 0 表示运行中 */
    uint8_t     _res[3];         /**< 对齐保留 */
} TIMER;

/*===========================================================================
 * 时间工具
 *===========================================================================*/

/** @brief 获取当前 CLOCK_MONOTONIC 时间（毫秒） */
static uint64_t now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000ULL
           + (uint64_t) ts.tv_nsec / 1000000ULL;
}

/*===========================================================================
 * 线程池派发
 *===========================================================================*/

/**
 * @brief 线程池回调：执行用户回调后释放派发上下文
 *
 * @param[in] arg DISPATCH_CTX 指针
 */
static void dispatch_cb(void *arg)
{
    DISPATCH_CTX *ctx = (DISPATCH_CTX *) arg;

    if (NULL != ctx)
    {
        ctx->cb(ctx->arg);
        free(ctx);
    }
}

/*===========================================================================
 * 到期任务处理
 *===========================================================================*/

/**
 * @brief 扫描链表，将到期任务的回调提交到线程池
 *
 * 任务状态（count/next_ms）在提交前同步更新，提交后立即决定 free 还是
 * 重新入链。回调在线程池中异步执行，不阻塞事件循环。
 *
 * @param[in] t 定时器指针
 */
static void fire_expired_tasks(TIMER *t)
{
    uint64_t    now  = now_ms();
    LIST_NODE  *node = NULL;
    LIST_NODE  *next = NULL;
    TIMER_TASK *task = NULL;

    node = t->task_list->first;

    while (NULL != node)
    {
        next = node->next;
        task = (TIMER_TASK *) node->data;

        if ((NULL != task)
            && (0 == task->cancelled)
            && (task->next_ms <= now))
        {
            /* 从链表取出 */
            (void) list_remove(t->task_list, node);
            task->list_node = NULL;

            /* count > 0 时递减 */
            if (0 < task->count)
            {
                task->count--;
            }

            /* 推进触发时间 */
            task->next_ms += task->period_ms;

            /* 提交回调到线程池异步执行 */
            if (NULL != t->pool)
            {
                DISPATCH_CTX *ctx = calloc(1, sizeof(DISPATCH_CTX));

                if (NULL != ctx)
                {
                    ctx->cb  = task->cb;
                    ctx->arg = task->arg;
                    (void) thread_pool_submit(t->pool, dispatch_cb, ctx);
                }
            }

            /* 判断任务后续处理 */
            if (0 != task->cancelled)
            {
                free(task);
            }
            else if (0 != task->count)
            {
                /* 还有剩余次数（或永久循环），重新入链 */
                task->list_node = list_push_back(t->task_list, task);
            }
            else
            {
                /* 次数用完 */
                free(task);
            }
        }

        node = next;
    }

    return;
}

/*===========================================================================
 * 公共 API 实现
 *===========================================================================*/

/**
 * @brief 创建定时器
 *
 * 分配 TIMER 结构体，创建 linkedList 和内部线程池。
 *
 * @param[in] interval_ms  轮询间隔（毫秒），select 固定 timeout。0 自动修正为 1。
 * @param[in] num_threads  线程池工作线程数，必须 >= 1
 * @return 成功返回定时器指针，失败返回 NULL
 */
TIMER *timing_create(uint32_t interval_ms, int32_t num_threads)
{
    TIMER *t = NULL;

    if (0 >= num_threads)
    {
        return NULL;
    }

    t = calloc(1, sizeof(TIMER));

    if (NULL == t)
    {
        return NULL;
    }

    /* 轮询间隔至少 1ms */
    t->interval_ms = (0 == interval_ms) ? 1 : interval_ms;

    /* 创建内部线程池 */
    t->pool = thread_pool_create(num_threads);

    if (NULL == t->pool)
    {
        free(t);
        return NULL;
    }

    /* 创建任务链表 */
    t->task_list = list_create();

    if (NULL == t->task_list)
    {
        thread_pool_destroy(t->pool);
        free(t);
        return NULL;
    }

    t->running = 0;

    return t;
}

/**
 * @brief 销毁定时器，释放所有资源
 *
 * 等待所有正在执行的回调完成后释放线程池和任务链表。
 *
 * @param[in] t 定时器指针，传 NULL 无操作
 */
void timing_destroy(TIMER *t)
{
    LIST_NODE *node = NULL;
    LIST_NODE *next = NULL;

    if (NULL == t)
    {
        return;
    }

    /* 等待线程池中所有回调执行完毕 */
    thread_pool_wait(t->pool);

    /* 释放所有任务数据 */
    node = t->task_list->first;

    while (NULL != node)
    {
        next = node->next;
        free(node->data);
        node = next;
    }

    list_destroy(t->task_list);
    thread_pool_destroy(t->pool);
    free(t);

    return;
}

/**
 * @brief 启动事件循环（阻塞）
 *
 * 以 interval_ms 为周期调用 select，每次唤醒后扫描到期任务并提交到线程池。
 * 循环直到 timing_stop() 被调用或 select 发生不可恢复的错误。
 *
 * @param[in] t 定时器指针
 * @return 正常退出返回 0，t 为 NULL 或 select 出错返回 -1
 */
int32_t timing_run(TIMER *t)
{
    struct timeval tv;
    struct timeval timeout;
    int32_t        n = 0;

    if (NULL == t)
    {
        return -1;
    }

    t->running = 1;

    /* 固定 timeout */
    tv.tv_sec  = (time_t) (t->interval_ms / 1000);
    tv.tv_usec = (suseconds_t) ((t->interval_ms % 1000) * 1000);

    while (0 != t->running)
    {
        /* select 纯睡眠 */
        timeout = tv;
        n       = select(0, NULL, NULL, NULL, &timeout);

        if ((0 > n) && (EINTR != errno))
        {
            t->running = 0;
            return -1;
        }

        /* 扫描到期任务并提交到线程池 */
        fire_expired_tasks(t);
    }

    return 0;
}

/**
 * @brief 停止事件循环
 *
 * 设置 running = 0，可从任何回调中安全调用，最多延迟 interval_ms。
 *
 * @param[in] t 定时器指针
 * @return 成功返回 0，t 为 NULL 返回 -1
 */
int32_t timing_stop(TIMER *t)
{
    if (NULL == t)
    {
        return -1;
    }

    t->running = 0;
    return 0;
}

/**
 * @brief 添加定时任务
 *
 * 创建 TIMER_TASK，设置 next_ms = now + first_delay_ms，加入链表。
 *
 * @param[in] t              定时器指针
 * @param[in] first_delay_ms 首次触发延迟（毫秒），0 = 下个 tick 立即触发
 * @param[in] period_ms      重复间隔（毫秒），0 = 每个 tick 都触发
 * @param[in] count          执行次数: -1=永久, 1=单次, N=N次
 * @param[in] cb             回调函数
 * @param[in] arg            用户上下文指针
 *
 * @return 成功返回任务句柄，失败返回 NULL
 */
TIMER_TASK *timing_add(TIMER     *t,
                       uint32_t   first_delay_ms,
                       uint32_t   period_ms,
                       int32_t    count,
                       timer_cb_t cb,
                       void      *arg)
{
    TIMER_TASK *task = NULL;

    if ((NULL == t) || (NULL == cb))
    {
        return NULL;
    }

    task = calloc(1, sizeof(TIMER_TASK));

    if (NULL == task)
    {
        return NULL;
    }

    /* next_ms = now + first_delay_ms（0 = 下个 tick 立即触发） */
    task->next_ms   = now_ms() + first_delay_ms;
    task->cb        = cb;
    task->arg       = arg;
    task->timer     = t;
    task->period_ms = period_ms;
    task->count     = count;
    task->cancelled = 0;

    /* 加入链表 */
    task->list_node = list_push_back(t->task_list, task);

    if (NULL == task->list_node)
    {
        free(task);
        return NULL;
    }

    return task;
}

/**
 * @brief 取消定时任务
 *
 * 只对尚未触发的任务有效。调用后 task 立即失效。
 *
 * @param[in] task 任务句柄
 * @return 成功返回 0，task 为 NULL 或已取消返回 -1
 */
int32_t timing_cancel(TIMER_TASK *task)
{
    if ((NULL == task) || (0 != task->cancelled))
    {
        return -1;
    }

    task->cancelled = 1;

    if (NULL != task->list_node)
    {
        /* 任务还在链表中，移除并释放 */
        (void) list_remove(task->timer->task_list, task->list_node);
        free(task);
    }
    /* list_node == NULL: 已取出正在线程池中执行，回调结束后 dispatch_cb 不会访问 task */

    return 0;
}
