# timingTask — 嵌入式定时任务库

基于 `select` + `linkedList` + `threadPool` 的轻量级定时器，适用于嵌入式 Linux 平台。

## 特性

- **异步回调** — 内部线程池执行用户回调，不阻塞事件循环
- **三种执行次数** — `count`: 1 = 单次, N = N 次, -1 = 永久
- **两个时间参数** — `first_delay_ms` 控制首次, `period_ms` 控制重复间隔
- **绝对时间** — CLOCK_MONOTONIC 比较，回调耗时不影响定时精度
- **单线程事件循环** — select 固定轮询，无锁
- **CMake 构建** — 支持交叉编译

## 依赖

| 库 | 说明 |
|----|------|
| [linkedList](https://github.com/MaxBellc/linkedList) | 任务链表 |
| [threadPool](https://github.com/MaxBellc/threadPool) | 内部回调线程池 |

## 构建

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 交叉编译 (aarch64)

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/aarch64-linux-gnu.cmake
make -j$(nproc)
```

产物：`libtiming_task.a`、`demo`、`test_runner`

## API

```c
#include "timing_task.h"

/* 创建定时器，interval_ms=轮询间隔，num_threads=回调线程数 */
TIMER *t = timing_create(50, 2);

/* 添加任务: first_delay=首次延迟(0=立即), period=间隔(0=每tick), count=-1/1/N */
TIMER_TASK *task = timing_add(t, 5000, 5000, -1, my_callback, my_arg);

/* 取消未触发的任务 */
timing_cancel(task);

/* 启动事件循环（阻塞），直到 timing_stop() */
timing_run(t);

/* 停止事件循环（可在回调中调用） */
timing_stop(t);

/* 销毁（等待所有回调执行完毕） */
timing_destroy(t);
```

## 示例

```bash
./build/demo
```

输出：

```
=== timingTask 示例 ===
(select + linkedList + threadPool, 50ms tick)

  [温度] 25.5 C
  [温度] 26.0 C
  [突发] 上报中...
  [心跳] uptime=1782500000
  ...
```

## 测试

```bash
./build/test_runner
```

## 文档

| 文档 | 内容 |
|------|------|
| [docs/design.md](docs/design.md) | 架构、数据流、设计取舍 |
| [docs/design.h](docs/design.h) | Doxygen 入口 |

## 文件结构

```
timingTask/
├── CMakeLists.txt
├── cmake/
│   └── aarch64-linux-gnu.cmake
├── include/
│   └── timing_task.h        # 公共 API
├── src/
│   └── timing_task.c        # 核心实现
├── examples/
│   └── demo.c
├── tests/
│   ├── test_task.c
│   ├── runner.c
│   └── unity.h
├── docs/
│   ├── design.md
│   └── design.h
└── README.md
```

## 许可证

MIT
