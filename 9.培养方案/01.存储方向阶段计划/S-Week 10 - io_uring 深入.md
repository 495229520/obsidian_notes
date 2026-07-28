---
title: S-Week 10 - io_uring 深入
date: 2026-07-11
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 10 - io_uring 深入

> [!goal] 本周目标
> 把 S-Week 5 的 io_uring 程序升级成特性消融实验台：SQPOLL、IOPOLL、registered buffers/files 各开各关，量化每个特性"省了什么、烧了什么"，并把 epoll vs io_uring 从一张对照表升级成能应对追问的体系化答案。存储岗问深度、网络岗问模型，这周的产出两边都用。

## 学习目标

1. **常规模式的每笔 I/O 开销由什么构成？** 提交 syscall + 用户 buffer 的页 pin/unpin + fd 查找与引用计数 + 完成中断与上下文切换。四个高级特性各消掉其中一项。
2. **SQPOLL 省了什么、烧了什么？** 内核轮询线程代收提交，应用侧零 syscall；代价是常驻烧一个核，且空闲超时（`sq_thread_idle`）后要检查唤醒标志。
3. **IOPOLL 是什么？** 完成侧不等中断，busy-poll NVMe 完成队列；延迟更低更稳，但要求 O_DIRECT，且吃 CPU。
4. **registered buffers/files 省的具体是什么？** 一次性注册并固定内存/文件引用，省掉每笔 I/O 的 get_user_pages 和 fd 查找。
5. **网络服务器为什么很多仍用 epoll？** 就绪模型配非阻塞 socket 生态成熟（Reactor、定时器、跨线程唤醒），io_uring 的收益要在高 IOPS 存储或极致 syscall 敏感场景才显著。

## 1. 程序升级（Day 1-2）

给 `uring_read.cpp` 加四个开关（可组合）：

```text
--sqpoll          IORING_SETUP_SQPOLL（注意 liburing 已处理 NEED_WAKEUP）
--iopoll          IORING_SETUP_IOPOLL（必须配 O_DIRECT，设备需支持 poll 队列）
--fixed-buffers   io_uring_register_buffers + prep_read_fixed
--fixed-files     io_uring_register_files
```

正确性 gate 不放松：每种模式 QD1 与同步 pread 校验和逐字节对账，全部通过才进入性能实验。

环境预检写进 `env.md`：内核版本（SQPOLL 无特权使用需 5.11+）、`ulimit -l`（registered buffers 受 memlock 限额约束）、NVMe poll 队列是否开启（`nvme.poll_queues` 模块参数，IOPOLL 依赖它）。

## 2. 消融矩阵（Day 3-4）

负载固定：4K 随机读、O_DIRECT、QD 取 1 / 8 / 32，每组 3 次。

| 配置 | 关注问题 |
|---|---|
| baseline（S-Week 5 原版） | 参照系 |
| + fixed buffers | sys CPU 降多少？IOPS 涨多少？ |
| + fixed files | 单开收益是不是最小？ |
| + SQPOLL | syscalls/s 是否趋近 0？多烧的那个核值不值？ |
| + IOPOLL | p99 和延迟抖动改善多少？CPU 换到了哪里？ |

每组必记三类指标，缺一不可：

- 吞吐与延迟：IOPS、p50 / p99。
- CPU 成本：user / sys 时间、SQPOLL 线程的核占用（`pidstat -t`）。
- syscall 计数：`perf stat -e raw_syscalls:sys_enter` 或对照组 `strace -c` 短窗口采样（strace 本身拖慢程序，只用来数相对比例，不进性能报告）。

预期形态（拿数据验证，不符合就解释）：低 QD 时各配置差异小；QD32 高 IOPS 下 fixed buffers 和 SQPOLL 的 sys CPU 明显下降；IOPOLL 主要改善 p99 抖动而不是均值。

## 3. io_models.md 升级 v2（Day 5）

在 S-Week 5 的三模型对照表基础上加两节：

1. **特性消融结论**：每个特性一行——消掉的开销、实测收益（本机数据）、代价、什么场景开。
2. **epoll vs io_uring 决策矩阵**：网络低连接数 / 网络海量连接 / 存储低 QD / 存储高 QD 四象限，各自选什么、为什么。与 [[13.4 epoll模型]]、[[13.6 Reactor模式与EventLoop]]、[[13.5 串讲]] 互链，把 select → poll → epoll → io_uring 的演进故事补上最后一环。

## 4. 推理保温（约 25%）

- 维护态：KV cache 三道口算保持手感（每 token KV 字节数、prefix cache 命中收益、offload 读回临界点），harness 数据抽查一次。

## 5. 面试保底（约 15%）

> 阶段 1 Linux 补强：网络两讲之后回到进程线程，这也是 io_uring SQPOLL（内核线程）和 Reactor（IO 线程）话题的底层依托。

- 算法（5-8 题）：堆与贪心。参考 [[5.2.11 堆与优先队列]]、[[5.2.17 贪心算法]]（Top-K、区间调度类），配 [[CodeTop 高频题 Top300]] 同类题。
- 八股（1 章）：进程与线程。过 [[11.1 进程简介]]、[[11.2 进程间通信IPC]]、[[11.7 僵尸进程]]、[[12.1 线程简介]]。验收：能对比进程/线程/协程的切换成本与隔离性，能讲清 fork 后发生了什么。
- 项目问答：10 个 Q&A（本周素材：每个特性省的开销、CPU 与延迟的交换、决策矩阵）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `src/uring_read.cpp`（v2） | 四特性开关可组合 | 每种模式正确性 gate 通过 |
| `results/ablation/*.csv` | 消融矩阵原始数据（含 CPU、syscall 计数） | 不手动修改 |
| `docs/ablation_report.md` | 消融结论表 + 曲线 | 每个特性的收益/代价有数据 |
| `docs/io_models.md`（v2） | 加消融结论 + 决策矩阵 | 能作为面试底稿 |

## 7. 验收标准

- [ ] 四个开关实现完成，所有模式正确性对账通过。
- [ ] 消融矩阵跑完，三类指标齐全。
- [ ] 每个特性"省了什么、烧了什么"有本机数据支撑。
- [ ] IOPOLL 的环境依赖（O_DIRECT、poll 队列）在 env.md 有记录；跑不通时有明确的边界声明而不是硬凑数据。
- [ ] `io_models.md` v2 完成，能应对"为什么不全上 io_uring"的追问。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- SQPOLL 线程空闲睡眠后，应用怎么知道要唤醒它？
- IOPOLL 为什么要求 O_DIRECT？buffered 路径为什么没法 poll？
- registered buffer 省掉的 get_user_pages 具体贵在哪？
- 你的消融实验里哪个特性收益最大？在什么 QD 下？为什么低 QD 时都不明显？
- 一个百万连接的网关，你会用 io_uring 重写吗？说理由。

## 关联知识

- [[S-Week 9 - eBPF 观测]]
- [[S-Week 11 - 完整版收口]]
- [[S-Week 10 - 前置知识 - io_uring 深入]]
- [[io_uring 异步 IO 专题]]
- [[S-Week 5 - io_uring 异步 IO]]（baseline 程序与 QD 扫描）
- liburing（axboe/liburing）man pages：io_uring_setup(2)、io_uring_register(2)
