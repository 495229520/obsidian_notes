---
title: io_uring 异步 IO 专题
date: 2026-07-11
tags:
  - 高性能存储
  - 存储方向专题清单
roadmap_week: 阶段 0-1（S-Week 5、S-Week 10）
sort_order: "01.20"
status: active
---

# io_uring 异步 IO 专题

> [!info] 所属路线
> - 培养方案阶段：阶段 0（S-Week 5 基础模型与 QD 扫描）+ 阶段 1（S-Week 10 高级特性消融）
> - 排序：01.20
> - 用途：把 io_uring 讲成一个体系：为什么需要它、SQ/CQ 模型、四个高级特性各消掉哪项开销、与 epoll / AIO 的对照。存储岗和网络岗共用的高频考点。

> [!goal] 目标
> 能回答三层问题：模型层（就绪 vs 完成）、开销层（一笔异步 I/O 的成本构成与各特性的削减点）、选型层（什么场景 io_uring 真赢、什么场景 epoll 仍是正解）。

---

## 1. 为什么需要 io_uring：同步路径的三堵墙

1. **阻塞**：同步 pread 一次只有一个请求在途，NVMe 几十路并行队列喂不满——阶段 0 的 QD 扫描已经量化过这一点。
2. **AIO 残废**：Linux AIO 只支持 O_DIRECT、提交路径每次仍是 syscall、某些情况下会退化成同步执行，数据库之外没人用得动。
3. **syscall 与拷贝开销**：高 IOPS 下，每笔 I/O 的提交 syscall、用户页 pin/unpin、fd 查找的固定成本累积成显著的 sys CPU。

io_uring 的回答：两个共享内存环 + 批量化 + 可选地把每一项固定成本都"预付"。

## 2. SQ / CQ 模型

- **SQ（提交队列）**：应用写 SQE（读/写/fsync/accept 等操作描述），内核消费。
- **CQ（完成队列）**：内核写 CQE（res 为结果：负值是 -errno，非负是字节数），应用消费。
- 两个环都在 mmap 共享内存上，批量提交（一次 io_uring_enter 提交 N 个）+ 批量收割，syscall 数与 I/O 数解耦。
- **完成语义**：CQE 出现表示操作已完成，数据已在 buffer 里——与 epoll 的"就绪后你自己去 read"根本不同。

最小使用骨架见 [[S-Week 5 - io_uring 异步 IO]]，正确性纪律（cqe->res 检查、QD1 与 pread 对账）同见该周计划。

## 3. 一笔异步 I/O 的开销构成与四个特性

| 开销项 | 常规模式 | 消掉它的特性 | 代价 |
|---|---|---|---|
| 提交 syscall | 每批一次 io_uring_enter | SQPOLL：内核线程轮询 SQ，应用零 syscall | 常驻烧一个核；idle 后需按 NEED_WAKEUP 标志唤醒 |
| 完成中断 + 上下文切换 | 中断驱动 | IOPOLL：busy-poll NVMe 完成队列 | 吃 CPU；要求 O_DIRECT 且设备开 poll 队列 |
| 用户页 pin/unpin | 每笔 get_user_pages | registered buffers：注册时一次性固定 | 受 memlock 限额；buffer 池要预规划 |
| fd 查找与引用计数 | 每笔 fdget/fdput | registered files：注册后用索引引用 | 文件集合固定时才方便 |

记忆钩子：**四个特性 = 把四项"每笔都付"的成本改成"注册时付一次"或"专人代付"**。

预期收益形态（S-Week 10 拿本机数据验证）：低 QD 低 IOPS 时全都不明显；高 IOPS 下 registered buffers 和 SQPOLL 降 sys CPU 最显著；IOPOLL 主要收窄延迟抖动（p99），不是提升均值。

## 4. 三模型对照与选型

| 模型 | 通知语义 | 覆盖对象 | 定位 |
|---|---|---|---|
| epoll + 非阻塞 read | 就绪通知 | 网络/管道 fd（普通文件恒"就绪"，无意义） | Reactor 生态标配 |
| Linux AIO | 完成通知 | 仅 O_DIRECT 文件 | 历史方案 |
| io_uring | 完成通知 | 文件 + 网络统一 | 高 QD 存储、syscall 敏感场景 |

选型决策（面试常追问"为什么不全上 io_uring"）：

- **存储高 QD**：io_uring 无争议——同步模型根本发不出并发。
- **网络海量连接、低频交互**：epoll 仍是主流。就绪模型配 Reactor（[[13.6 Reactor模式与EventLoop]]）生态成熟；io_uring 需要为每个潜在读预挂 buffer，内存与编程模型代价不小。
- **网络高吞吐代理/存储后端网络**：io_uring 批量与零 syscall 有实测收益，逐步渗透中。

把 select → poll → epoll → io_uring 讲成一条演进线（每代解决上一代什么问题），底稿在 [[13.5 串讲]] + S-Week 10 的 `io_models.md` v2。

## 5. 面试口述模板

```text
io_uring 是提交/完成两个共享内存环，批量提交批量收割，把 syscall
数和 I/O 数解耦，语义是完成通知，文件和网络统一覆盖。它和 epoll
的本质区别是就绪 vs 完成：epoll 告诉你能读了、数据还要自己拷，
io_uring 通知时数据已经就位；epoll 对普通文件无意义，因为普通文
件永远就绪。高级特性我做过消融实验：registered buffers 省每笔的
页固定、SQPOLL 省提交 syscall、IOPOLL 用 CPU 换掉完成中断，收益
都要在高 IOPS 下才显现，低 QD 时和裸模式几乎没差别——所以选型上
存储高并发我用 io_uring，海量连接低频网络我仍选 epoll。
```

追问预案：

- "SQPOLL 线程睡了怎么办？" → SQ ring 的 flags 置 NEED_WAKEUP，下次提交需带 IORING_ENTER_SQ_WAKEUP 唤醒；liburing 的 submit 已封装。
- "IOPOLL 为什么必须 O_DIRECT？" → poll 的是设备完成队列；buffered 路径的完成点在 page cache 拷贝，不存在可 poll 的设备事件。
- "registered buffer 快在哪？" → 省掉每笔 I/O 的 get_user_pages/put_page（页表遍历 + 引用计数 + 可能的缺页），高 IOPS 下这是可测量的 sys CPU。
- "io_uring 有什么安全争议？" → 曾是内核漏洞高发区，部分托管环境（如某些容器平台）默认用 seccomp 禁用；生产选型要确认平台策略。

## 关联知识

- [[S-Week 5 - io_uring 异步 IO]]（基础模型、QD 扫描、正确性 gate）
- [[S-Week 10 - io_uring 深入]]（特性消融实验）
- [[S-Week 10 - 前置知识 - io_uring 深入]]（特性的内核机制与环境预检）
- [[13.4 epoll模型]]、[[13.5 串讲]]、[[13.6 Reactor模式与EventLoop]]（就绪模型一侧）
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]（io_uring 在全路径中的位置）
- [[00.存储方向专题清单索引]]
