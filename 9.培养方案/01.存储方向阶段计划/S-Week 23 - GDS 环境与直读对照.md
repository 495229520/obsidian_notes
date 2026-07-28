---
title: S-Week 23 - GDS 环境与直读对照
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 23 - GDS 环境与直读对照

> [!goal] 本周目标
> 阶段 3 开局：租支持 GDS 的云 GPU 机型，跑通 cuFile SSD→GPU 直读，用 pageable / pinned bounce / GDS 三组对照把"少一跳"的收益量化成带宽、CPU 占用两条曲线。这是 S4 `gds-kv-offload-lab` 的第一块实验，也是 S1 的 O_DIRECT 纪律和 S-Week 20 的"注册 + DMA 直达"心智模型在 GPU 上的兑现。

## 学习目标

1. **传统路径的两跳账怎么算？** NVMe → 主机内存 → 显存：两份总线带宽、主机内存穿透两次、CPU 参与编排——GDS 省的是那一跳和主机那条总线，不是"更快的引擎"。
2. **cuFile 的使用条件是什么？** O_DIRECT + 对齐（S-Week 2 纪律原样适用）、nvidia-fs 内核模块、支持的文件系统与 PCIe 拓扑。
3. **兼容模式陷阱怎么防？** 条件不满足时 cuFile 静默回落内部 bounce buffer——每轮实验必须用 gds_stats / cufile.log 断言真走了 P2P。
4. **三组对照各代表什么？** pageable（最差基线）、pinned + cudaMemcpyAsync（传统路径最优形态，真正的 baseline）、GDS（直达）。
5. **收益什么时候显形？** 大块顺序、多并发、主机 CPU/内存带宽吃紧时；单盘小块下 pinned 与 GDS 差距不大——预期形态先写下来，拿数据验证。

## 1. 租机与环境预检（Day 1）

沿用总纲"每周一次、集中 4-6 小时"的 GPU 租用节奏，**脚本全部就绪再开表**：

```bash
nvidia-smi && nvidia-smi topo -m        # GPU 型号 + PCIe 拓扑（NVMe 与 GPU 的距离）
lsmod | grep nvidia_fs                  # nvidia-fs 内核模块
/usr/local/cuda/gds/tools/gdscheck -p   # GDS 平台支持自检（文件系统/驱动/兼容项逐条）
nvme list && cat /etc/mtab | grep -E "ext4|xfs"
```

机型、驱动版本、CUDA 版本、gdscheck 输出全部进 `env.md`——GDS 数据的可信度从环境记录开始。gdscheck 报不支持的项：能修则修（挂载参数、模块加载），不能修就记录并评估是否换机型——**在 compat mode 下跑完全程是本周最大的无效劳动**。

## 2. 三组对照实现（Day 2-3）

- 工具通道：`gdsio`（GDS 自带 benchmark 工具，传输模式参数可切 GDS / CPU 中转两类路径）先扫一轮，拿到参考值。
- 自写通道 `src/gds_read.cu`（正确性 gate 沿用 S-Week 5/10 纪律）：
  - A：`pread` → pageable 内存 → `cudaMemcpy`；
  - B：`pread(O_DIRECT)` → pinned buffer → `cudaMemcpyAsync`；
  - C：`cuFileRead` 直达显存（`cuFileBufRegister` 注册显存，句柄与 buffer 池化复用）。
  - 每组读回 D2H 后与 pread 版本校验和对账，全部通过才进性能实验。
- 每轮跑完立即 `gds_stats` 断言 C 组的 P2P 计数在涨——把断言写进脚本，不靠肉眼。

## 3. 对照矩阵与归因（Day 4-5）

- 矩阵：块大小 4K / 64K / 1M / 16M × 并发 worker 1 / 4 / 16 × 三组路径，每组 3 次。
- 指标三件套：吞吐（GB/s）、CPU 占用（`pidstat` 盯搬运线程）、gds_stats 计数；有条件加主机内存带宽观测，没有就推算 B 组的两倍穿透并声明。
- 预期形态（不符合就解释）：小块低并发三组接近（瓶颈在盘）；大块高并发下 B 组撞主机内存/CPU 墙、C 组继续跟盘走；A 组全程垫底。
- 产出 `docs/gds_report.md` v1：三条曲线 + "收益来自解除主机瓶颈"的归因 + 环境边界（单盘/租用机型/驱动版本）。

## 4. 推理线合流（约 25%）

- 阶段 3 起推理保温与存储主线在 S4 合流：本周合流动作是 serving benchmark harness 冒烟复跑（下周 KV offload 实验就靠它发压）+ KV cache 每 token 口算保手感。

## 5. 面试保底（约 15%）

> 阶段 3 面试保底进入**秋招冲刺编排**：CodeTop 前 150 已清零（阶段 2 验收），改为按频率向后扩展 + 手撕代码 + 五板块题库轮换，2027-07 秋招提前批前完成三轮全真模拟。

- 算法（5-8 题）：CodeTop 151-200 按频率推进；手撕热身一道：LRU 缓存徒手写到一次过（[[146. LRU 缓存]]、[[5.2.16 面试常考数据结构设计]]）。
- 题库轮换（1 板块）：[[存储面试问题清单 - Linux I O]] 全板块随机抽答复面。
- 项目问答：10 个 Q&A（本周素材：两跳账、compat mode、三组对照设计）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `env.md` | 机型/驱动/拓扑/gdscheck 全记录 | 逐项可查 |
| `src/gds_read.cu` | 三路径对照程序 | 校验和 gate 通过 |
| `results/gds/*.csv` | 对照矩阵原始数据 | 不手动修改、附 gds_stats 断言日志 |
| `docs/gds_report.md`（v1） | 三曲线 + 归因 + 边界 | 每个结论有数据 |

## 7. 验收标准

- [ ] gdscheck 预检通过（或不支持项有记录与对策），全程无 compat mode 假实验。
- [ ] 三组路径正确性对账全部通过。
- [ ] 对照矩阵跑完，三件套指标齐全。
- [ ] "收益来自哪一跳"的归因有曲线支撑，预期不符处有解释。
- [ ] 租期在 6 小时内完成（脚本先行纪律）。
- [ ] 合流与面试保底完成本周额度。

## 面试问题

- SSD 到显存的传统路径有几跳？每跳消耗谁的资源？
- GDS 需要哪些条件？不满足时 cuFile 的行为是什么？你怎么防？
- 你的实验里 pinned bounce 和 GDS 什么时候接近、什么时候拉开？为什么？
- cuFileBufRegister 和 io_uring registered buffer、RDMA MR 什么关系？
- 单盘场景 GDS 不明显，那它的招牌场景是什么？

## 关联知识

- [[S-Week 22 - nvme-of-lab 收口与阶段 2 复盘]]
- [[S-Week 24 - KV offload 环境与负载构造]]
- [[S-Week 23 - 前置知识 - GDS 环境与直读对照]]
- [[GPUDirect Storage 专题 - cuFile 与 bounce buffer]]
- [[O_DIRECT 与持久化语义专题]]（对齐纪律的来源）
- [[S-Week 10 - io_uring 深入]]（registered buffers 同源）
- NVIDIA GPUDirect Storage 文档（cuFile API / gdsio / Best Practices）
