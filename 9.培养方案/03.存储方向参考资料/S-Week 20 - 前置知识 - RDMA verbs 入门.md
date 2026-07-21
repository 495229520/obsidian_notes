---
title: S-Week 20 - 前置知识 - RDMA verbs 入门
date: 2026-07-12
tags:
  - 高性能存储/存储方向参考资料/计划
aliases:
  - 存储 Week 20 前置知识
  - RDMA 前置知识
status: active
---

# S-Week 20 - 前置知识 - RDMA verbs 入门

## 索引

- [[#0. 先建立直觉：TCP 是"请内核帮我发"，RDMA 是"网卡直接搬内存"]]
- [[#1. 三大机制：kernel bypass、zero-copy、传输卸载]]
- [[#2. 对象模型：从 device 到 QP 的五层积木]]
- [[#3. QP 状态机：每一步在填什么参数]]
- [[#4. 数据面：一次 send/recv 的十步生命周期]]
- [[#5. 双边与单边：send/recv vs read/write]]
- [[#6. soft-RoCE 环境与 pingpong 跑通]]
- [[#7. perftest 与三队列同构对照]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 20 - RDMA verbs 入门]] 的总前置知识：写代码前必须通读 2-4 节（对象模型和状态机是本周全部工作的骨架），跑 soft-RoCE 前看 6 节，收口笔记时对照 7 节。深挖版见 [[RDMA verbs 专题 - QP WQE CQ 状态机]]，实验真实性边界见 [[soft-RoCE 与实验真实性边界专题]]。

---

> 这是 S3 技术密度最高的一周，也是国内端侧 RDMA / GPU 互联 JD 的直接证据周。RDMA 的学习曲线陡在**名词多**（PD/MR/QP/WQE/CQ/GID/PSN……），但骨架其实只有一句话：**把 NVMe 的那套"队列对 + doorbell"搬到网卡上，让用户态程序直接指挥网卡搬内存**。上周你刚在 NVMe 里学过这套东西——本周所有名词都能找到对应物。

---

## 0. 先建立直觉：TCP 是"请内核帮我发"，RDMA 是"网卡直接搬内存"

同样是把 4 KiB 数据送到对面机器的内存里：

| | TCP | RDMA |
|---|---|---|
| 发起 | write() 系统调用陷入内核 | 用户态写 WQE + 敲 doorbell，**不进内核** |
| 数据搬运 | 用户 buffer → 内核 socket buffer →（协议栈分段）→ 网卡 | 网卡 DMA **直接读用户 buffer** |
| 对端接收 | 网卡 → 内核 buffer → 中断/软中断 → 协议栈 → 用户 read() 再拷一次 | 网卡 DMA **直接写进对端注册好的内存** |
| 传输层逻辑（重传/保序） | 内核软件实现，烧 CPU | **网卡硬件实现** |

CPU 的账：TCP 每字节都要过 CPU 的手（拷贝 + 协议栈）；RDMA 把这两样都卸给网卡，CPU 只负责"下命令、收完成"。这就是 AI 集群互联（GPU 间 KV cache 搬运、参数同步）清一色 RDMA 的原因——CPU 不再按字节付费。

> [!important] 第一性原理
> RDMA 的一切复杂度（内存注册、状态机、带外交换）都源于一个决定：**让硬件绕过操作系统直接访问应用内存**。绕过 OS，就得自己把 OS 原本默默做的事补上——地址翻译与钉页（MR）、连接参数协商（状态机 + 带外交换）、权限（lkey/rkey）。每个"麻烦"都对应一项被绕过的 OS 服务，这样记就不乱。

## 1. 三大机制：kernel bypass、zero-copy、传输卸载

- **kernel bypass**：QP 的发送/接收队列映射进用户态，post 一个 WQE 是纯内存写 + 一次 doorbell（MMIO）——数据路径零系统调用。（控制路径——建 QP、注册 MR——仍走内核，一次性成本。）
- **zero-copy**：网卡 DMA 直达应用 buffer，收发两侧都没有"内核 buffer 中转拷贝"。
- **传输卸载**：分段、保序、重传、拥塞由网卡硬件做——RC 模式给你"可靠、保序"的语义，软件栈里没有对应的 CPU 消耗。

对照记忆：io_uring 消掉了"每次 I/O 一次 syscall"（但数据还走内核）；RDMA 连数据路径带传输层一起搬出内核。[[S-Week 10 - io_uring 深入]] 的 registered buffers 与本周的 MR 是同一逻辑——**pin 住内存、预先翻译好地址，换取每次操作免翻译**。

## 2. 对象模型：从 device 到 QP 的五层积木

创建顺序即依赖顺序，每层记"一句话职责"：

| 对象 | 一句话职责 | 关键点 |
|---|---|---|
| device / context | 打开网卡 | `ibv_open_device` |
| PD（Protection Domain） | 权限的圈地：MR 和 QP 必须同域才能互用 | 隔离故障与越权 |
| MR（Memory Region） | 注册一块内存：pin 住 + 建地址翻译 + 发钥匙 | 返回 `lkey`（本端用）和 `rkey`（授权远端用）；access flags 决定远端能不能读写 |
| CQ（Completion Queue） | 完成事件的收件箱 | 发送/接收可共用一个 CQ |
| QP（Queue Pair） | 发送队列 + 接收队列，通信的主体 | 类型选 RC（可靠连接，本周唯一用到的） |

QP 三种类型一句话区分：**RC**（可靠、保序、支持单边 read/write——NVMe-oF 和存储场景的主力）；UC（不可靠连接，少用）；UD（不可靠数据报，类似 UDP，multicast 和某些 RPC 用）。

## 3. QP 状态机：每一步在填什么参数

QP 出生在 RESET，要走三步才能通信——**每步填的参数就是这步存在的理由**：

| 迁移 | 填什么 | 为什么这步才能填 |
|---|---|---|
| RESET → INIT | 本地：port_num、pkey_index、访问权限（允许远端读/写吗） | 只涉及本端配置 |
| INIT → RTR（Ready to Receive） | **对端**：dest_qp_num、对端 GID、起始 PSN、path MTU、min_rnr_timer | 必须先拿到对端参数——所以这步之前要做带外交换 |
| RTR → RTS（Ready to Send） | 重传行为：timeout、retry_cnt、rnr_retry、本端发送 PSN | 能收之后才配置怎么发（含出错怎么重试） |

**带外交换**（out-of-band exchange）：RDMA 自己不解决"第一次怎么认识对方"——两端先用普通 TCP 连接交换 `(QPN, GID, PSN, rkey, 远端 buffer 地址)`，然后各自把 QP 推到 RTS。你的 pingpong 程序里这段 TCP 代码不是杂务，是**协议设计的一部分**，librdmacm（rdma_cm）就是把这段标准化的库。

PSN（Packet Sequence Number）是可靠性的序号起点；GID 是 RoCE 世界的"IP 地址"（每个网卡 IP 对应 v2 GID 表里一项，程序里要选对 **GID index**——soft-RoCE 上跑不通十有八九是这里）。

## 4. 数据面：一次 send/recv 的十步生命周期

```text
接收端：
1. ibv_post_recv：往 RQ 挂一个 recv WQE（指向本地 buffer + lkey）
   —— 必须先于对端发送！RC 模式下没有 recv WQE 等着，
      对端 send 会收到 RNR NAK（Receiver Not Ready）
发送端：
2. ibv_post_send：往 SQ 写 send WQE（opcode=IBV_WR_SEND，
   sge 指向数据 + lkey），敲 doorbell
3. 网卡读 WQE → DMA 读用户 buffer → 分段发包（传输层硬件做）
接收端网卡：
4. 收包、校验、按序重组
5. 消费一个 recv WQE，把数据 DMA 进它指的 buffer
6. 往接收端 CQ 写 CQE
发送端网卡：
7. 收到对端 ACK（RC 硬件行为）
8. 往发送端 CQ 写 CQE（表示"这个 WQE 完成了"）
两端应用：
9/10. ibv_poll_cq 收割 CQE，检查 status == IBV_WC_SUCCESS
```

三个纪律：poll_cq 是**忙轮询**（也可配事件通知 `ibv_req_notify_cq`，走 completion channel 睡眠等待——延迟换 CPU，又是那道选择题）；CQE 的 status 必须逐个检查（错误不会抛异常，只会安静地躺在 CQE 里）；send 完成 ≠ 对端应用已处理，只是传输层送达。

## 5. 双边与单边：send/recv vs read/write

| 语义 | 对端 CPU | 对端要做什么 | 典型用途 |
|---|---|---|---|
| send/recv（双边） | 参与 | 必须提前 post_recv | 控制消息、RPC、NVMe-oF 的命令胶囊 |
| read/write（单边） | **完全不参与** | 只需事先注册 MR 并把 rkey+地址给对方 | 大块数据搬运：NVMe-oF 数据面、**PD 分离的 KV cache transfer** |

单边 write 的经典问题："对端怎么知道数据到了？"三种答案：`IBV_WR_RDMA_WRITE_WITH_IMM`（带立即数，对端出一个 CQE）；轮询 buffer 尾部标志字节（数据按地址升序落定，约定最后一个字节为完成标志）；再补一个小 send 通知。你的实验用 IMM 或尾字节轮询任一即可，但要能说出三种。

这就是推理侧 [[Week 8 - Prefill Decode + Open Source Repro]] 里 KV transfer 的底层：prefill 节点把 KV cache 单边 write 进 decode 节点的显存/内存注册区——对端 GPU/CPU 零参与。两条线在这里正式交汇。

## 6. soft-RoCE 环境与 pingpong 跑通

soft-RoCE（rxe）= 内核用软件实现 RoCEv2 网卡：verbs API 完整可用，报文封成 UDP（端口 4791）走普通网卡。**功能 100%，性能 0% 代表性**——它本身就是"内核软件栈"，恰恰是真 RDMA 绕过的东西。

```bash
sudo apt install -y libibverbs-dev ibverbs-utils rdma-core perftest
sudo modprobe rdma_rxe
sudo rdma link add rxe0 type rxe netdev eth0    # 绑到你的网卡
ibv_devices && ibv_devinfo -d rxe0               # 确认设备与 GID 表
# 先用现成工具双端验证环境（server / client）：
ibv_rc_pingpong -d rxe0 -g 1                     # -g 选 GID index（RoCE 必须）
ibv_rc_pingpong -d rxe0 -g 1 <server_ip>
```

环境通了再写自己的 `rdma_pingpong.cpp`——顺序照第 2-4 节的积木和状态机走。写完双边再加单边 write 变体。**三个故意错误**（不 post_recv / 错 rkey / 漏状态机一步）各制造一次，把报错现场记进笔记——错误现场是比成功路径更硬的理解证据。

## 7. perftest 与三队列同构对照

- perftest 套件：`ib_send_lat`（双边延迟）、`ib_send_bw`（双边带宽）、`ib_write_lat/bw`（单边）。soft-RoCE 上数值只看**相对形态**（如 write 略快于 send），绝对值一律不进结论——边界声明写死。
- 收口时把三次见到的"队列对"画进一张对照表：

| | NVMe | io_uring | RDMA |
|---|---|---|---|
| 提交 | SQ + SQE | SQ + SQE | SQ + WQE |
| 完成 | CQ + CQE + phase bit | CQ + CQE | CQ + CQE |
| 通知设备 | doorbell 寄存器 | io_uring_enter / SQPOLL | doorbell 寄存器 |
| 完成感知 | MSI-X 中断 / IOPOLL | 等待 / 轮询 CQ | 事件通道 / poll_cq |

一张表讲三个系统——"提交队列 + 完成队列 + doorbell"是高性能 I/O 的通用形态，这是本周最值钱的抽象，面试里能把讨论从"背 API"拉到"懂设计"。

## 8. 常见错误

- **忘 post_recv 就 send** → RNR NAK。接收 WQE 先行是 RC 的铁律（也是故意错误实验之一）。
- **GID index 选错**：RoCE 必须显式选 GID（v2 + 正确 IP 的那项）；`ibv_devinfo -v` 看 GID 表，pingpong 用 `-g` 指定。
- **MR access flags 漏权限**：单边 write 要求对端 MR 带 `IBV_ACCESS_REMOTE_WRITE`，漏了 → REM_ACCESS_ERR。
- **两端 path MTU 不一致**：RTR 填的 MTU 超过链路实际，soft-RoCE 上表现为莫名超时。
- **不检查 CQE status**：错误安静地躺在 CQE 里，程序"看起来跑通了"数据却没到。
- **把带外交换写成硬编码**：QPN/GID 写死在代码里，换台机器就崩——TCP 交换这段必须是真代码。
- **拿 soft-RoCE 的微秒数吹性能**：它是软件模拟，数值无代表性——这是本周的第一大边界纪律。
- **poll_cq 忙等被当成 bug**：忙轮询是设计选择（CPU 换延迟），不是缺陷；但要知道事件通知模式的存在。

## 9. 学习检查清单

- [ ] 能用"绕过 OS → 自己补 OS 的活"框架解释 MR、状态机、带外交换为什么存在。
- [ ] 五层积木（device/PD/MR/CQ/QP）每层一句话职责 + 创建顺序。
- [ ] 状态机三步各填什么参数、为什么是这步填，能脱稿。
- [ ] 十步生命周期能画出来，RNR 的产生机制能解释。
- [ ] 双边 vs 单边的差异、单边完成感知的三种做法、KV transfer 的连线。
- [ ] soft-RoCE 环境命令熟练，GID index 的坑趟过。
- [ ] 三队列对照表能默写并口述。

## 10. 关键要点总结

- RDMA = 用户态直接指挥网卡搬内存：kernel bypass + zero-copy + 传输卸载，CPU 不再按字节付费。
- 一切复杂度都是"绕过 OS 后自己补课"：MR 补地址翻译、状态机 + 带外交换补连接协商、lkey/rkey 补权限。
- RC 铁律：先 post_recv 再等 send；CQE status 逐个检查。
- 单边 write 对端 CPU 零参与——PD 分离 KV transfer 的底层，两条培养线在此交汇。
- "SQ + CQ + doorbell"第三次出现：NVMe、io_uring、RDMA 同构，一张对照表讲透。
- soft-RoCE 只证功能不证性能，边界声明写死。

## 关联知识

- [[S-Week 20 - RDMA verbs 入门]]（本篇服务的周计划）
- [[RDMA verbs 专题 - QP WQE CQ 状态机]]（深挖版与面试口述）
- [[soft-RoCE 与实验真实性边界专题]]（边界声明的标准写法）
- [[S-Week 10 - io_uring 深入]]（registered buffers 与 MR 同源）
- [[S-Week 18 - 前置知识 - NVMe 命令模型与本地基线]]（SQ/CQ/doorbell 第一次出现）
- [[Week 8 - Prefill Decode + Open Source Repro]]（KV transfer：单边 write 的推理侧应用）
- [[13.7 eventfd、timerfd与跨线程唤醒]]（事件通知 vs 轮询的八股互证，本周面试保底）

## 参考

- RDMA Aware Networks Programming User Manual（NVIDIA/Mellanox，对象模型与状态机的权威手册，查表用）
- rdma-core 仓库的 `libibverbs/examples`（rc_pingpong 源码，精读对象）
- perftest 文档（ib_send_lat / ib_write_bw 参数）
- 内核文档：Documentation/infiniband（rxe 一节）
- `man ibv_post_send`、`man ibv_modify_qp`（状态机参数表）
