---
title: S-Week 2 - O_DIRECT + 持久化语义
date: 2026-07-08
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 2 - O_DIRECT + 持久化语义

> [!goal] 本周目标
> 搞清两个最容易在面试里翻车的问题：**O_DIRECT 到底绕过了什么、代价是什么**；**write 返回之后数据到底在哪、fsync 家族分别保证什么**。本周结束时要有一张 fsync 策略延迟对比表——它同时是阶段 2 `mini-kv-engine` WAL 设计的地基。

## 学习目标

1. **O_DIRECT 绕过了什么？** 绕过 page cache 直达块层；为什么数据库（如 RocksDB / InnoDB）常用它。
2. **O_DIRECT 的对齐要求是什么？** 缓冲区地址、文件偏移、长度都要对齐到逻辑块大小。
3. **write 返回意味着什么？** 只是进了 page cache（脏页），掉电会丢。
4. **fsync / fdatasync / O_SYNC / sync_file_range 的区别？** 各自把什么刷下去、各自多贵。
5. **每笔 fsync 和批量 fsync 差多少？** 这是 group commit 的动机。

## 1. O_DIRECT 读实验（Day 1-2）

### 1.1 实现

在上周 `read_latency.cpp` 基础上加 `--direct` 开关：

```cpp
// 关键点：O_DIRECT 要求 buffer 地址、offset、length 都对齐
int fd = open(path, O_RDONLY | O_DIRECT);
void* buf = nullptr;
// 逻辑块大小从 /sys/block/nvme0n1/queue/logical_block_size 读取，常见 512 或 4096
if (posix_memalign(&buf, 4096, block_size) != 0) { /* error */ }
```

用 `std::unique_ptr<void, decltype(&free)>` 或自写 RAII 包装管理对齐内存。故意做一次不对齐的调用，记录 `EINVAL` 错误——面试讲对齐要求时就有第一手证据。

### 1.2 对比矩阵

| 实验 | 预期观察 |
|---|---|
| buffered 热 cache 随机读 | 微秒级（上周数据） |
| O_DIRECT 随机读（同偏移序列） | 恒定到设备，和上周"冷 cache"接近 |
| O_DIRECT 顺序读 | 和随机读差距缩小——没有 readahead 帮忙了 |

必须回答：

- O_DIRECT 读为什么"预热"了也不变快？
- 什么场景该用 O_DIRECT（自管缓存的数据库、避免双重缓存、benchmark 要测设备本身）？
- O_DIRECT 等于"更快"吗？（不等于——它是放弃 page cache 服务，换确定性和控制权）

## 2. 持久化语义实验（Day 3-5）

### 2.1 写路径五组对比

写一个 append 写程序：4 KiB 一笔，共 10000 笔，五种策略——

| 策略 | 操作 | 保证 |
|---|---|---|
| A：仅 write | write 返回即继续 | 数据在 page cache，掉电丢 |
| B：每笔 fdatasync | write + fdatasync | 数据落盘，多数元数据不保证 |
| C：每笔 fsync | write + fsync | 数据 + 元数据落盘 |
| D：每 100 笔 fsync | 批量刷 | 组内掉电可能丢，吞吐大增 |
| E：O_SYNC 打开 | 每笔 write 自带同步 | 语义近似每笔 fsync |

记录：每笔延迟分布（p50/p99）、总吞吐（笔/秒）。预期 A 和 C 的吞吐差 1-2 个数量级。

### 2.2 必须回答

- fsync 和 fdatasync 的差别具体是什么元数据？（size 变化必须刷，mtime 可以不刷——所以 append 场景 fdatasync 并不省多少，覆盖写才省）
- D 策略丢数据的窗口有多大？这和"group commit 提升吞吐的代价"是同一个问题（[[AI Infra 存储与 GPU 数据路径系统工程师培养方案]] S2 面试题）。
- 为什么"write 返回 ≠ 落盘"设计成默认行为？（page cache 写合并、回写的吞吐收益）

### 2.3 理论配套

- OSTEP：第 39 章 Files and Directories、第 40 章 File System Implementation、第 42 章 Crash Consistency: FSCK and Journaling。
- 复习 [[4.2 重定向、同步]]，把笔记里的 sync 函数和本周数据对上。
- 延伸思考（为 S2 项目埋点）：journaling 文件系统自己也在写日志——应用层 WAL + 文件系统 journal 是两层什么关系？

## 3. 推理保温（约 25%）

- [[Week 5 - Serving Benchmark Harness]] 收尾：完成共享 prefix 场景与 `benchmark_report.md`（对应其 Day 5-7）。
- 若已完成，进入 [[Week 6 - Observability + Metrics]]：部署 vLLM OpenAI server + Prometheus 最小闭环。

## 4. 面试保底（约 15%）

- 算法（5-8 题）：双指针 / 滑动窗口。参考 [[5.2.7 双指针与滑动窗口]]，配 [[CodeTop 高频题 Top300]] 同类题。
- 八股（1 章）：RAII 与移动语义。过 [[15.8 移动语义与右值引用&&]]、[[13.2 右值引用]]。
- 项目问答：10 个 Q&A（本周素材极多：对齐、fsync 家族、掉电语义）。

## 5. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `src/read_latency.cpp`（更新） | 加 `--direct` 模式 | 对齐处理正确，EINVAL 案例有记录 |
| `src/write_sync_bench.cpp` | 五种写策略对比程序 | 策略可命令行切换 |
| `results/*.csv` | 原始数据 | 不手动修改 |
| `docs/benchmark.md`（更新） | O_DIRECT 对比表 + fsync 策略对比表 | 结论可追溯 |

## 6. 验收标准

- [ ] O_DIRECT 三组实验完成，能解释"预热无效"。
- [ ] 五种写策略对比完成，A vs C 的吞吐差有具体倍数。
- [ ] 能脱稿讲清 write → page cache → 回写 → fsync 的完整语义链。
- [ ] 不对齐 EINVAL 案例记录在案。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- write 返回后数据一定落盘了吗？掉电丢什么？
- fsync 和 fdatasync 的区别？append 场景下 fdatasync 便宜多少？
- O_DIRECT 的三个对齐要求是什么？违反了会怎样？
- 数据库为什么常用 O_DIRECT + 自管缓存？
- 每笔 fsync 的 IOPS 上限由什么决定？
- group commit 用什么换什么？

## 关联知识

- [[S-Week 1 - 环境搭建 + Page Cache 基线]]
- [[S-Week 3 - fio 对照与 Benchmark Matrix]]
- [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]]
- [[O_DIRECT 与持久化语义专题]]
- [[4.2 重定向、同步]]
- [[4.3 文件锁]]
- OSTEP Ch.39 / 40 / 42
