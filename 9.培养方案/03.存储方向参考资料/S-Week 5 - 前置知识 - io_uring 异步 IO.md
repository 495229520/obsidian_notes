---
title: S-Week 5 - 前置知识 - io_uring 异步 IO
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向参考资料
  - 计划
aliases:
  - 存储 Week 5 前置知识
  - io_uring 前置知识
status: active
---

# S-Week 5 - 前置知识 - io_uring 异步 IO

## 索引

- [[#0. 先建立直觉：同步 I/O 为什么喂不饱 NVMe]]
- [[#1. SQ / CQ 双环模型]]
- [[#2. liburing 最小 API 集]]
- [[#3. cqe->res：错误与短读处理]]
- [[#4. 维持稳定 QD：收割一个补一个]]
- [[#5. 逐笔计时：user_data 时间戳]]
- [[#6. 正确性 gate：QD1 与 pread 对账]]
- [[#7. QD 扫描实验设计]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 5 - io_uring 异步 IO]] 的总前置知识：写代码前通读 0-2 节建立模型，实现时对照 3-5 节（这三节是 bug 高发区），跑实验前看 6-7 节。模型对照与高级特性见 [[io_uring 异步 IO 专题]]；SQPOLL 等深入特性是阶段 1 [[S-Week 10 - io_uring 深入]] 的内容，本周不碰。

---

> 前四周的读路径全是同步的：一个线程同一时刻只有一笔在途 I/O。但你的 NVMe 有几十条硬件队列，QD1 只用了它百分之几的并行度。本周用 io_uring 让单线程堆出 QD32，亲眼看到 IOPS 翻几倍——以及 p99 为此付出的代价。

---

## 0. 先建立直觉：同步 I/O 为什么喂不饱 NVMe

S-Week 2 的 O_DIRECT 随机读约几十微秒一笔，同步循环的吞吐上限：

$$
IOPS_{sync} = \frac{1}{Latency}
$$

延迟 50 µs 时单线程只有 2 万 IOPS，而盘的标称随机读可能是几十万——设备在等你，不是你在等设备。解法只有两个：多线程（贵、难扩展）或异步（单线程维持 N 笔在途）。io_uring 就是 Linux 上异步这条路的现代答案。

## 1. SQ / CQ 双环模型

```text
应用                          内核
 │ 写 SQE（操作描述）           │
 ├──→ SQ ring（共享内存）──────→ 消费 SQE，发起 I/O
 │                             │
 │ ←──  CQ ring（共享内存）←──── 完成后写 CQE（结果）
 │ 读 CQE                      │
```

- **SQE**：一个操作的描述（读哪个 fd、哪个偏移、多长、读进哪个 buffer）。
- **CQE**：一个完成事件（res 结果码 + user_data 回传字段）。
- 两个环都在 mmap 共享内存上：提交 N 笔可以只用一次 `io_uring_enter` syscall，收割可以完全不用 syscall（直接读共享内存）。
- **完成是乱序的**：先提交的不一定先完成——这是异步的本质，也是第 5 节 user_data 存在的理由。

## 2. liburing 最小 API 集

裸 io_uring 系统调用极难用，liburing 是官方封装（作者就是 io_uring 作者）。本周只需要七个函数：

```cpp
io_uring ring;
io_uring_queue_init(entries, &ring, 0);          // entries ≥ 目标 QD，2 的幂

io_uring_sqe* sqe = io_uring_get_sqe(&ring);     // 取一个空 SQE 槽
io_uring_prep_read(sqe, fd, buf, len, offset);   // 填成一个 pread 请求
io_uring_sqe_set_data64(sqe, my_tag);            // 附带 8 字节自定义数据

io_uring_submit(&ring);                          // 提交所有已填 SQE（一次 syscall）

io_uring_cqe* cqe;
io_uring_wait_cqe(&ring, &cqe);                  // 阻塞等一个完成
uint64_t tag = io_uring_cqe_get_data64(cqe);     // 取回自定义数据
// 检查 cqe->res（第 3 节）
io_uring_cqe_seen(&ring, cqe);                   // 归还 CQE 槽位——不调用会撑爆 CQ
```

安装与版本：`sudo apt install -y liburing-dev`，内核要求 5.15+（S-Week 1 环境验证已确认）。O_DIRECT 模式下每个在途请求需要**独立的对齐 buffer**（S-Week 2 的 posix_memalign 包装直接复用，做成 QD 个 buffer 的池）。

## 3. cqe->res：错误与短读处理

`cqe->res` 是本周程序正确性的命门，三种取值：

| res | 含义 | 处理 |
|---|---|---|
| < 0 | 错误，值为 -errno | 打印 `-res` 对应的错误并终止/记录（EINVAL 多半是对齐错） |
| == 请求长度 | 完整读成功 | 正常路径 |
| 0 到请求长度之间 | **短读** | 基准场景按错误处理（说明偏移越界或被截断），不能当成功计入延迟 |

benchmark 程序的纪律：偏移生成时保证 `offset + block_size <= file_size`（对齐到块边界，同前几周），短读就不该出现；一旦出现说明有 bug，让它大声失败而不是悄悄污染数据。

## 4. 维持稳定 QD：收割一个补一个

QD 扫描要求"整个测量窗口内在途请求数恒为 QD"，标准做法两阶段：

```text
灌满阶段：连续填 QD 个 SQE，一次 submit —— 在途 = QD
稳态循环：wait_cqe 收一个 → 记延迟 → 立刻填一个新 SQE、submit
          —— 在途始终在 QD 与 QD-1 之间抖动，近似恒定
排水阶段：不再补新请求，收完剩余 QD 个 —— 排水期数据不计入统计
```

两个计数器别混：**已提交数**和**已完成数**，结束条件看完成数。排水期（和灌满期）延迟形态与稳态不同，掐头去尾只统计稳态窗口——与 fio 的 ramp_time 同理。

## 5. 逐笔计时：user_data 时间戳

完成乱序意味着**不能用"第几个完成"去对"第几个提交"**。逐笔延迟的标准做法：提交时把时间戳塞进 user_data，完成时取出来相减：

```cpp
io_uring_sqe_set_data64(sqe, now_ns());              // 提交时刻（steady_clock）
// ...
uint64_t submit_ns = io_uring_cqe_get_data64(cqe);
record(now_ns() - submit_ns);                        // 该笔的完成延迟
```

- 时钟仍用 `std::chrono::steady_clock`（S-Week 1 纪律）。
- 时间戳要在**填 SQE 的当下**打，且批量提交时逐个打——攒一批再统一打会把排队时间算漏。
- 需要同时回传"buffer 编号 + 时间戳"时，用索引进数组的方式（user_data 存索引，时间戳和 buffer 指针放在数组元素里）。

## 6. 正确性 gate：QD1 与 pread 对账

沿用"correctness 先于 benchmark"的推理版纪律，性能实验前必须通过：

1. 固定 seed 生成同一串偏移序列。
2. 同一文件分别跑：同步 pread 版（S-Week 2 程序）与 io_uring QD1 版。
3. 两边对每笔数据算校验和，**逐笔比对全等**才算通过。

QD1 的意义：排除并发因素，纯验证"io_uring 读回来的字节和 pread 一样"。gate 过了以后所有 QD 下只需抽查校验和总和。这一步能抓住的典型 bug：buffer 池索引错乱、偏移生成器状态被并发路径共享、短读被静默忽略。

## 7. QD 扫描实验设计

- 负载：4K 随机读 + O_DIRECT（排除 page cache 干扰，理由见 [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]]）。
- QD 序列：1 / 2 / 4 / 8 / 16 / 32 / 64 / 128，每组 3 次，逐笔 CSV。
- 三条产出曲线：QD→IOPS（找饱和点）、QD→p50/p99（找起飞点）、与 fio `ioengine=io_uring` 同参数对账（[[S-Week 3 - fio 对照与 Benchmark Matrix]] 的对账方法原样复用）。
- 解读框架就是 Little's law 三段论（线性段 → 饱和点 → 排队段），预习见 [[S-Week 3 - 前置知识 - fio 对照与 Benchmark Matrix]] 第 4 节——本周你自己的程序要复现出 fio 曾给你看过的形状。

## 8. 常见错误

- **忘记 io_uring_cqe_seen**：CQ 环填满后 wait_cqe 永久阻塞——"程序卡死"第一嫌疑人。
- **多个在途请求共用一个 buffer**：数据互相踩踏，延迟正常、数据全错——正确性 gate 就是为它设的。
- **用完成顺序对应提交顺序**：完成乱序，延迟算出来忽大忽小甚至为负。
- **时间戳在 submit 之后才打**：漏掉内核排队段，QD 越深误差越大。
- **排水期数据混进统计**：尾部延迟被稀释，p99 偏乐观。
- **entries 小于目标 QD**：get_sqe 返回 nullptr 没检查，段错误或静默丢请求。
- **忽略 io_uring_submit 返回值**：它返回实际提交个数，和你以为的不一致时后续计数全乱。
- **EINVAL 查半天代码**：先查三对齐（buffer/offset/length），O_DIRECT 的老朋友。

## 9. 学习检查清单

- [ ] 能画出 SQ/CQ 双环并解释谁写谁读、为什么批量提交省 syscall。
- [ ] 能默写七个 liburing 函数的调用顺序。
- [ ] 能说出 cqe->res 三种取值的处理方式。
- [ ] 能解释"收割一补一"如何维持稳定 QD，以及掐头去尾的原因。
- [ ] 能解释完成乱序为什么必须用 user_data 传时间戳。
- [ ] 能复述正确性 gate 的三步和它能抓住的 bug 类型。
- [ ] 知道本周实验为什么固定 O_DIRECT。

## 10. 关键要点总结

- 同步 I/O 的吞吐被延迟锁死，io_uring 用双环 + 批量把 syscall 数与 I/O 数解耦，单线程堆出深队列。
- 完成是乱序的：user_data 是把提交上下文带到完成侧的唯一正道。
- cqe->res 三分支（负 errno / 完整 / 短读）一个都不能少；短读在本基准中等于 bug。
- 稳定 QD 靠收割一补一，统计只取稳态窗口。
- correctness gate（QD1 vs pread 逐笔对账）先于一切性能数字——这条纪律阶段 1 消融实验还会再用。

## 关联知识

- [[S-Week 5 - io_uring 异步 IO]]（本篇服务的周计划）
- [[io_uring 异步 IO 专题]]（模型对照与高级特性总览）
- [[S-Week 10 - 前置知识 - io_uring 深入]]（阶段 1：SQPOLL/IOPOLL/registered buffers）
- [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]]（对齐 buffer 池的来源）
- [[S-Week 3 - 前置知识 - fio 对照与 Benchmark Matrix]]（Little's law 与对账方法）
- [[13.4 epoll模型]]（就绪模型对照，本周 `io_models.md` 要用）

## 参考

- liburing（axboe/liburing）：README 与 examples 目录
- `man io_uring_enter`、`man io_uring_setup`（模型权威语义）
- Efficient IO with io_uring（Jens Axboe，随 liburing 分发）
- Lord of the io_uring（unixism.net 教程，入门友好）
