---
title: S-Week 19 - NVMe-oF TCP 与延迟分解
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 19 - NVMe-oF TCP 与延迟分解

> [!goal] 本周目标
> 用内核 nvmet 在 TCP transport 上搭一个 NVMe-oF target，跨网络跑 fio，把远程盘延迟拆成"网络 RTT + 协议/软件开销 + 设备延迟"三段——`nvmeof_latency_breakdown.md` 的第一版数据。理解 AI 存储公司都在做的"盘池化"到底解决什么问题。

## 学习目标

1. **NVMe-oF 解决什么问题？** 存算分离 / 盘池化：盘不再绑死在单机，容量与算力独立扩缩、故障域解耦；NVMe 命令模型原样搬到网络上（qpair 映射为网络连接）。
2. **transport 抽象是什么？** 同一套 NVMe 命令，绑到 TCP / RDMA / FC 三种传输。TCP 最通用（无特殊硬件）但延迟与 CPU 代价最高。
3. **NVMe/TCP 在字节流上怎么跑？** 命令与数据封装成 PDU（CapsuleCmd / H2CData / C2HData…），在 TCP 字节流上定界——S1 学的粘包/拆包问题在这里以工业形态重现。
4. **延迟怎么分解？** 总延迟 ≈ 网络 RTT + 软件开销（两端协议栈 + nvmet 处理）+ 设备延迟。RTT 用 ping 测，设备延迟用 S-Week 18 本地基线，软件开销 = 余项。
5. **队头阻塞在哪？** 多命令共享一条 TCP 连接，一个大传输会挡住后面的小命令——对比 RDMA 的消息语义与多 QP（S-Week 20 伏笔）。

## 1. 搭 target（Day 1-2）

两套拓扑各有用途，都要搭：

- 同机双 network namespace：功能验证，网络延迟近零，适合调配置。
- 两台云主机：性能测量（虚拟网络，标签"性能级但受虚拟化影响"）。

```bash
# target 端：nvmetcli 或直接操作 configfs
sudo apt install -y nvme-cli
sudo modprobe nvmet nvmet-tcp
# configfs 配 subsystem / namespace（绑后端盘或文件）/ port（tcp, 4420）
# host 端：
sudo modprobe nvme-tcp
sudo nvme connect -t tcp -a <target_ip> -s 4420 -n <nqn>
sudo nvme list    # 出现远程盘即成功
```

配置过程全部脚本化进 `scripts/nvmet_setup.sh`（含清理）。

## 2. 对比与延迟分解（Day 3-4）

- fio 4K randread，QD 1 / 8 / 32：本地 NVMe vs netns 回环 vs 跨主机，每组 3 次。
- QD1 延迟分解表：三段减法分账，各段占比；加起来和总延迟对不上时找原因（虚拟网络抖动、iostat 采样窗口）。
- 两侧同窗口观测：host 端与 target 端各跑 iostat / biolatency（工具复用 S1），确认设备侧延迟没变、多出来的都在路上。
- 大块顺序读：看带宽贴多少线速，确认瓶颈在网络还是盘。

## 3. 报告与配菜（Day 5）

- `docs/nvmeof_latency_breakdown.md` v1：分解表 + host / transport / target / 设备四层系统图草稿。
- 每个数字打上三类实验标签（功能级 / 性能级 / 云环境声明）。
- 配菜（1-2 小时）：JuiceFS 架构泛读启动——元数据引擎与对象存储数据面分离的设计，为 S-Week 21 的对比笔记积累素材。

## 4. 推理保温（约 25%）

- LMCache 文档泛读 30 分钟（S4 预热：KV offload 的存储接口长什么样、和本周的远程存储路径有什么关系）。

## 5. 面试保底（约 15%）

> 阶段 2 八股按章清账第 8 讲：协议解析与本周的 PDU 定界互证。

- 算法（5-8 题）：Trie 与位运算。参考 [[5.2.14 Trie 字典树]]、[[5.2.2 位运算]]，配 [[CodeTop 高频题 Top300]] 同类题。
- 八股（1 章）：IO 缓冲与协议解析。过 [[8.7 IO缓冲]]、[[8.15 输入输出缓冲区与协议解析器]]，复面 [[8.9 TCP的粘包和拆包]]。验收：用"NVMe/TCP 的 PDU 怎么在字节流上定界"来回答粘包问题——项目与八股互为证据。
- 项目问答：10 个 Q&A（本周素材：盘池化、transport 代价、延迟分解闭环）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `scripts/nvmet_setup.sh` | target 一键搭建与清理 | 可复跑 |
| `results/nvmeof_tcp/*` | 本地 vs 回环 vs 跨主机矩阵 | 不手动修改、带标签 |
| `docs/nvmeof_latency_breakdown.md`（v1） | 三段分解表 + 四层图草稿 | 分账误差有解释 |

## 7. 验收标准

- [ ] target 脚本一键可复现，host 能 connect 且 fio 跑通。
- [ ] QD1 延迟三段分解有数，加总与实测对得上（误差有解释）。
- [ ] 本地 vs 远程的 QD 扫描曲线完成，差距随 QD 收窄的现象有解释。
- [ ] 每个数字有三类实验标签。
- [ ] 能脱稿讲 NVMe-oF 解决什么问题、TCP transport 的代价。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- NVMe-oF 解决什么问题？三种 transport 各自的代价？
- 你的延迟分解怎么闭环？"软件开销"那段包含什么？
- 为什么 QD 高了，远程和本地的吞吐差距会缩小？
- NVMe/TCP 一次 4K 读有几个网络往返？
- 盘池化之后，故障域发生了什么变化？

## 关联知识

- [[S-Week 18 - NVMe 命令模型与本地基线]]
- [[S-Week 20 - RDMA verbs 入门]]
- [[S-Week 19 - 前置知识 - NVMe-oF TCP 与延迟分解]]
- [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]]
- [[分布式存储阅读专题 - JuiceFS 与 3FS]]（配菜阅读线骨架）
- [[8.13 TCP内核队列与参数调优]]（网络侧调优背景）
- [[块层观测专题 - iostat blktrace eBPF]]（两侧同窗口观测）
- Linux nvmet / nvme-cli 文档；JuiceFS 架构文档（配菜）
