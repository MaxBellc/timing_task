/**
 * @file    test_task.c
 * @brief   timingTask API 单元测试
 *
 * @details 通过公开 API 覆盖所有功能和边界。
 *          每项测试独立创建/销毁定时器，确保测试隔离。
 *
 * 测试覆盖:
 *   - 生命周期: create / destroy
 *   - count: 1 / N / -1
 *   - first_delay_ms: 0 / >0
 *   - period_ms: 0 / >0
 *   - 取消: 触发前 / 回调中 / 重复 / NULL
 *   - 错误处理: NULL 参数
 *   - 多任务并发
 *   - 边界: interval_ms=0 / destroy-without-run
 *
 * @author  timingTask
 * @version 2.2.0
 * @date    2026
 *
 * @copyright MIT License
 */

#include "timing_task.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>

/*===========================================================================
 * 测试上下文
 *===========================================================================*/

/**
 * @brief 测试上下文
 */
typedef struct _TASK_TEST_CTX_
{
    int32_t  fire_count;  /**< 触发次数 */
    TIMER   *timer;       /**< 定时器指针，用于回调中 stop */
} TASK_TEST_CTX;

/*===========================================================================
 * 回调函数
 *===========================================================================*/

/** @brief 简单计数 */
static void count_cb(void *arg)
{
    TASK_TEST_CTX *ctx = (TASK_TEST_CTX *) arg;

    ctx->fire_count++;
}

/** @brief 计数并停止定时器 */
static void stop_cb(void *arg)
{
    TASK_TEST_CTX *ctx = (TASK_TEST_CTX *) arg;

    ctx->fire_count++;
    timing_stop(ctx->timer);
}

/*===========================================================================
 * 取消测试辅助
 *===========================================================================*/

static TIMER_TASK *g_cancel_target = NULL;

/** @brief 取消全局任务并计数 */
static void cancel_target_cb(void *arg)
{
    TASK_TEST_CTX *ctx = (TASK_TEST_CTX *) arg;

    ctx->fire_count++;
    (void) timing_cancel(g_cancel_target);
}

/*===========================================================================
 * 测试: 生命周期
 *===========================================================================*/

/** @brief 创建和销毁 */
void test_create_destroy(void)
{
    TIMER *timer = timing_create(50, 2);

    TEST_ASSERT_NOT_NULL(timer);
    timing_destroy(timer);
}

/** @brief destroy(NULL) 不崩溃 */
void test_destroy_null(void)
{
    timing_destroy(NULL);
    TEST_ASSERT_TRUE(1);
}

/** @brief interval_ms=0 自动修正为 1 */
void test_create_zero_interval(void)
{
    TIMER *timer = timing_create(0, 2);

    TEST_ASSERT_NOT_NULL(timer);
    timing_destroy(timer);
}

/*===========================================================================
 * 测试: count 语义
 *===========================================================================*/

/** @brief count=1 触发一次后自动删除 */
void test_count_one(void)
{
    TASK_TEST_CTX ctx = {0};
    TIMER        *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);
    ctx.timer = timer;

    /* 50ms 后触发并停止 */
    timing_add(timer, 50, 50, 1, stop_cb, &ctx);
    timing_run(timer);
    timing_destroy(timer);

    TEST_ASSERT_EQUAL(1, ctx.fire_count);
}

/** @brief count=1 + period=0 也只触发一次 */
void test_count_one_no_repeat(void)
{
    TIMER *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);

    TASK_TEST_CTX ctx_a = {0};
    TASK_TEST_CTX ctx_b = { .timer = timer };

    /* ctx_a: count=1 + period=0，应立即触发且不重复 */
    timing_add(timer, 0, 0, 1, count_cb, &ctx_a);
    /* ctx_b: 150ms 后停止 */
    timing_add(timer, 150, 150, 1, stop_cb, &ctx_b);

    timing_run(timer);
    timing_destroy(timer);

    TEST_ASSERT_EQUAL(1, ctx_a.fire_count);
}

