# timingTask 设计文档

## 1. 架构

```
┌──────────────────────────────────────────────┐
│                    TIMING                      │
│                                              │
│  ┌──────────────┐                            │
│  │   task_list  │  linkedList                │
│  │              │  task1 → task2 → task3 →...│
│  └──────┬───────┘                            │
│         │                                    │
│         ▼                                    │
│   select(timeout = interval_ms)              │
│         │                                    │
│         ▼                                    │
│   fire_expired_tasks()                       │
│   ├─ now_ms() 取实时钟                       │
│   ├─ 扫描链表: next_ms <= now → 触发         │
│   └─ 根据 count 决定 free / push_back         │
└──────────────────────────────────────────────┘
```

单线程。select 固定 timeout 做轮询精度，CLOCK_MONOTONIC 做绝对时间比较。

## 2. 核心数据流

```
timing_create(50)
  → calloc(TIMING)
  → interval_ms = 50, task_list = list_create()

timing_add(t, 5000, 5000, -1, cb, arg)
  → calloc(TIMING_TASK)
  → next_ms = now_ms() + 5000    // CLOCK_MONOTONIC 绝对时间
  → period_ms = 5000, count = -1
  → list_push_back(task_list, task)

timing_run(t)
  → running = 1
  → while (running):
      select(0, NULL, NULL, NULL, &timeout=50ms)
      fire_expired_tasks()
        ├─ now = now_ms()
        ├─ 扫描链表
        ├─ next_ms <= now → list_remove → count>0 递减 → next_ms += period_ms → cb()
        ├─ cancelled? → free
        ├─ count != 0? → push_back (或永久循环)
        └─ count == 0? → free

timing_stop(t)
  → running = 0
  → 下个 select 返回时退出 while 循环
```

## 3. 内部结构

```c
struct _TIMER_ {
    LIST_HEAD *task_list;       // 8B, linkedList
    uint32_t   interval_ms;     // 4B, select timeout + 检查精度
    int8_t     running;         // 1B
    uint8_t    _res[3];         // 对齐
};

struct _TIMER_TASK_ {
    uint64_t   next_ms;         // 8B, CLOCK_MONOTONIC 绝对触发时间
    timer_cb_t cb;              // 8B
    void      *arg;             // 8B
    LIST_NODE *list_node;       // 8B, NULL = 已取出
    TIMING     *timer;           // 8B, 所属定时器
    uint32_t   period_ms;       // 4B, 触发后 next_ms += period_ms
    int32_t    count;           // 4B, -1=永久, >0 递减
    int8_t     cancelled;       // 1B
    uint8_t    _res[3];         // 对齐
};
```

## 4. API

```c
/* 生命周期 */
TIMING *timing_create(uint32_t interval_ms);
void   timing_destroy(TIMING *t);
int32_t timing_run(TIMING *t);      /* 阻塞 */
int32_t timing_stop(TIMING *t);     /* 异步安全 */

/* 添加任务 */
TIMING_TASK *timing_add(TIMING    *t,
                       uint32_t  first_delay_ms,  /* 0=立即 */
                       uint32_t  period_ms,       /* 0=每tick */
                       int32_t   count,           /* -1/1/N */
                       timer_cb_t cb,
                       void     *arg);

/* 取消任务 */
int32_t timing_cancel(TIMING_TASK *task);
```

## 5. 时间模型

```
                    first_delay_ms    period_ms     period_ms
                        │               │             │
    ──┬─────────────────┼───────────────┼─────────────┼────→
      │                 ▼               ▼             ▼
     创建             首次触发        第二次触发     第三次触发
   next_ms =          next_ms        next_ms        next_ms
   now + first        += period      += period      += period
```

- `next_ms` 始终存 CLOCK_MONOTONIC 绝对时间。
- `first_delay_ms = 0` → 下个 tick 立即触发首次（不是跳过一周期）。
- `period_ms = 0` → 每个 tick 都触发（`next_ms += 0` 不变，每次 `next_ms <= now` 仍成立）。
- 使用 CLOCK_MONOTONIC 而非 tick 累加：回调耗时不影响时间判断，不存在漂移。
- `uint64_t` 毫秒单位，不会 49 天溢出。

## 6. count 语义

```
-1  → 永久循环，不递减
 1  → 单次，触发后 free
 N  → 执行 N 次，每次 count--，到 0 free
```

触发逻辑（`fire_expired_tasks`）:

```
if (next_ms <= now) {
    list_remove
    if (count > 0) count--
    next_ms += period_ms
    cb(arg)
    if (cancelled) → free
    elif (count != 0) → push_back
    else → free
}
```

## 7. 安全取消

两个状态互斥：
- `list_node != NULL` — 在链表中等待，`cancel()` 直接 `list_remove` + `free`
- `list_node == NULL` — 已从链表取出，正在回调中，`cancel()` 只设 `cancelled = 1`，由 `fire_expired_tasks` 在回调返回后 `free`

没有竞态。单线程下不需要锁。

## 8. 回调耗时影响

因为用 `now_ms()` 绝对时间做比较：如果某个回调执行了 3 秒，回来后 `now` 已过 3 秒，所有期间该触发的任务一次性赶上。不会丢触发：

```
tick N:     next_ms=1000, now=1000 → 触发，callback 开始
callback:   执行了 3 秒...
tick N+60:  callback 返回，now=4000
            扫描链表: 所有 next_ms <= 4000 的任务都触发
```

代价：回调阻塞了整个事件循环，期间新任务得不到调度。这是单线程模型的固有取舍。

## 9. 设计取舍

| 决策 | 选择 | 原因 |
|------|------|------|
| 任务存储 | linkedList, O(n) 扫描 | 嵌入式 3-50 任务，遍历够用 |
| 事件循环 | select 固定 timeout | POSIX，无额外 fd |
| 时间源 | CLOCK_MONOTONIC | 回调耗时不影响精度 |
| 定时粒度 | `interval_ms` | 固定轮询，不动态计算 timeout |
| 停止延迟 | 最多 1 tick | 可接受，无需额外唤醒机制 |
| 线程 | 单线程 | 无锁，回调安全 |
| 模式统一 | `count`: -1/1/N | 消掉枚举，一个字段管所有 |
| 首次控制 | `first_delay_ms` | 独立参数，比 flags 直观 |
| 类型命名 | `UPPER_CASE` | 符合编码规范 |
| 定宽类型 | `int32_t/int8_t/uint32_t` | 符合编码规范 |
| 字段排序 | 8→4→1 字节 + _res 对齐 | 符合编码规范 |

## 10. 使用示例

```c
TIMING *t = timing_create(50);

/* 5 秒后首次，之后每 2 秒上报，永久 */
timing_add(t, 5000, 2000, -1, report_temp, NULL);

/* 立即触发一次初始化 */
timing_add(t, 0, 0, 1, init_device, NULL);

/* 3 秒后开始，每 3 秒定位一次，共 5 次 */
timing_add(t, 3000, 3000, 5, report_gps, NULL);

/* 10 秒后停止定时器 */
timing_add(t, 10000, 0, 1, stop_cb, t);

timing_run(t);
timing_destroy(t);
```
