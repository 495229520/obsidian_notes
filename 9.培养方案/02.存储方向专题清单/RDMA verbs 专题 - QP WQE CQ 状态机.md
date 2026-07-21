---
title: RDMA verbs 专题 - QP WQE CQ 状态机
date: 2026-07-12
tags:
  - 高性能存储/存储方向专题清单
roadmap_week: 阶段 2（S-Week 20 实验主线；国内端侧 RDMA / GPU 互联 JD 的直接证据）
sort_order: "02.20"
status: active
---

# RDMA verbs 专题 - QP WQE CQ 状态机

> [!info] 所属路线
> - 培养方案阶段：阶段 2 S-Week 20（rc_pingpong 级程序 + perftest 基准）；单边 write 语义直接连到阶段 3 的 KV cache transfer
> - 排序：02.20
> - 用途：verbs 对象模型、QP 状态机、一次 send/recv 生命周期的沉淀层——2026-07-09 依国内 JD 校准加深的部分，端侧 RDMA / GPU 互联岗的面试主场。

> [!goal] 目标
> 讲清四件事：RDMA 与 TCP 数据路径的本质差异；为什么必须内存注册；QP 状态机每步的参数与理由；双边/单边语义与完成感知。入门版（含环境搭建与常见报错）见 [[S-Week 20 - 前置知识 - RDMA verbs 入门]]。

---

## 1. 本质差异：把 OS 请出数据路径

RDMA 三大机制，各自消掉 TCP 路径上的一项成本：

| 机制 | 消掉什么 | 对应 TCP 的什么 |
|---|---|---|
| kernel bypass | 数据路径的系统调用与上下文切换 | write/read 陷入内核 |
| zero-copy | 两端的内核 buffer 中转拷贝 | 用户 ↔ socket buffer |
| 传输卸载 | 分段/保序/重传的 CPU 成本 | 内核协议栈软件实现 |

理解一切复杂度的钥匙：**绕过 OS，就要自己补上 OS 默默做的事**——MR 补地址翻译与钉页、状态机 + 带外交换补连接协商、lkey/rkey 补访问控制。每个"麻烦"对应一项被绕过的服务。

与 [[S-Week 10 - io_uring 深入]] 的连线：registered buffers 和 MR 是同一逻辑（pin + 预翻译，换每次操作免翻译）；与 [[NVMe 命令模型与 SSD 内部专题]] 的连线：SQ/CQ/doorbell 第三次出现——"提交环 + 完成环"是高性能 I/O 的通用形态。

## 2. 对象模型与内存注册

五层积木按依赖顺序：device → **PD**（权限圈地）→ **MR**（注册内存：pin 页 + 地址翻译表 + 发钥匙 lkey/rkey）→ **CQ**（完成收件箱）→ **QP**（SQ+RQ，通信主体，RC 类型为主力）。

**为什么必须注册内存**（必考）：网卡 DMA 需要物理地址且页不能被换出/迁移——注册做三件事：pin 住页、建立网卡侧的虚实翻译、生成访问钥匙。rkey 交给对端才允许单边访问，access flags（REMOTE_READ/WRITE）是权限边界。注册是毫秒级的贵操作 → 工程上**注册池化复用**，绝不在数据路径上注册——和 io_uring 注册 buffer、GPU 显存注册（GDS 的伏笔）同一纪律。

## 3. QP 状态机：每步填的参数就是这步的理由

| 迁移 | 关键参数 | 为什么是这一步 |
|---|---|---|
| RESET → INIT | port_num、pkey、access flags | 纯本端配置 |
| INIT → RTR | 对端 QPN、GID、起始 PSN、path MTU、min_rnr_timer | 需要对端参数——之前必须完成带外交换 |
| RTR → RTS | timeout、retry_cnt、rnr_retry、本端发送 PSN | 能收之后才配"怎么发、出错怎么重试" |

**带外交换**是协议设计的一部分：RDMA 不解决初识问题，两端先用 TCP 换 (QPN, GID, PSN, rkey, buffer 地址)，再各自推状态机。rdma_cm 库就是这段流程的标准化。GID 是 RoCE 的"IP"，GID index 选错是实验第一坑。

## 4. 数据面与完成感知