/** @brief count=3 正好触发 3 次 */
void test_count_n(void)
{
    TASK_TEST_CTX ctx = {0};
    TIMER        *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);

    /* 每 20ms，共 3 次。120ms 后停止确保 3 次都完成 */
    timing_add(timer, 20, 20, 3, count_cb, &ctx);
    ctx.timer = timer;
    timing_add(timer, 120, 120, 1, stop_cb, &ctx);

    timing_run(timer);
    timing_destroy(timer);

    /* 3 次 count + 1 次 stop = 4 */
    TEST_ASSERT_EQUAL(4, ctx.fire_count);
}

/** @brief count=-1 持续触发不停止 */
void test_count_forever(void)
{
    TASK_TEST_CTX ctx = {0};
    TIMER        *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);
    ctx.timer = timer;

    /* 每 20ms，首次立即 */
    timing_add(timer, 0, 20, -1, count_cb, &ctx);
    /* 120ms 后停止 */
    timing_add(timer, 120, 120, 1, stop_cb, &ctx);

    timing_run(timer);
    timing_destroy(timer);

    /* 120ms / 20ms ≈ 6 次 + 1 次首次 + 1 次 stop */
    TEST_ASSERT_MSG(ctx.fire_count >= 7 && ctx.fire_count <= 10,
                    "count=-1 应触发 7-10 次");
}

/*===========================================================================
 * 测试: first_delay_ms
 *===========================================================================*/

/** @brief first_delay_ms=0: 下个 tick 立即触发 */
void test_first_delay_zero(void)
{
    TASK_TEST_CTX ctx = {0};
    TIMER        *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);
    ctx.timer = timer;

    timing_add(timer, 0, 100, 1, stop_cb, &ctx);
    timing_run(timer);
    timing_destroy(timer);

    TEST_ASSERT_EQUAL(1, ctx.fire_count);
}

/** @brief first_delay_ms>0: 等够时间才触发 */
void test_first_delay_positive(void)
{
    TASK_TEST_CTX ctx = {0};
    TIMER        *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);
    ctx.timer = timer;

    timing_add(timer, 50, 50, 1, stop_cb, &ctx);
    timing_run(timer);
    timing_destroy(timer);

    TEST_ASSERT_EQUAL(1, ctx.fire_count);
}

/** @brief first_delay_ms=0 的任务先于 >0 的任务触发 */
void test_first_delay_order(void)
{
    TASK_TEST_CTX ctx_zero   = {0};
    TASK_TEST_CTX ctx_delay  = {0};
    TASK_TEST_CTX ctx_stop   = {0};
    TIMER        *timer = timing_create(20, 2);

    TEST_ASSERT_NOT_NULL(timer);

    /* first_delay_ms=0: 下个 tick */
    timing_add(timer, 0, 200, 1, count_cb, &ctx_zero);
    /* first_delay_ms=200: 等 200ms */
    timing_add(timer, 200, 200, 1, count_cb, &ctx_delay);
    /* 100ms 后停止，delay 任务应还未触发 */
    ctx_stop.timer = timer;
    timing_add(timer, 100, 100, 1, stop_cb, &ctx_stop);

    timing_run(timer);
    timing_destroy(timer);

    /* 0-delay 已触发，200-delay 未触发 */
    TEST_ASSERT_EQUAL(1, ctx_zero.fire_count);
    TEST_ASSERT_EQUAL(0, ctx_delay.fire_count);
}

/*===========================================================================
 * 测试: period_ms
 *===========================================================================*/

/** @brief period_ms=0: 每个 tick 都触发 */
void test_period_zero(void)
{
    TASK_TEST_CTX ctx = {0};
    TIMER        *timer = timing_create(20, 2);

    TEST_ASSERT_NOT_NULL(timer);
    ctx.timer = timer;

    /* period=0 + count=-1: 每个 tick 触发，60ms 后停止 */
    timing_add(timer, 0, 0, -1, count_cb, &ctx);
    timing_add(timer, 60, 60, 1, stop_cb, &ctx);

    timing_run(timer);
    timing_destroy(timer);

    /* 60ms / 20ms tick，异步执行允许波动 */
    TEST_ASSERT_MSG(ctx.fire_count >= 2 && ctx.fire_count <= 6,
                    "period=0 每个 tick 触发");
}

