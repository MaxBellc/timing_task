/**
 * @file    unity.h
 * @brief   嵌入式 C 轻量级单元测试框架
 *
 * @details 特性:
 *          - 单头文件，零外部依赖
 *          - 适合嵌入式 / 资源受限环境
 *          - 支持 setUp/tearDown 夹具
 *          - 断言失败自动报告 文件:行号
 *          - 彩色 PASS/FAIL 输出
 *          - 支持跳过测试 (TEST_IGNORE)
 *
 * 用法:
 * @code
 *   #include "unity.h"
 *
 *   static int value;
 *   void setUp(void)    { value = 42; }
 *   void tearDown(void) { value = 0;  }
 *
 *   void test_answer(void) {
 *       TEST_ASSERT_EQUAL(42, value);
 *   }
 *
 *   void test_null(void) {
 *       TEST_ASSERT_NULL(NULL);
 *   }
 *
 *   TEST_MAIN(
 *       RUN_TEST(test_answer);
 *       RUN_TEST(test_null);
 *   )
 * @endcode
 *
 * @author  timer_sched
 * @version 1.0.0
 * @date    2024
 *
 * @copyright MIT License
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 测试运行状态
 *===========================================================================*/

/** @brief 全局测试计数器 */
__attribute__((unused)) static int _unity_pass = 0;         /**< 通过的测试用例数 */
__attribute__((unused)) static int _unity_fail = 0;         /**< 失败的测试用例数 */
__attribute__((unused)) static int _unity_assertions = 0;   /**< 断言总数 */
__attribute__((unused)) static int _unity_skip = 0;           /**< 跳过的测试用例数 */
__attribute__((unused)) static const char *_unity_current_test = NULL;

/* ANSI 颜色（嵌入式终端可能不支持，自动降级为纯文本） */
#define _UNITY_CLR_RED   "\033[1;31m"
#define _UNITY_CLR_GREEN "\033[1;32m"
#define _UNITY_CLR_YELLOW "\033[1;33m"
#define _UNITY_CLR_RESET "\033[0m"

/*===========================================================================
 * 夹具钩子（弱符号，用户可选覆盖）
 *===========================================================================*/

/** @brief 每个测试用例执行前调用 */
__attribute__((weak)) void setUp(void) {}

/** @brief 每个测试用例执行后调用（即使断言失败也会调用） */
__attribute__((weak)) void tearDown(void) {}

/*===========================================================================
 * 内部辅助
 *===========================================================================*/

/** @brief 断言失败时输出信息 */
__attribute__((unused)) static void _unity_fail_msg(const char *file, int line,
                            const char *expected, const char *actual)
{
    printf(_UNITY_CLR_RED "  FAIL" _UNITY_CLR_RESET
           "  %s:%d: 期望 %s，实际 %s\n",
           file, line, expected, actual);
}

/** @brief 断言失败时输出信息（数值版本） */
__attribute__((unused)) static void _unity_fail_msg_i(const char *file, int line,
                              const char *msg,
                              int64_t expected, int64_t actual)
{
    printf(_UNITY_CLR_RED "  FAIL" _UNITY_CLR_RESET
           "  %s:%d: %s (期望 %ld, 实际 %ld)\n",
           file, line, msg, (long)expected, (long)actual);
}

/** @brief 断言失败时输出信息（无符号版本） */
__attribute__((unused)) static void _unity_fail_msg_u(const char *file, int line,
                              const char *msg,
                              uint64_t expected, uint64_t actual)
{
    printf(_UNITY_CLR_RED "  FAIL" _UNITY_CLR_RESET
           "  %s:%d: %s (期望 %lu, 实际 %lu)\n",
           file, line, msg, (unsigned long)expected, (unsigned long)actual);
}

/*===========================================================================
 * 断言宏
 *===========================================================================*/

/**
 * @brief 断言条件为真
 * @param cond 要检查的条件表达式
 */
#define TEST_ASSERT(cond)   do {                                            \
    _unity_assertions++;                                                    \
    if (!(cond)) {                                                          \
        _unity_fail_msg(__FILE__, __LINE__, "true", "false");               \
    }                                                                       \
} while (0)

/**
 * @brief 断言条件为真，失败时打印自定义消息
 * @param cond 要检查的条件表达式
 * @param msg  失败时的描述字符串
 */
#define TEST_ASSERT_MSG(cond, msg) do {                                     \
    _unity_assertions++;                                                    \
    if (!(cond)) {                                                          \
        _unity_fail_msg(__FILE__, __LINE__, msg, "false");                  \
    }                                                                       \
} while (0)

/**
 * @brief 断言两个有符号整数相等
 */
#define TEST_ASSERT_EQUAL(expected, actual) do {                            \
    _unity_assertions++;                                                    \
    int64_t _e = (int64_t)(expected);                                       \
    int64_t _a = (int64_t)(actual);                                         \
    if (_e != _a) {                                                         \
        _unity_fail_msg_i(__FILE__, __LINE__, "值不相等", _e, _a);          \
    }                                                                       \
} while (0)

/**
 * @brief 断言两个无符号整数相等
 */
#define TEST_ASSERT_EQUAL_UINT(expected, actual) do {                       \
    _unity_assertions++;                                                    \
    uint64_t _e = (uint64_t)(expected);                                     \
    uint64_t _a = (uint64_t)(actual);                                       \
    if (_e != _a) {                                                         \
        _unity_fail_msg_u(__FILE__, __LINE__, "值不相等", _e, _a);          \
    }                                                                       \
} while (0)

