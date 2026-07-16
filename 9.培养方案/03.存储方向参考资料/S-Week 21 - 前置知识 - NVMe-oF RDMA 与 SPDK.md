---
title: S-Week 21 - 前置知识 - NVMe-oF RDMA 与 SPDK
date: 2026-07-12
tags:
  - infra
  - 存储
  - 参考资料
aliases:
  - 存储 Week 21 前置知识
  - SPDK 前置知识
status: active
---

# S-Week 21 - 前置知识 - NVMe-oF RDMA 与 SPDK

## 索引

- [[#0. 先建立直觉：同一条命令，换一条腿]]
- [[#1. NVMe-oF RDMA 数据面：比 TCP 少了什么]]
- [[#2. nvmet-rdma 搭建与功能级纪律]]
- [[#3. 无损网络三件套：PFC、ECN、DCQCN]]
- [[#4. SPDK：用户态驱动的收益与代价]]
- [[#5. spdk perf 对比实验设计]]
- [[#6. "CPU 换延迟"哲学的三次重现]]
- [[#7. 配菜收口：JuiceFS 与 3FS 对比笔记]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 21 - NVMe-oF RDMA 与 SPDK]] 的总前置知识：搭 RDMA transport 前读 1-2 节，写无损网络口述稿前精读 3 节（这是纯概念周任务，本篇就是主要材料），跑 SPDK 前读 4-5 节。拥塞控制深挖版见 [[RoCE 拥塞控制专题 - PFC ECN DCQCN]]，transport 对照见 [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]]。

---

> 本周把 S3 的三块拼图拼完：S-Week 19 的 NVMe-oF 和 S-Week 20 的 RDMA 在 `nvmet-rdma` 里合体；RoCE 网络为什么要"无损"过成口述级；SPDK 展示存储数据路径的第三种形态——**连内核都不要了**。三块的共同主题：性能优化的每一步都是把某个"通用服务"换成"专用机制"，而你要能说出换掉的是什么、代价是什么。

---

## 0. 先建立直觉：同一条命令，换一条腿

S-Week 19 的 configfs 配置里改一个词（`addr_trtype: tcp → rdma`），host 端 `nvme connect -t rdma`——同一个 subsystem、同一块盘、同一套 NVMe 命令，跑在了完全不同的传输上。这就是 transport 抽象的兑现时刻。

变化发生在数据面：

- **TCP 腿**：命令和数据都封成 PDU，在字节流里排队，两端内核协议栈全程参与（拷贝、中断、定界）。
- **RDMA 腿**：命令胶囊走 send/recv（双边），**数据走单边 read/write**——target 直接从 host 注册的内存里读写数据，host 的 CPU 在数据搬运阶段完全不参与。上周你手写的两种语义，在这里就是产品级用法。

## 1. NVMe-oF RDMA 数据面：比 TCP 少了什么

一次 4K 读在两条腿上的对照：

| 环节 | NVMe/TCP | NVMe/RDMA |
|---|---|---|
| 命令下发 | CapsuleCmd PDU 走字节流 | 命令胶囊走 send（对端已 post_recv） |
| 数据返回 | C2HData PDU → 内核协议栈 → 拷贝进用户/内核 buffer | target 对 host 的 MR 做 RDMA write，**DMA 直达** |
| 定界 | PDU 头显式定界（粘包问题） | 消息语义天然定界，无粘包 |
| 队头阻塞 | 一条 TCP 连接内严格有序 | 多 QP 并行，彼此独立 |
| CPU 成本 | 两端按字节付费 | 数据面近零（网卡搬运） |

所以 RDMA 腿少了：**两次内核拷贝、中断驱动的协议栈处理、字节流定界、单连接队头阻塞**。代价在网络层——RDMA（RoCE 形态）对丢包极其敏感，这就引出第 3 节。

功能级实验的观测点（soft-RoCE 上数值无意义，看**路径与形态**）：连接建立时 `rdma` 统计接口和 target 日志里能看到 QP 建立；对照 TCP 腿的 `ss -t` 连接——"qpair 映射到什么"在两条腿上的差异是这周实验的主要产出。

## 2. nvmet-rdma 搭建与功能级纪律

```bash
# target 端（在 S-Week 19 脚本基础上改）
sudo modprobe nvmet-rdma rdma_rxe
# port 配置只改 transport：
echo rdma > ports/2/addr_trtype     # 其余 traddr/trsvcid(4420)/adrfam 同前
# host 端
sudo modprobe nvme-rdma
sudo nvme connect -t rdma -a <IP> -s 4420 -n <NQN>
```

纪律重申（本周最容易违反）：soft-RoCE 上的 NVMe-oF/RDMA 是**功能级实验**——它验证"路径能通、语义正确、和 TCP 的结构差异"，**不产生任何性能结论**。fio 可以跑（证明功能），数字只许出现在"功能验证"小节且标注边界。真实性能对比留给（可选的）裸金属补测，见 [[soft-RoCE 与实验真实性边界专题]]。

## 3. 无损网络三件套：PFC、ECN、DCQCN

这是纯口述任务，逻辑链必须自己能推一遍：

**第一环：RoCE 为什么怕丢包。** RoCEv2 把 IB 传输层封在 UDP 里跑以太网。它的硬件重传是简化的 **go-back-N**：丢一个包，从那个包开始的整个窗口全部重发——高带宽下丢 0.1% 的包就能把有效吞吐打掉一大截（对比 TCP 的 SACK 只补洞）。结论：**要么别丢包，要么修好拥塞控制，最好都要**。

**第二环：PFC 保证不丢。** PFC（Priority Flow Control，802.1Qbb）：交换机某优先级队列快满时，向**上游**发 pause 帧，让上游停发这个优先级——逐跳背压，缓冲不溢出，包不丢。这是"无损以太网"的机制本体。

**第三环：PFC 的副作用（面试的分水岭，必须讲够三个）：**

1. **队头阻塞**：pause 按优先级粒度生效，同优先级里无辜的流量陪着一起停（受害者流问题）。
2. **pause 风暴**：背压逐跳向上游传播，一个拥塞点能冻住一大片网络。
3. **PFC 死锁**：多台交换机的缓冲依赖成环（CBD，cyclic buffer dependency），谁都在等对方放行——极端但真实发生过的生产事故。

**第四环：ECN + DCQCN 让 PFC 少触发。** 思路：与其等缓冲满了硬刹车（PFC），不如提前**轻点刹车**。交换机队列超阈值时给包打 ECN 标记（CE）；接收端网卡看到标记，回一个 **CNP**（拥塞通知包）给发送端；发送端网卡按 DCQCN 算法降速，之后按"快恢复 → 加性增 → 超增"三阶段爬回来。三个角色：**CP**（拥塞点，交换机，负责标记）、**NP**（通知点，接收端网卡，负责回 CNP）、**RP**（反应点，发送端网卡，负责调速）。

**第五环：分工总结（口述稿的收尾句）。** DCQCN 做常态调速让网络尽量不到 PFC 触发线；PFC 做最后防线保证万一到了也不丢包。副作用清单：DCQCN 参数敏感（阈值、速率恢复的时间常数要按网络规模调，调错会欠吞吐或压不住）、PFC 三连坑如上。认知边界声明：**概念与取舍能讲，没在真实交换机上调过**——这句诚实声明本身就是标准答案的一部分。

## 4. SPDK：用户态驱动的收益与代价

SPDK 的三个支柱，每个都对应它消掉的一项内核服务：

| 支柱 | 消掉了什么 | 代价 |
|---|---|---|
| 用户态 NVMe 驱动（UIO/VFIO 把 BAR 映射给用户态） | 系统调用、内核块层的通用逻辑 | 设备被独占，内核和其他进程看不见这块盘 |
| 轮询模式（PMD） | 中断与上下文切换 | 核 100% 空转是工作状态——烧核换延迟 |
| 无锁 per-core 设计 + hugepages | 锁竞争、TLB miss、页错误 | 应用要按 SPDK 的框架（reactor/bdev）重写 |

环境准备与实验命令：

```bash
git clone https://github.com/spdk/spdk && cd spdk
sudo scripts/pkgdep.sh && ./configure && make -j
sudo HUGEMEM=2048 scripts/setup.sh        # 配 hugepages + 把 NVMe 绑到 vfio-pci/uio
sudo build/examples/hello_bdev            # bdev 抽象跑通
sudo build/examples/perf -q 32 -o 4096 -w randread -t 30   # spdk 自带 perf
sudo scripts/setup.sh reset               # 实验完必须归还设备给内核！
```

两个环境坑提前知道：云主机常无 IOMMU → vfio 不可用，setup.sh 会回落到 `uio_pci_generic`（记录进 env.md）；**绑定后 `/dev/nvme0n1` 消失是正常现象**（设备从内核驱动上解绑了），`setup.sh reset` 归还——忘了 reset，重启前这块盘对系统"不存在"。

## 5. spdk perf 对比实验设计

对照组：内核路径用 fio（io_uring 引擎、O_DIRECT、同 bs/QD）；实验组：`spdk perf` 同参数。每组记三类指标：

- 延迟：平均 + p99（spdk perf 有 `-L` 输出延迟统计）；
- IOPS；
- **CPU：每千 IOPS 烧掉多少核**——SPDK 侧固定烧满整核（轮询），要算的是"这个核换来了多少 IOPS"；内核侧用 `pidstat` 记 fio 的 CPU。

预期形态（QD1 差距最大，QD32 收窄）：SPDK 的收益集中在**低 QD 延迟**（省中断/syscall 的固定成本）；高 QD 下设备饱和，两者 IOPS 接近，差的是 CPU 成本。结论表的最后一列永远是"什么场景值得"：单盘家用没必要，几十块盘的存储节点每核 IOPS 密度就是钱。

## 6. "CPU 换延迟"哲学的三次重现

把三个学过的机制放进一张表——这是 S3 的抽象收口，也是面试的高光素材：

| 机制 | 谁在轮询 | 消掉什么 | 烧什么 |
|---|---|---|---|
| io_uring IOPOLL（S-Week 10） | 内核线程轮询 NVMe CQ | 完成中断 | 一个核 |
| RDMA poll_cq（S-Week 20） | 用户态轮询网卡 CQ | 事件通知开销 | 一个核 |
| SPDK PMD（本周） | 用户态轮询设备队列 | 中断 + syscall + 内核栈 | 整核常驻 |

同一句总结：**当单次操作的软件固定成本可比于硬件延迟时，轮询开始划算**——NVMe 微秒级、RDMA 微秒级，正是这个区间；毫秒级的机械盘时代轮询是荒谬的，这就是"为什么这些技术现在才流行"的答案。

## 7. 配菜收口：JuiceFS 与 3FS 对比笔记

本周产出两篇对比笔记（总纲最低要求），骨架直接用 [[分布式存储阅读专题 - JuiceFS 与 3FS]] 的三问结构：元数据路径、数据路径、AI 负载适配。写作纪律：每篇 ≤ 一页、每个论断标注来源（官方文档/设计文档）、不懂的标 TODO 不硬写。Ceph 只画一张 RADOS 写路径草图（client → primary OSD → 副本 → ack）即止。

## 8. 常见错误

- **soft-RoCE 上的 NVMe-oF/RDMA 数字进了性能结论**：本周最大红线——功能级标签写死。
- **PFC 副作用只讲队头阻塞**：三连坑（HoL、风暴、死锁）少一个，口述就不完整。
- **把 DCQCN 讲成 TCP 拥塞控制**：DCQCN 跑在**网卡硬件/固件**里、以 CNP 为信号、按速率（而非窗口）调节——三点都和 TCP 不同。
- **SPDK 实验完忘 `setup.sh reset`**：盘从系统里"消失"，下一个实验一脸懵。
- **对比实验两侧参数不一致**：fio 用 psync、SPDK 用 QD32——比的不是路径是参数。
- **把 CPU 100% 当 SPDK 的故障**：轮询模式的常态，要报告的是"每核换来多少 IOPS"。
- **对比笔记写成产品介绍**：没有"两者差异 + 为什么"，只有各自的功能罗列——对比是笔记的灵魂。

## 9. 学习检查清单

- [ ] 能对照表说出 NVMe-oF RDMA 相对 TCP 少了哪四样开销。
- [ ] 能把无损网络五环逻辑链（怕丢包 → PFC → 三副作用 → ECN/DCQCN → 分工）3 分钟脱稿讲完。
- [ ] CP/NP/RP 三角色和各自动作能对号入座。
- [ ] SPDK 三支柱各自"消掉什么、代价什么"能成表。
- [ ] setup.sh 绑定/归还的流程和坑清楚。
- [ ] "CPU 换延迟"三次重现的对照表能默写，临界条件那句话能脱稿。
- [ ] 两篇对比笔记的三问骨架就绪。

## 10. 关键要点总结

- transport 抽象兑现：同一套 NVMe 命令，TCP 腿全程内核护送，RDMA 腿命令走双边、数据走单边直达。
- 无损网络逻辑链：go-back-N 怕丢包 → PFC 硬保不丢（三副作用）→ ECN/DCQCN 常态调速让 PFC 只当最后防线。
- SPDK = 用户态驱动 + 轮询 + 无锁：消掉内核通用服务，代价是独占、烧核、生态隔离。
- 轮询划算的临界条件：软件固定成本 ≈ 硬件延迟——微秒级设备时代的必然选择。
- 全周纪律：soft-RoCE 只证功能；每个性能数字必须能回答"在哪类实验环境测的"。

## 关联知识

- [[S-Week 21 - NVMe-oF RDMA 与 SPDK]]（本篇服务的周计划）
- [[S-Week 20 - 前置知识 - RDMA verbs 入门]]（单边/双边语义的地基）
- [[RoCE 拥塞控制专题 - PFC ECN DCQCN]]（口述稿的深挖版）
- [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]]（transport 对照收口）
- [[soft-RoCE 与实验真实性边界专题]]（功能级标签的标准写法）
- [[S-Week 10 - io_uring 深入]]（IOPOLL：轮询哲学第一次出现）
- [[分布式存储阅读专题 - JuiceFS 与 3FS]]（配菜笔记的骨架）

## 参考

- NVMe over Fabrics Specification（RDMA transport binding 章，泛读）
- DCQCN 论文：*Congestion Control for Large-Scale RDMA Deployments*（SIGCOMM 2015，读摘要 + 设计章）
- SPDK 官方文档：Getting Started、bdev、`examples/perf` 用法
- JuiceFS 架构文档；DeepSeek 3FS 设计笔记（GitHub 仓库 docs）
- RoCEv2 与 PFC：IEEE 802.1Qbb 概念介绍类资料（厂商白皮书即可）