一次 send/recv 的十步生命周期（完整版在前置知识）压缩成三条铁律：

1. **recv WQE 先行**：RC 下对端没有 post_recv 就 send → RNR NAK。
2. **CQE status 逐个检查**：错误不抛异常，安静躺在 CQE 里。
3. **send 完成 ≠ 对端应用已处理**：只是传输层送达。

双边 vs 单边：

- send/recv（双边）：对端 CPU 参与，适合控制消息——NVMe-oF 的命令胶囊用它。
- read/write（单边）：对端 CPU **零参与**，适合大块数据——NVMe-oF 数据面、**PD 分离的 KV cache transfer**（[[Week 8 - Prefill Decode + Open Source Repro]]，两条培养线的交汇点）。
- 单边 write 的完成感知三答案：WRITE_WITH_IMM（对端出 CQE）、轮询尾部标志字节、补一个小 send。

完成收割的两种模式又是那道选择题：poll_cq 忙轮询（延迟最低、烧核）vs completion channel 事件通知（省 CPU、多一次唤醒延迟）——与 epoll/io_uring/IOPOLL 的完成模型放进同一张对照表（本周面试保底正好是 [[13.7 eventfd、timerfd与跨线程唤醒]]）。

## 5. 错误现场速查（自己造过一遍的）

| 现场 | 报错形态 | 根因 |
|---|---|---|
| 对端未 post_recv | RNR NAK / IBV_WC_RNR_RETRY_EXC_ERR | recv WQE 先行铁律 |
| rkey 错误 / 权限不足 | IBV_WC_REM_ACCESS_ERR | rkey 交换错 / MR 缺 REMOTE_WRITE |
| 状态机漏步 / 参数错 | post 时 EINVAL 或 IBV_WC_WR_FLUSH_ERR | QP 未到位就收发；QP 出错后进 ERROR 态，在途 WQE 全部 flush |

## 6. 面试口述模板

```text
RDMA 和 TCP 的本质差异是把 OS 请出数据路径：kernel bypass 省掉数据
面 syscall，zero-copy 让网卡 DMA 直达注册内存，传输层卸载到网卡——
CPU 不再按字节付费。代价是自己补 OS 的活：内存注册做 pin 加地址翻译
加 rkey 权限，这和 io_uring 的 registered buffer 是同一个逻辑；连接
协商靠 QP 状态机加带外交换——INIT 配本端、RTR 填对端 QPN 和 GID 所以
之前要先用 TCP 换参数、RTS 配重传。数据面三条铁律：recv WQE 先行否
则 RNR、CQE 状态逐个查、send 完成只是送达。双边给控制面，单边 write
对端 CPU 零参与——NVMe-oF 数据面和 PD 分离的 KV transfer 都用它，
完成感知用 IMM 或尾字节轮询。我在 soft-RoCE 上手写过 pingpong 双边
加单边两个版本、故意造过三种错误现场，性能数字的边界我主动声明。
```

追问预案：

- "注册为什么慢？能在数据路径上做吗？" → pin + 建翻译表毫秒级；不能，工程上注册池复用。
- "RC/UC/UD 怎么选？" → RC：可靠保序 + 单边，存储主力；UD：无连接类 UDP，规模大（QP 数少）但要自己处理可靠性。
- "QP 进 ERROR 态怎么恢复？" → 在途 WQE 全 flush，走 RESET 重新过状态机；生产上配合上层重连逻辑。
- "GPU 之间怎么用这套？" → GPUDirect RDMA：MR 注册显存，网卡直接 DMA 显存——S4 的 GDS 与 KV transfer 就建在这上面。

## 关联知识

- [[S-Week 20 - RDMA verbs 入门]]（本专题服务的周计划）
- [[S-Week 20 - 前置知识 - RDMA verbs 入门]]（入门版：环境、十步生命周期、三队列对照）
- [[soft-RoCE 与实验真实性边界专题]]（数据真实性的标准答案）
- [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]]（单边语义的产品级用法）
- [[S-Week 10 - io_uring 深入]]（registered buffers 同源）
- [[Week 8 - Prefill Decode + Open Source Repro]]（KV transfer 交汇点）
- [[00.存储方向专题清单索引]]
