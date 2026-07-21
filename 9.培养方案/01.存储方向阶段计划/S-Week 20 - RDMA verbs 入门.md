---
title: S-Week 20 - RDMA verbs 入门
date: 2026-07-12
tags:
  - 高性能存储/存储方向阶段计划/计划
status: active
---

# S-Week 20 - RDMA verbs 入门

> [!goal] 本周目标
> 在 soft-RoCE 上写出 rc_pingpong 级的最小 verbs 程序，把 QP / WQE / CQ 状态机画成自己的图，跑 ibv_perftest 基准——这是国内端侧 RDMA / GPU 互联 JD 的直接证据（总纲 2026-07-09 校准加深的部分）。soft-RoCE 只讲功能路径，性能边界写死在报告里。

## 学习目标

1. **RDMA 和 TCP 数据路径的本质差异？** kernel bypass（用户态直接 ring doorbell，不经内核协议栈）+ zero-copy（网卡 DMA 直达注册内存，不经内核缓冲区拷贝）+ 传输层卸载到网卡——CPU 不再按字节付费。
2. **为什么必须内存注册（MR）？** 网卡要 DMA 就需要稳定的物理地址：注册 = pin 页 + 建立地址翻译 + 生成访问钥匙（lkey/rkey）。和 io_uring registered buffers 是同一个逻辑（[[S-Week 10 - io_uring 深入]] 直接连线）。
3. **QP 状态机怎么走？** RESET → INIT（绑端口、设访问权限）→ RTR（能收：填对端 QPN / GID / PSN）→ RTS（能发：填重传参数）。对端参数靠带外通道（TCP）交换——RDMA 自己不解决建联。
4. **一次 send/recv 的完整生命周期？** 接收端必须先 post_recv（RC 模式下没有 recv WQE 等着会 RNR）→ 发送端 post_send 生成 WQE → 网卡执行、DMA → 两端各生成 CQE → poll_cq 收割。与 NVMe SQ/CQ、io_uring SQ/CQ 同构——一张三系统对照表讲清"提交队列 + 完成队列"这个通用模式。
5. **双边和单边语义差在哪？** send/recv 双边（对端 CPU 参与 post recv）；read/write 单边（对端 CPU 完全不参与）——PD 分离的 KV transfer（Mooncake 等）用单边 write 就是这个原因。

## 1. 环境搭建（Day 1）

```bash
sudo apt install -y libibverbs-dev ibverbs-utils rdma-core perftest
sudo modprobe rdma_rxe
sudo rdma link add rxe0 type rxe netdev eth0   # 网卡名按实际
ibv_devices && ibv_devinfo -d rxe0
# 先用现成工具双端跑通：
ibv_rc_pingpong -d rxe0 -g 0            # server
ibv_rc_pingpong -d rxe0 -g 0 <server>   # client
```

## 2. 最小 verbs 程序（Day 2-4）

手写 `src/rdma_pingpong.cpp`（可对照 libibverbs 示例精读后重写，不许直接抄）：

- 流程：打开设备 → PD → 注册 MR → 创建 CQ → 创建 QP(RC) → TCP 交换 QPN/GID/rkey → 状态机迁移 INIT/RTR/RTS → post_recv / post_send → poll_cq。
- 正确性 gate：收发数据校验和对账（纪律与 S-Week 5 相同）。
- 再加一个单边变体：`IBV_WR_RDMA_WRITE` 直写对端注册内存，对端轮询尾部标志位感知到达。
- 故意制造三个错误现场并记录报错与原因：不 post_recv（RNR）、错 rkey（远端访问错）、状态机漏一步（QP 状态错）。

## 3. 基准与笔记（Day 5）

- perftest：`ib_send_lat` / `ib_send_bw` / `ib_write_lat`——soft-RoCE 上数值只用于观察相对形态，绝对值无代表性，边界声明写死。
- `docs/rdma_verbs_notes.md`：QP/WQE/CQ 状态机图（SVG 存 `图片/9.培养方案/`）+ 一次 send/recv 生命周期 + NVMe / io_uring / RDMA 三队列对照表。

> [!warning] soft-RoCE 的边界（总纲原文，必须写进报告）
> soft-RoCE 只用于理解功能路径，延迟数据没有代表性。真实 RDMA 性能需要 RoCE / IB 网卡或云裸金属，收口前可集中租一次补测。面试被问"你的 RDMA 数据真实吗"，这个边界声明就是标准答案。

## 4. 推理保温（约 25%）

- 复习 [[Week 8 - Prefill Decode + Open Source Repro]] 的 KV transfer 一节：PD 分离里 KV cache 用 RDMA 单边写搬运——本周的单边 write 实验就是它的最小版本，把这条线写进笔记。

## 5. 面试保底（约 15%）

> 阶段 2 八股按章清账第 9 讲：三种完成通知模型对照。

- 算法（5-8 题）：[[CodeTop 高频题 Top300]] 前 150 未刷高频冲刺第一轮（按频率降序补）。
- 八股（1 章）：事件通知机制。过 [[13.7 eventfd、timerfd与跨线程唤醒]]、[[13.6.1 回调函数与Reactor事件分发]]、[[13.8 连接对象生命周期与RAII]]。验收：对比"epoll 就绪通知 / io_uring CQ / RDMA CQ"三种完成模型的异同。
- 项目问答：10 个 Q&A（本周素材：MR 与 pin、状态机、单边写）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `src/rdma_pingpong.cpp` | 双边 + 单边两种模式 | 校验和对账通过 |
| `results/perftest/*` | soft-RoCE 基准（带边界声明） | 只做形态观察 |
| `docs/rdma_verbs_notes.md` | 状态机 SVG + 生命周期 + 三队列对照 | 能 3 分钟脱稿讲 |
| 错误现场记录 | 三个故意错误的报错与原因 | 报错能反推根因 |

## 7. 验收标准

- [ ] 自己的 pingpong 在 soft-RoCE 双端跑通，数据校验一致。
- [ ] 状态机每次迁移能说出"这一步填了什么参数、为什么这步才能填"。
- [ ] 双边 / 单边各跑通一种，语义差异能脱稿讲。
- [ ] 三个错误现场有记录（报错 → 原因）。
- [ ] perftest 数据有，soft-RoCE 边界声明写死。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- RDMA 和 TCP 的本质差异？零拷贝到底"零"在哪一步？
- 为什么要内存注册？和 io_uring 的 registered buffer 什么关系？
- QP 从 RESET 到 RTS，每一步在配置什么？
- RC 模式下对端没 post recv 会发生什么？
- 单边 write 对端怎么知道数据到了？

## 关联知识

- [[S-Week 19 - NVMe-oF TCP 与延迟分解]]
- [[S-Week 21 - NVMe-oF RDMA 与 SPDK]]
- [[S-Week 20 - 前置知识 - RDMA verbs 入门]]
- [[RDMA verbs 专题 - QP WQE CQ 状态机]]
- [[soft-RoCE 与实验真实性边界专题]]
- [[S-Week 10 - io_uring 深入]]（registered buffers 同源）
- [[Week 8 - Prefill Decode + Open Source Repro]]（KV transfer 用单边写）
- RDMA Aware Networks Programming User Manual；perftest 文档
