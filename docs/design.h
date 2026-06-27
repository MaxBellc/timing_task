/**
 * @mainpage timingTask — 嵌入式定时任务调度器
 *
 * @section intro 简介
 * timingTask 是一个基于 select + linkedList 的轻量级定时任务调度器，
 * 专为嵌入式 Linux 平台（泰山派 RK3566 等）设计。
 *
 * @section features 特性
 * - 三种执行模式: ONCE / REPEAT_N / REPEAT_FOREVER
 * - 毫秒精度，固定轮询间隔
 * - select 事件循环，可集成外部 fd (MQTT/串口/socket)
 * - linkedList 管理任务，实现简洁
 * - 单线程设计，无锁开销
 * - 安全的回调中取消机制
 *
 * @section usage 快速开始
 * @code
 *   timer_sched_t *sched = timer_sched_create(50);  // 50ms tick
 *   timer_sched_add_task(sched, 5000, 5000, -1,
 *                         my_callback, my_arg);
 *   timer_sched_run(sched);
 *   timer_sched_destroy(sched);
 * @endcode
 *
 * @section files 文件结构
 * - @ref timing_task.h   公共 API 头文件
 * - @ref timing_task.c   核心实现
 * - @ref mqtt_demo.c     定时 MQTT 演示
 * - @ref test_task.c     定时任务功能测试
 * - @ref test_fd.c       外部 fd 集成测试
 */