/**
 * @brief 断言两个整数不相等
 */
#define TEST_ASSERT_NOT_EQUAL(expected, actual) do {                        \
    _unity_assertions++;                                                    \
    int64_t _e = (int64_t)(expected);                                       \
    int64_t _a = (int64_t)(actual);                                         \
    if (_e == _a) {                                                         \
        _unity_fail_msg_i(__FILE__, __LINE__, "值不应相等", _e, _a);        \
    }                                                                       \
} while (0)

/**
 * @brief 断言指针为 NULL
 */
#define TEST_ASSERT_NULL(ptr) do {                                          \
    _unity_assertions++;                                                    \
    if (NULL != (ptr)) {                                                    \
        _unity_fail_msg(__FILE__, __LINE__, "NULL", "非 NULL 指针");         \
    }                                                                       \
} while (0)

/**
 * @brief 断言指针非 NULL
 */
#define TEST_ASSERT_NOT_NULL(ptr) do {                                      \
    _unity_assertions++;                                                    \
    if (NULL == (ptr)) {                                                    \
        _unity_fail_msg(__FILE__, __LINE__, "非 NULL 指针", "NULL");         \
    }                                                                       \
} while (0)

/**
 * @brief 断言条件为 true
 */
#define TEST_ASSERT_TRUE(cond)  TEST_ASSERT(cond)

/**
 * @brief 断言条件为 false
 */
#define TEST_ASSERT_FALSE(cond) TEST_ASSERT(!(cond))

/**
 * @brief 断言两个 C 字符串相等
 */
#define TEST_ASSERT_STREQUAL(expected, actual) do {                         \
    _unity_assertions++;                                                    \
    const char *_e = (expected);                                            \
    const char *_a = (actual);                                              \
    if (NULL == _e || NULL == _a || 0 != strcmp(_e, _a)) {                  \
        printf(_UNITY_CLR_RED "  FAIL" _UNITY_CLR_RESET                     \
               "  %s:%d: 字符串不相等\n", __FILE__, __LINE__);              \
        printf("        期望: \"%s\"\n", _e ? _e : "(null)");               \
        printf("        实际: \"%s\"\n", _a ? _a : "(null)");               \
    }                                                                       \
} while (0)

/**
 * @brief 断言两个内存块相等
 */
#define TEST_ASSERT_MEMORY_EQUAL(expected, actual, size) do {               \
    _unity_assertions++;                                                    \
    if (memcmp((expected), (actual), (size)) != 0) {                        \
        printf(_UNITY_CLR_RED "  FAIL" _UNITY_CLR_RESET                     \
               "  %s:%d: 内存内容不相等 (%zu 字节)\n",                       \
               __FILE__, __LINE__, (size_t)(size));                         \
    }                                                                       \
} while (0)

/*===========================================================================
 * 测试用例 & 运行器
 *===========================================================================*/

/**
 * @brief 运行一个测试用例
 *
 * 包装 setUp / tearDown 调用，统计用例级通过/失败。
 *
 * @param test_func 测试函数指针（void (*)(void)）
 */
#define RUN_TEST(test_func) do {                                            \
    _unity_current_test = #test_func;                                       \
    printf("  [TEST] %-50s", _unity_current_test);                          \
    fflush(stdout);                                                         \
    int _prev_assertions = _unity_assertions;                               \
    int _prev_fail = _unity_fail;                                           \
    setUp();                                                                \
    test_func();                                                            \
    tearDown();                                                             \
    if (_unity_assertions > _prev_assertions && _unity_fail == _prev_fail) {\
        _unity_pass++;                                                      \
        printf(_UNITY_CLR_GREEN "  PASS" _UNITY_CLR_RESET "\n");           \
    } else if (_unity_fail > _prev_fail) {                                  \
        printf("\n");                                                       \
    } else {                                                                \
        _unity_pass++;  /* 无断言的测试视为通过 */                            \
        printf(_UNITY_CLR_GREEN "  PASS" _UNITY_CLR_RESET "\n");           \
    }                                                                       \
} while (0)

/**
 * @brief 跳过某个测试用例
 * @param test_func 测试函数指针
 */
#define IGNORE_TEST(test_func) do {                                         \
    _unity_skip++;                                                          \
    printf("  [TEST] %-50s" _UNITY_CLR_YELLOW " SKIP" _UNITY_CLR_RESET "\n",\
           #test_func);                                                     \
} while (0)

/**
 * @brief 生成测试程序入口 main()
 * @param ... 一系列 RUN_TEST() 调用
 */
#define TEST_MAIN(...)                                                      \
int main(void) {                                                            \
    printf("\n=== " _UNITY_CLR_GREEN "timer_sched 单元测试"                  \
           _UNITY_CLR_RESET " ===\n\n");                                    \
    __VA_ARGS__                                                             \
    printf("\n--- 结果: %d 通过, %d 失败, %d 跳过 ---\n",                   \
           _unity_pass, _unity_fail, _unity_skip);                          \
    return _unity_fail > 0 ? EXIT_FAILURE : EXIT_SUCCESS;                   \
}

#ifdef __cplusplus
}
#endif

#endif /* UNITY_H */
