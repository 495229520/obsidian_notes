---
title: S-Week 5 - io_uring 异步 IO
date: 2026-07-08
tags:
  - 高性能存储/存储方向阶段计划/计划
status: active
---

# S-Week 5 - io_uring 异步 IO

> [!goal] 本周目标
> 用 liburing 写出第一个异步 I/O 程序，扫出 iodepth-IOPS-p99 曲线，并能把 io_uring 的完成模型和 epoll 的就绪模型讲成对照——这是存储岗和网络岗都爱问的题。MVP 的最后一块拼图。

## 学习目标

1. **io_uring 的 SQ / CQ 模型是什么？** 提交队列 + 完成队列两个共享内存环，批量提交、批量收割。
2. **它相比同步 pread 解决了什么？** 单线程堆队列深度，让 NVMe 的并行队列吃满。
3. **它相比 epoll + read 解决了什么？** epoll 是就绪通知（对普通文件无意义），io_uring 是完成通知，统一覆盖文件与网络。
4. **它相比 Linux AIO 解决了什么？** AIO 只支持 O_DIRECT 且提交路径贵；io_uring 无此限制、支持批量与零 syscall 模式。
5. **QD 扫描曲线怎么解读？** 吞吐饱和点、p99 起飞点、二者之间的最优运营区间。

## 1. liburing 最小程序（Day 1-2）

安装：`sudo apt install -y liburing-dev`（确认 `uname -r` >= 5.15）。

核心骨架（随机读，QD 可配）：

```cpp
io_uring ring;
io_uring_queue_init(queue_depth, &ring, 0);

// 灌满队列：每个 slot 一个对齐 buffer + 一个随机偏移
for (int i = 0; i < queue_depth; ++i) {
    io_uring_sqe* sqe = io_uring_get_sqe(&ring);
    io_uring_prep_read(sqe, fd, bufs[i], block_size, next_offset());
    io_uring_sqe_set_data64(sqe, submit_time_ns());
}
io_uring_submit(&ring);

// 收割一个、补一个，维持稳定 QD
io_uring_cqe* cqe;
io_uring_wait_cqe(&ring, &cqe);
// cqe->res 必须检查：< 0 是 -errno，>= 0 是读到的字节数
record_latency(now_ns() - io_uring_cqe_get_data64(cqe));
io_uring_cqe_seen(&ring, cqe);
```

正确性验证：QD=1 时与同步 pread 的数据逐字节比对（同一偏移序列、同一校验和），确认无误后才做性能实验——沿用推理版"correctness 先于 benchmark"的 gate。

## 2. QD 扫描实验（Day 3-4）

### 2.1 矩阵

- 负载：4K 随机读，O_DIRECT（排除 page cache 干扰）。
- QD：1 / 2 / 4 / 8 / 16 / 32 / 64 / 128。
- 每组 3 次重复，逐笔延迟进 CSV。

### 2.2 三条曲线

1. QD → IOPS：找吞吐饱和点。
2. QD → p50 / p99：找延迟起飞点。
3. 与 [[S-Week 3 - fio 对照与 Benchmark Matrix]] 中 fio `ioengine=io_uring` 同参数结果对账。

### 2.3 必须回答

- IOPS 饱和之后继续加 QD，p99 为什么还在涨？（排队论：设备满了，多出来的请求全在排队）
- 你的盘的"甜点 QD"是多少？给延迟 SLO（如 p99 < 1ms）时怎么反推 QD 上限？
- QD1 的 io_uring 和同步 pread 谁快？为什么差距不大？（单请求路径长度相近，io_uring 赢在并发）

## 3. 模型对照（Day 5，写成笔记）

写 `docs/io_models.md`，三段对照：

| 模型 | 通知语义 | 适用 | 关键笔记 |
|---|---|---|---|
| epoll + read | 就绪通知（readiness） | 网络 fd | [[13.4 epoll模型]]、[[13.6 Reactor模式与EventLoop]] |
| Linux AIO | 完成通知，限 O_DIRECT | 老式数据库 | — |
| io_uring | 完成通知（completion），文件+网络统一 | 高 QD 存储、现代 server | 本周实验 |

提一句留待阶段 1 深入的特性：SQPOLL（内核轮询线程，零 syscall 提交）、registered buffers / files（省去每次映射开销）——本周不做。

## 4. 推理保温（约 25%）

- [[Week 7 - KV Cache + Prefix Cache + Paged KV]] 上半：shared prefix workload、prefix cache on/off benchmark。
- 交叉思考：paged KV cache 的 block table 和文件系统的块分配是同构问题——把这个类比写进笔记，S4 和面试都用得上。

## 5. 面试保底（约 15%）

- 算法（5-8 题）：栈与单调结构。参考 [[5.2.10 栈与单调结构]]，配 [[CodeTop 高频题 Top300]] 同类题。
- 八股（1 章）：并发同步。过 [[17.2 原子性、可见性与内存序]]、[[12.3 互斥量]]。
- 项目问答：10 个 Q&A。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `src/uring_read.cpp` | liburing 随机读程序（QD 可配） | QD1 正确性对账通过 |
| `results/qd_sweep/*.csv` | 扫描原始数据 | 不手动修改 |
| `docs/qd_sweep_report.md` | 三条曲线 + 甜点 QD 分析 | 拐点有标注有解释 |
| `docs/io_models.md` | epoll / AIO / io_uring 模型对照 | 能作为面试底稿 |

## 7. 验收标准

- [ ] liburing 程序正确性 gate 通过（QD1 与 pread 逐字节一致）。
- [ ] QD 扫描完成，IOPS 饱和点与 p99 起飞点标注。
- [ ] 与 fio 对账完成，差异有解释。
- [ ] cqe->res 错误处理正确（负值当 -errno 处理，短读有处理）。
- [ ] `io_models.md` 完成，能 3 分钟脱稿讲 epoll vs io_uring。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- io_uring 的 SQ / CQ 分别是谁写谁读？
- 为什么 epoll 对普通文件没有意义？
- io_uring 相比 Linux AIO 的三个改进？
- 给定 p99 < 1ms 的 SLO，你怎么定 QD？
- 你的程序怎么保证异步读的结果是对的？

## 关联知识

- [[S-Week 4 - mmap 与读路径对比]]
- [[S-Week 6 - MVP 收口与报告]]
- [[S-Week 5 - 前置知识 - io_uring 异步 IO]]
- [[io_uring 异步 IO 专题]]
- [[13.4 epoll模型]]
- [[13.6 Reactor模式与EventLoop]]
- liburing（axboe/liburing）examples
