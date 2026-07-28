---
title: S-Week 10 - 前置知识 - io_uring 深入
date: 2026-07-11
tags:
  - 高性能存储
  - 存储方向参考资料
  - 计划
aliases:
  - 存储 Week 10 前置知识
  - io_uring 深入前置知识
status: active
---

# S-Week 10 - 前置知识 - io_uring 深入

## 索引

- [[#0. 先建立直觉：一笔异步 I/O 到底花钱在哪]]
- [[#1. SQPOLL：把提交 syscall 外包给内核线程]]
- [[#2. IOPOLL：把完成中断换成忙轮询]]
- [[#3. registered buffers / files：把每笔的固定成本预付]]
- [[#4. 环境预检：跑不通往往不是代码错]]
- [[#5. CPU 与 syscall 怎么计量]]
- [[#6. 消融实验设计原则]]
- [[#7. 常见错误]]
- [[#8. 学习检查清单]]
- [[#9. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 10 - io_uring 深入]] 的总前置知识：改代码前读 0-3 节理解每个特性的机制，跑实验前必读第 4 节环境预检和第 5-6 节计量方法。S-Week 5 的 SQ/CQ 基础模型不再重复，忘了先回看 [[S-Week 5 - io_uring 异步 IO]]；体系化对照见 [[io_uring 异步 IO 专题]]。

---

> S-Week 5 证明了 io_uring 能"堆深度吃满设备"；本周回答的是另一个问题：**高 IOPS 下 CPU 都花在哪，io_uring 的高级特性分别能省回来多少**。这周的关键纪律是：每个收益都必须和它的代价一起报告。

---

## 0. 先建立直觉：一笔异步 I/O 到底花钱在哪

常规 io_uring 模式下，每笔（或每批）I/O 的成本清单：

| 成本项 | 发生在 | 谁付 |
|---|---|---|
| io_uring_enter syscall | 每批提交 | 应用线程（用户态↔内核态切换） |
| 用户 buffer 页固定（get_user_pages） | 每笔 I/O | 内核（页表遍历 + 引用计数） |
| fd 查找与引用计数（fdget/fdput） | 每笔 I/O | 内核 |
| 完成中断 + 上下文切换 | 每笔完成 | 内核 + 被打断的 CPU |

低 IOPS 时这些都是零头；到几十万 IOPS 量级，每项都会在 `sys` CPU 里现形。四个特性就是逐项削减这张清单——所以消融实验的预期是：**QD 低看不出差别是正常结果，不是实验失败**。

## 1. SQPOLL：把提交 syscall 外包给内核线程

- `IORING_SETUP_SQPOLL`：内核起一个轮询线程盯着 SQ，应用只管往共享内存环里写 SQE，理想情况下**提交路径零 syscall**。
- 空闲控制：`sq_thread_idle`（毫秒）内没有新提交，轮询线程睡眠；此后 SQ ring flags 会置 `IORING_SQ_NEED_WAKEUP`，下一次提交需要带唤醒标志的 io_uring_enter。**liburing 的 io_uring_submit 已封装这个检查**，自己裸写才需要处理。
- 代价：轮询线程常驻烧核。报告 CPU 时必须把这个线程算进去（`pidstat -t` 能看到独立的 iou-sqp 线程）。
- 权限：内核 5.11 起无需特权即可用；更老内核要 CAP_SYS_NICE/root——预检时确认内核版本。

## 2. IOPOLL：把完成中断换成忙轮询

- `IORING_SETUP_IOPOLL`：完成侧不等设备中断，由调用 io_uring_enter 等待的线程直接轮询 NVMe 完成队列。
- 消掉的是**中断 + 唤醒 + 上下文切换**的延迟与抖动——所以预期改善主要在 p99 的稳定性，不是均值的大幅下降。
- 两个硬前提，缺一就是 -EINVAL / -EOPNOTSUPP：
  1. 文件必须 O_DIRECT 打开（buffered 路径的完成点在 page cache 拷贝，没有可轮询的设备事件）。
  2. 设备要有 poll 队列：NVMe 驱动模块参数 `nvme.poll_queues` 大于 0（`cat /sys/module/nvme/parameters/poll_queues` 检查；改了要重载驱动或加内核启动参数）。
- SQPOLL 与 IOPOLL 可以叠加，但叠加后是"双烧核"配置，报告里单独标注。

## 3. registered buffers / files：把每笔的固定成本预付

```cpp
// 注册一次
io_uring_register_buffers(&ring, iovecs, n);   // 页固定从每笔挪到注册时
io_uring_register_files(&ring, fds, n);        // fd 引用从每笔挪到注册时
// 之后用 fixed 变体 + 索引
io_uring_prep_read_fixed(sqe, file_index, buf, len, off, buf_index);
```

- registered buffers 省的是每笔 get_user_pages/put_page；buffer 池必须预分配、大小固定——正好和 O_DIRECT 的对齐 buffer 池是同一套设计。
- registered files 省 fdget/fdput，单文件基准下收益通常最小——预期它是消融矩阵里"垫底"的特性，验证这个预期本身就是结论。
- 限额：固定的内存计入 memlock（内核版本不同记账方式有差异），注册失败报 ENOMEM 先查 `ulimit -l`。

## 4. 环境预检：跑不通往往不是代码错

写进 `env.md` 再动手：

```bash
uname -r                                          # >= 5.11：SQPOLL 无特权
ulimit -l                                         # memlock 限额（registered buffers）
cat /sys/module/nvme/parameters/poll_queues       # > 0 才能 IOPOLL
liburing 版本（dpkg -l liburing-dev）             # 特性接口随版本变化
```

> [!warning] IOPOLL 跑不通怎么办
> 云主机上 poll_queues 常常是 0 且未必有权限改。跑不通时的正确姿势是**边界声明**：报告里写明"IOPOLL 因环境不支持未测，机制分析如下"——和 soft-RoCE 的"功能级 vs 性能级"三分法同一纪律。硬凑一组不可信数据是最差选择。

## 5. CPU 与 syscall 怎么计量

| 指标 | 工具 | 注意 |
|---|---|---|
| 进程 user/sys CPU | `pidstat 1` 或 /proc/pid/stat 前后差 | 与实验窗口对齐 |
| SQPOLL 线程占用 | `pidstat -t`（线程视图） | iou-sqp 线程单列 |
| syscalls/s | `perf stat -e raw_syscalls:sys_enter -p <pid>` | 低开销，可全程挂 |
| syscall 构成 | `strace -c` 短窗口 | strace 严重拖慢目标，只看相对构成，数据不进性能报告 |

报告纪律：每个配置一行，IOPS / p99 / user CPU / sys CPU / 轮询线程 CPU / syscalls-per-IO 六列齐全。"IOPS 涨了 10% 但多烧一个核"必须能从表里直接读出来。

## 6. 消融实验设计原则

- **一次加一个特性**：baseline → +fixed buffers → +sqpoll → +iopoll，不许一把梭；组合项（如 sqpoll+fixed）放在单项都测完之后。
- **正确性 gate 每个模式各过一次**：QD1 与 pread 校验和对账。fixed buffer 的索引写错、IOPOLL 的短读处理，都是"性能正常、数据错了"的典型来源。
- **同一物理环境一口气跑完**：跨天跑消融，环境漂移会淹没特性差异；跑不完就整轮重跑。
- 每组 3 次重复、逐笔延迟进 CSV、原始数据不改——阶段 0 纪律原样沿用。

## 7. 常见错误

- **SQPOLL 下用裸 io_uring_enter 忘了 NEED_WAKEUP**：轮询线程睡了，提交无人消费，程序"卡死"——用 liburing 的 submit 或检查 flags。
- **IOPOLL 配 buffered 文件**：直接报错；错误处理没打印 -cqe->res 的话会误以为是别的问题。
- **registered buffer 索引与 iovec 对不上**：读到别的 buffer 里，校验和 gate 能抓住——这就是 gate 不能省的原因。
- **strace 挂着跑性能数据**：syscall 被放大几十倍，整组数据作废。
- **CPU 只报进程不报轮询线程**：SQPOLL"零 syscall"的营销数字背后烧的核被藏掉，报告可信度归零。
- **在 poll_queues=0 的机器上"测出" IOPOLL 收益**：设置没生效而不自知，数据是常规路径的复测。预检命令写进脚本，跑前自动校验。

## 8. 学习检查清单

- [ ] 能默写常规模式每笔 I/O 的四项成本，以及四个特性各消哪项。
- [ ] 能解释 SQPOLL 的 NEED_WAKEUP 机制和 CPU 代价的计量方式。
- [ ] 能说出 IOPOLL 的两个硬前提及跑不通时的边界声明写法。
- [ ] 能解释 registered buffers 与 O_DIRECT buffer 池的设计契合点。
- [ ] 知道 strace 与 perf stat 在 syscall 计量上的分工。
- [ ] 能复述消融实验的"一次一个变量 + 每模式过 gate"原则。

## 9. 关键要点总结

- 高级特性的本质：把"每笔都付"的固定成本改成"注册时付一次"或"专人代付"。
- 收益只在高 IOPS 显形，低 QD 无差别是正常结论。
- 每个收益必须与代价同表呈现：烧核的线程、memlock、环境依赖。
- 环境预检先行：内核版本、ulimit -l、poll_queues；跑不通写边界声明，不硬凑。
- 正确性 gate 每模式一遍，性能数据才有资格被讨论。

## 关联知识

- [[S-Week 10 - io_uring 深入]]（本篇服务的周计划）
- [[S-Week 5 - io_uring 异步 IO]]（SQ/CQ 基础与 baseline 程序）
- [[io_uring 异步 IO 专题]]（体系化对照与面试口述）
- [[13.4 epoll模型]]、[[13.6 Reactor模式与EventLoop]]（就绪模型一侧）
- [[S-Week 2 - O_DIRECT + 持久化语义]]（对齐 buffer 池的来源）

## 参考

- `man io_uring_setup`（SQPOLL/IOPOLL flag 语义）、`man io_uring_register`
- liburing（axboe/liburing）：examples 与 test 目录是最好的用法参考
- Efficient IO with io_uring（Jens Axboe 的设计文档，随 liburing 仓库分发）
- Systems Performance ch 6 CPUs（CPU 计量方法）
