---
title: NVMe-oF 专题 - TCP 与 RDMA transport 取舍
date: 2026-07-12
tags:
  - infra
  - 存储
  - NVMe
  - RDMA
  - 面试
roadmap_week: "阶段 2（S-Week 19 TCP、S-Week 21 RDMA，S-Week 22 收口）"
sort_order: "02.10"
status: active
---

# NVMe-oF 专题 - TCP 与 RDMA transport 取舍

> [!info] 所属路线
> - 培养方案阶段：阶段 2 `nvme-of-lab` 的主干，横跨 S-Week 19（TCP + 延迟分解）、S-Week 21（RDMA 功能级）、S-Week 22（四层图收口）
> - 排序：02.10
> - 用途：报告"项目二"的理论底稿——NVMe-oF 解决什么问题、三种 transport 各自的代价、延迟分解怎么闭环。Pure / VAST / WEKA 这类公司的面试主场。

> [!goal] 目标
> 讲清三件事：盘池化/存算分离为什么需要 NVMe-oF；TCP 与 RDMA 两条 transport 在数据面的逐项差异；远程盘延迟的四层分解方法。入门与搭建细节见 [[S-Week 19 - 前置知识 - NVMe-oF TCP 与延迟分解]] 和 [[S-Week 21 - 前置知识 - NVMe-oF RDMA 与 SPDK]]。

---

## 1. NVMe-oF 解决什么：盘的位置变成部署细节

- **盘池化**：盘集中到存储节点，容量按需划拨——消灭"这台满那台空"的碎片。
- **存算分离**：计算与存储独立扩缩、独立故障域；AI 训练集群的存储层（含 3FS 这类系统）全是这个架构。
- 实现方式不发明新命令：**同一套 NVMe 命令集，qpair 从 PCIe 映射到网络连接**——host 端 `nvme list` 里远程盘与本地盘无差别，上层全体无感。

架构对象一行记全：host —(transport)— port → subsystem → namespace，NQN 做全局命名，discovery 服务做黄页。

## 2. 三种 transport 对照表

| 维度 | TCP | RDMA（RoCE/IB） | FC |
|---|---|---|---|
| 硬件要求 | 无（任何以太网） | RDMA 网卡（+ 无损网络配置） | FC HBA + FC 交换网 |
| 数据面拷贝 | 两端内核栈各一次+ | 网卡 DMA 直达（零拷贝） | HBA 卸载 |
| 延迟开销 | 最高（协议栈 + 中断 + 定界） | 最低（单边 read/write） | 低 |
| CPU 成本 | 按字节付费 | 数据面近零 | 低 |
| 运维复杂度 | 最低 | 高（PFC/ECN 调优，见 [[RoCE 拥塞控制专题 - PFC ECN DCQCN]]） | 高（专用网络，存量市场） |
| 定位 | 通用默认、跨机房友好 | 性能敏感的 AI/数据库存储后端 | 传统企业 SAN 存量 |

一句话取舍：**TCP 用最大兼容性换性能，RDMA 用网络运维复杂度换性能，FC 是历史存量**。云上和跨数据中心默认 TCP；机柜内的 AI 存储集群上 RDMA。

## 3. 数据面差异：一次 4K 读的两条腿

| 环节 | NVMe/TCP | NVMe/RDMA |
|---|---|---|
| 命令 | CapsuleCmd PDU 走字节流 | 命令胶囊走 send/recv（双边） |
| 数据 | C2HData PDU，两端内核拷贝 | target 对 host MR 单边 write，DMA 直达 |
| 定界 | PDU 头（粘包问题的工业答案） | 消息语义天然定界 |
| 大写 | R2T 流程多一个往返（≈2 RTT） | 单边 read 拉取，无额外往返 |
| 队头阻塞 | 单连接字节流严格有序 | 多 QP 独立并行 |

RDMA 腿少掉的四样：内核拷贝 ×2、中断驱动的协议栈、字节流定界、单连接队头阻塞。这张表就是"三种 transport 代价"面试题的展开版。

## 4. 延迟分解：四层图与闭环方法

$$
L_{total} \approx L_{network} + L_{software} + L_{device}
$$

- 网络：ping RTT 中位数（+ 千兆下 4K 的传输时延）；设备：本地基线（**口径锁定**：同 bs/QD/engine/direct）；软件：余项（host + target 两端栈合计，拆分需两侧打点，声明为 TODO）。
- 闭环校验：三段加总与实测差 > 20% 就归因（虚拟网络抖动 / 采样窗口 / 时钟——分解只用单侧计时）。
- 佐证手段：两侧同窗口观测——target 侧 biolatency 显示设备延迟未变，host 侧总延迟涨了，差值就在路上和栈里。
- QD 形态：QD1 差距 = RTT + 软件项，最刺眼；QD 高了流水线掩盖单笔延迟，吞吐差距收窄——**"延迟差、吞吐平"是网络存储的标准形态**。

## 5. 面试口述模板

```text
NVMe-oF 解决盘池化和存算分离：同一套 NVMe 命令，qpair 从 PCIe 映射
成网络连接，上层无感。三种 transport 是三档交换：TCP 零硬件门槛，
代价是两端内核栈按字节付费、PDU 定界和单连接队头阻塞，大写还要 R2T
多一个往返；RDMA 命令走双边、数据走单边 DMA 直达，四样开销全省，
代价是要维护无损以太网——PFC 加 DCQCN 那一套；FC 是企业 SAN 存量。
我的实验把远程盘延迟分解成网络、软件、设备三段：RTT 用 ping、设备
用同口径本地基线、软件是余项，两侧同窗口 biolatency 互证闭环。QD1
时远程差一整个 RTT 加软件项，QD 堆上去吞吐差距收窄——延迟差、吞吐
平。RDMA 部分我在 soft-RoCE 上做的是功能级验证，性能边界我主动声明。
```

追问预案：

- "什么时候选 TCP 而不是 RDMA？" → 跨机房/云环境（无损网络不可控）、运维团队无 RDMA 经验、延迟预算宽松——兼容性收益大于微秒级差距。
- "NVMe/TCP 的队头阻塞怎么缓解？" → 多 qpair（多连接）分摊；这正是 RDMA 多 QP 天然免疫的点。
- "你的软件开销那段包含什么？" → 两端协议栈 + nvmet 处理 + PDU 封装；两端拆分需要 bpftrace 在 nvmet 函数上打点，我声明为 TODO 而不是硬拆。
- "存算分离后故障域怎么变了？" → 盘故障不再绑定计算节点（好）；网络分区成为新的故障面、存储节点成为爆炸半径更大的共享依赖（坏）——要配副本/EC 与多路径。

## 关联知识

- [[S-Week 19 - NVMe-oF TCP 与延迟分解]] / [[S-Week 21 - NVMe-oF RDMA 与 SPDK]]（本专题服务的两周）
- [[S-Week 19 - 前置知识 - NVMe-oF TCP 与延迟分解]]（PDU 与 configfs 细节）
- [[NVMe 命令模型与 SSD 内部专题]]（qpair 的设备层来源）
- [[RDMA verbs 专题 - QP WQE CQ 状态机]]（单边 write 的机制底稿）
- [[RoCE 拥塞控制专题 - PFC ECN DCQCN]]（RDMA 腿的网络代价）
- [[soft-RoCE 与实验真实性边界专题]]（功能级声明的写法）
- [[00.存储方向专题清单索引]]