/** @brief period_ms>0 + first_delay_ms=0: 立即首次，之后按间隔 */
void test_period_positive(void)
{
    TASK_TEST_CTX ctx = {0};
    TIMER        *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);
    ctx.timer = timer;

    timing_add(timer, 0, 30, -1, count_cb, &ctx);
    /* 100ms 后停止 */
    timing_add(timer, 100, 100, 1, stop_cb, &ctx);

    timing_run(timer);
    timing_destroy(timer);

    /* 100ms / 30ms ≈ 3 次 + 1 首次 + 1 stop */
    TEST_ASSERT_MSG(ctx.fire_count >= 4 && ctx.fire_count <= 6,
                    "period>0 应触发 4-6 次");
}

/*===========================================================================
 * 测试: 取消任务
 *===========================================================================*/

/** @brief 触发前取消，回调不被调用 */
void test_cancel_before_fire(void)
{
    TASK_TEST_CTX ctx = {0};
    TIMER        *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);

    /* 注册后立即取消 */
    TIMER_TASK *task = timing_add(timer, 50, 50, 1, count_cb, &ctx);

    TEST_ASSERT_NOT_NULL(task);
    TEST_ASSERT_EQUAL(0, timing_cancel(task));

    /* 100ms 后停止 */
    ctx.timer = timer;
    timing_add(timer, 100, 100, 1, stop_cb, &ctx);

    timing_run(timer);
    timing_destroy(timer);

    /* 只有 stop_cb 触发，count_cb 不应触发 */
    TEST_ASSERT_EQUAL(1, ctx.fire_count);
}

/** @brief 重复取消返回 -1 */
void test_cancel_twice(void)
{
    TIMER *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);

    TIMER_TASK *task = timing_add(timer, 100, 100, 1, count_cb, NULL);

    TEST_ASSERT_NOT_NULL(task);
    TEST_ASSERT_EQUAL(0, timing_cancel(task));
    /* 重复取消失败 */
    TEST_ASSERT_EQUAL(-1, timing_cancel(task));

    timing_destroy(timer);
}

/** @brief cancel(NULL) 返回 -1 */
void test_cancel_null(void)
{
    TEST_ASSERT_EQUAL(-1, timing_cancel(NULL));
}

/** @brief 从其他回调中取消任务 */
void test_cancel_from_other(void)
{
    TIMER        *timer = timing_create(10, 2);
    TASK_TEST_CTX target_ctx = {0};
    TASK_TEST_CTX cancel_ctx = {0};
    TASK_TEST_CTX stop_ctx   = {0};

    TEST_ASSERT_NOT_NULL(timer);

    /* 任务 A: 80ms 后触发，应被取消 */
    TIMER_TASK *target = timing_add(timer, 80, 80, 1, count_cb, &target_ctx);

    TEST_ASSERT_NOT_NULL(target);

    /* 任务 B: 40ms 时取消 A */
    g_cancel_target = target;
    timing_add(timer, 40, 40, 1, cancel_target_cb, &cancel_ctx);

    /* 任务 C: 120ms 时停止 */
    stop_ctx.timer = timer;
    timing_add(timer, 120, 120, 1, stop_cb, &stop_ctx);

    timing_run(timer);
    timing_destroy(timer);

    /* target 在 40ms 被取消，80ms 不应触发 */
    TEST_ASSERT_EQUAL(0, target_ctx.fire_count);
    TEST_ASSERT_EQUAL(1, cancel_ctx.fire_count);
    TEST_ASSERT_EQUAL(1, stop_ctx.fire_count);

    g_cancel_target = NULL;
}

/*===========================================================================
 * 测试: 错误处理
 *===========================================================================*/

