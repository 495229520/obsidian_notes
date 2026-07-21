---
title: S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径
date: 2026-07-10
tags:
  - 高性能存储/存储方向专题清单/计划
roadmap_week: 阶段 0-1（S-Week 1-11）
sort_order: "01.00"
status: active
---

# S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径

> [!info] 所属路线
> - 培养方案阶段：阶段 0-1（S-Week 1 起贯穿整个 linux-io-lab）
> - 排序：01.00
> - 用途：能力主线一的骨架专题。培养方案的验收标准是"能画出一次 read/write 从系统调用到 NVMe 设备的完整路径，并解释每层的延迟来源"——这篇就是那张图的文字版。

> [!goal] 目标
> 把 read 的每一层讲到"知道它存在、知道它干什么、知道它贡献多少延迟"的深度。S-Week 1 用它回答"一次 read 走了哪些层"；S-Week 8-9（blktrace / eBPF）时回来把每层换成实测数字。

---

## 1. 全路径总览

![[图片/9.培养方案/02.存储方向专题清单/9_2_2_1.svg|900]]

文字版（面试白板手画时按这个顺序默写）：

```text
用户程序 read()/pread()
  │  ① 系统调用边界（用户态 → 内核态）
  ▼
VFS（虚拟文件系统）
  │  ② 统一接口，按 fd 分发到具体文件系统
  ▼
文件系统（ext4 / xfs）
  │  ③ 文件逻辑偏移 → 磁盘物理块号
  ▼
Page Cache ────────命中──────→ 拷贝到用户缓冲区，直接返回 ★短路点
  │  ④ 未命中：分配 page，发起真正的设备读
  ▼
块层（block layer, blk-mq）
  │  ⑤ 组装 bio → request，合并、排队、分发到硬件队列
  ▼
NVMe 驱动
  │  ⑥ 请求写入 submission queue，敲 doorbell
  ▼
NVMe SSD 设备
     ⑦ 设备执行，DMA 到内存，completion queue + 中断返回
```

记忆锚点：**page cache 是唯一的短路点**。它命中，下面三层全部不发生——这就是冷/热延迟差数量级的结构性原因。

## 2. 每一层做什么

### ① 系统调用边界

用户态陷入内核（syscall 指令），做参数与权限检查。固定成本约百纳秒级。它决定了一个重要直觉：**4K 小读的热路径里，syscall 开销占比不小**——这也是后面 io_uring（批量提交、减少 syscall 次数）存在的动机之一，S-Week 5 展开。

### ② VFS

Linux "一切皆文件"的实现层：把 `read` 统一分发给 ext4、xfs、procfs、socket 等各自的实现。对性能分析而言它几乎透明，但面试画图必须有这一层——它是"为什么同一个 read 接口能读普通文件也能读管道"的答案。

### ③ 文件系统

核心工作是**映射**：文件内偏移 → extent/块号。元数据（inode、extent 树）本身也要读，也走 page cache（dentry/inode cache——`drop_caches` 的 2 就是丢它们）。ext4 与 xfs 在大文件顺序写、并发分配上策略不同，阶段 0 只需记录用的是哪个（`df -T`）。

### ④ Page Cache

见 [[S-Week 1 - Page Cache 与 readahead 专题]]。路径视角只需记住：命中 = 一次内存拷贝；未命中 = 同步等待下面所有层完成后再拷贝。

### ⑤ 块层（blk-mq）

未命中的读被组装成 **bio**（描述"读哪个设备哪些扇区到哪些 page"），进入多队列块层 blk-mq：

- 相邻请求**合并**（顺序读的第二重加速：readahead 批量化 + 块层合并）。
- 每个 CPU 有软件队列，映射到设备的多个硬件队列——为 NVMe 的多队列模型铺路。
- `iostat` 的 r/s、await、aqu-sz 都统计于这一层；blktrace 能看到 bio 在这层的完整生命周期（S-Week 8）。

### ⑥ NVMe 驱动

