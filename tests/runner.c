/**
 * @file    runner.c
 * @brief   单元测试入口
 *
 * @details 汇总所有测试套件，统一运行。
 *
 * @author  timingTask
 * @version 2.2.0
 * @date    2026
 *
 * @copyright MIT License
 */

#include "unity.h"

/*===========================================================================
 * 测试函数声明
 *===========================================================================*/

/* 生命周期 */
void test_create_destroy(void);
void test_destroy_null(void);
void test_create_zero_interval(void);

/* count 语义 */
void test_count_one(void);
void test_count_one_no_repeat(void);
void test_count_n(void);
void test_count_forever(void);

/* first_delay_ms */
void test_first_delay_zero(void);
void test_first_delay_positive(void);
void test_first_delay_order(void);

/* period_ms */
void test_period_zero(void);
void test_period_positive(void);

/* 取消 */
void test_cancel_before_fire(void);
void test_cancel_twice(void);
void test_cancel_null(void);
void test_cancel_from_other(void);

/* 错误处理 */
void test_null_timer(void);
void test_null_callback(void);

/* 多任务 */
void test_mixed_counts(void);
void test_many_tasks(void);

/* 边界 */
void test_stop_returns(void);
void test_destroy_without_run(void);

/*===========================================================================
 * 测试入口
 *===========================================================================*/

TEST_MAIN(
    /* ---- 生命周期 ---- */
    RUN_TEST(test_create_destroy);
    RUN_TEST(test_destroy_null);
    RUN_TEST(test_create_zero_interval);

    /* ---- count ---- */
    RUN_TEST(test_count_one);
    RUN_TEST(test_count_one_no_repeat);
    RUN_TEST(test_count_n);
    RUN_TEST(test_count_forever);

    /* ---- first_delay_ms ---- */
    RUN_TEST(test_first_delay_zero);
    RUN_TEST(test_first_delay_positive);
    RUN_TEST(test_first_delay_order);

    /* ---- period_ms ---- */
    RUN_TEST(test_period_zero);
    RUN_TEST(test_period_positive);

    /* ---- 取消 ---- */
    RUN_TEST(test_cancel_before_fire);
    RUN_TEST(test_cancel_twice);
    RUN_TEST(test_cancel_null);
    RUN_TEST(test_cancel_from_other);

    /* ---- 错误处理 ---- */
    RUN_TEST(test_null_timer);
    RUN_TEST(test_null_callback);

    /* ---- 多任务 ---- */
    RUN_TEST(test_mixed_counts);
    RUN_TEST(test_many_tasks);

    /* ---- 边界 ---- */
    RUN_TEST(test_stop_returns);
    RUN_TEST(test_destroy_without_run);
)