/** @brief NULL 定时器参数 */
void test_null_timer(void)
{
    TEST_ASSERT_NULL(
        timing_add(NULL, 100, 100, 1, count_cb, NULL));
    TEST_ASSERT_EQUAL(-1, timing_run(NULL));
    TEST_ASSERT_EQUAL(-1, timing_stop(NULL));
}

/** @brief NULL 回调参数 */
void test_null_callback(void)
{
    TIMER *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);
    TEST_ASSERT_NULL(
        timing_add(timer, 100, 100, 1, NULL, NULL));
    timing_destroy(timer);
}

/*===========================================================================
 * 测试: 多任务并发
 *===========================================================================*/

/** @brief count=1 / count=4 / count=-1 三种任务同时运行 */
void test_mixed_counts(void)
{
    TASK_TEST_CTX ctx_1   = {0};
    TASK_TEST_CTX ctx_4   = {0};
    TASK_TEST_CTX ctx_inf = {0};
    TIMER        *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);

    /* count=1: 60ms */
    timing_add(timer, 60, 60, 1, count_cb, &ctx_1);
    /* count=4: 每 15ms，首次立即 */
    timing_add(timer, 0, 15, 4, count_cb, &ctx_4);
    /* count=-1: 每 25ms */
    timing_add(timer, 25, 25, -1, count_cb, &ctx_inf);

    /* 120ms 后停止 */
    TASK_TEST_CTX stop_ctx = { .timer = timer };
    timing_add(timer, 120, 120, 1, stop_cb, &stop_ctx);

    timing_run(timer);
    timing_destroy(timer);

    TEST_ASSERT_EQUAL(1, ctx_1.fire_count);
    TEST_ASSERT_EQUAL(4, ctx_4.fire_count);
    /* 120ms / 25ms ≈ 4-5 次 */
    TEST_ASSERT_MSG(ctx_inf.fire_count >= 3 && ctx_inf.fire_count <= 6,
                    "count=-1 应触发 3-6 次");
}

/** @brief 50 个任务注册 */
void test_many_tasks(void)
{
    int32_t        i     = 0;
    TIMER         *timer = timing_create(10, 2);
    TASK_TEST_CTX  ctx   = {0};

    TEST_ASSERT_NOT_NULL(timer);

    for (i = 0; i < 50; i++)
    {
        TIMER_TASK *t = timing_add(timer,
                                   (uint32_t)(50 + i * 2),
                                   (uint32_t)(50 + i * 2),
                                   1,
                                   count_cb, &ctx);
        TEST_ASSERT_NOT_NULL(t);
    }

    /* 300ms 后停止 */
    TASK_TEST_CTX stop_ctx = { .timer = timer };
    timing_add(timer, 300, 300, 1, stop_cb, &stop_ctx);

    timing_run(timer);
    timing_destroy(timer);

    TEST_ASSERT_MSG(ctx.fire_count > 0, "至少部分任务已触发");
}

/*===========================================================================
 * 测试: 边界
 *===========================================================================*/

/** @brief stop 后 run 正常返回 */
void test_stop_returns(void)
{
    TIMER *timer = timing_create(10, 2);

    TEST_ASSERT_NOT_NULL(timer);

    /* 10ms 后 stop */
    timing_add(timer, 10, 10, 1, stop_cb,
               &(TASK_TEST_CTX){ .timer = timer });

    TEST_ASSERT_EQUAL(0, timing_run(timer));
    timing_destroy(timer);
}

/** @brief 未 run 直接 destroy 不崩溃 */
void test_destroy_without_run(void)
{
    TIMER        *timer = timing_create(10, 2);
    TASK_TEST_CTX ctx   = {0};

    TEST_ASSERT_NOT_NULL(timer);

    timing_add(timer, 1000, 1000, 1, count_cb, &ctx);
    timing_add(timer, 2000, 2000, -1, count_cb, &ctx);

    timing_destroy(timer);
    TEST_ASSERT_TRUE(1);
}