把 request 翻译成 NVMe 读命令，放入 **submission queue**，写 doorbell 寄存器通知设备。NVMe 的 queue pair（SQ/CQ）模型每核可有独立队列、无锁提交——这是它比 SATA/AHCI（单队列）适合多核的本质原因，S3（nvme-of-lab）深挖。

### ⑦ 设备

SSD 控制器取命令、查 FTL 映射、读 NAND、DMA 直接写回内存、往 **completion queue** 放完成项、发中断。设备内部（FTL/GC/写放大）是阶段 2 的内容。

## 3. 每层延迟量级

面试要求能"解释每层的延迟来源"，量级表（以 4K 读为例，精确值以实测为准）：

| 层 | 量级 | 备注 |
|---|---|---|
| syscall 往返 | ~100 ns | 热路径固定成本 |
| VFS + 文件系统查映射 | ~百 ns（缓存命中时） | extent 缓存在内存 |
| page cache 命中拷贝 | ~几百 ns - 1 µs | 约等于 memcpy 4 KiB + syscall |
| 块层排队与调度 | 微秒级，负载高时排队可放大 | aqu-sz 大时 await 上升 |
| NVMe 设备读 | ~20-100 µs | 冷读延迟的主体 |
| （对照）HDD 随机读 | ~5-10 ms | 寻道 + 旋转，OSTEP Ch.37 |

两条结论：

1. **热读瓶颈在 CPU 侧**（syscall + 拷贝），**冷读瓶颈在设备**。优化方向完全不同。
2. p99 毛刺通常来自排队（块层/设备内部 GC），不是中位数路径——所以观测要分层（iostat 看块层、biolatency 看分布，S-Week 8-9）。

## 4. buffered vs O_DIRECT 的分叉点

`O_DIRECT` 打开的文件在第 ④ 层分叉：**跳过 page cache**，用户缓冲区直接 DMA。代价是失去缓存与预读、且有对齐要求；收益是路径可控、不污染 cache——数据库自己管缓存时用它。这是 [[S-Week 2 - O_DIRECT + 持久化语义]] 的主题，也是 [[O_DIRECT 与持久化语义专题]] 的入口。

写路径的不同点（本专题以读为主线）：write 通常只写进 page cache 标记 dirty 就返回（第 ④ 层短路），落盘由 writeback / fsync 触发——持久化语义同样归 S-Week 2。

## 5. 面试口述模板

```text
一次 read 先经过系统调用进入 VFS，分发到具体文件系统，文件系统把
文件偏移翻译成块号，然后查 page cache：命中就直接内存拷贝返回，
微秒级；未命中则组装 bio 进入块层 blk-mq，合并排队后由 NVMe 驱动
写入 submission queue，设备读完通过 DMA 和 completion queue 返回，
这条冷路径在几十到上百微秒。所以冷热差 1-2 个数量级，瓶颈一个在
CPU 一个在设备。我在 linux-io-lab 里用冷/热实验加 iostat 验证过
这个分层：热读时块设备零流量，冷读时 r/s 和 r_await 都对得上。
```

追问预案：

- "怎么证明热读没碰盘？" → iostat r/s = 0 + buff/cache 涨落。
- "冷顺序读为什么快于冷随机读？" → readahead + 块层合并，见 [[S-Week 1 - Page Cache 与 readahead 专题]]。
- "每层的延迟你测过吗？" → 阶段 0 有端到端冷/热数据；阶段 1 用 blktrace/biolatency 分层实测（诚实交代进度）。

## 关联知识

- [[S-Week 1 - 前置知识 - 环境搭建 + Page Cache 基线]]（本专题的入门版）
- [[S-Week 1 - Page Cache 与 readahead 专题]]（第 ④ 层深挖）
- [[O_DIRECT 与持久化语义专题]]（分叉路径，S-Week 2）
- [[io_uring 异步 IO 专题]]（syscall 开销的解法，S-Week 5）
- [[块层观测专题 - iostat blktrace eBPF]]（第 ⑤ 层实测，阶段 1）
- [[4.1 打开、读取、写入、关闭]]、[[13.4 epoll模型]]
- [[00.存储方向专题清单索引]]
