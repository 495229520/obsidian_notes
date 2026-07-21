---
title: mmap 与读路径对比专题
date: 2026-07-12
tags:
  - 高性能存储/存储方向专题清单
roadmap_week: 阶段 0-1（S-Week 4 主线；S-Week 11 全路径图复用）
sort_order: "01.30"
status: active
---

# mmap 与读路径对比专题

> [!info] 所属路线
> - 培养方案阶段：阶段 0（S-Week 4 实验主线）；S-Week 11 的全路径图把本专题作为 mmap 旁支的底稿
> - 排序：01.30
> - 用途：把三条读路径（read / O_DIRECT / mmap）放进同一张对照表，讲清 mmap 的缺页机制、选型边界，以及 RocksDB 默认不用 mmap 读这个工业案例。

> [!goal] 目标
> 能回答三层问题：机制层（mmap 读到底发生了什么、拷贝几次、缺页分几类）、数据层（什么负载下 mmap 赢/输，用自己的 fault 计数说话）、选型层（给一个负载能当场给出推荐路径和理由）。

---

## 1. 三条读路径同表对照

| 维度 | read/pread（buffered） | O_DIRECT | mmap |
|---|---|---|---|
| 走 page cache | 是 | 否 | 是（缺页装入） |
| 数据拷贝次数（命中时） | 1（cache → 用户 buffer） | —（DMA 直达用户 buffer） | 0（直接访问 cache 页） |
| 每次访问的入口成本 | syscall | syscall | 首次缺页，之后纯内存访问 |
| 并发能力 | 同步一次一个（io_uring 可异步） | 同上 | **缺页同步阻塞，单线程堆不了队列深度** |
| 错误处理 | 返回值 -errno，好处理 | 同左 | I/O 错误变 **SIGBUS 信号**，极难优雅处理 |
| readahead / 预取控制 | fadvise | 无（自己负责） | madvise |

一句话版本：read 付 syscall 和一次拷贝，mmap 付缺页和页表/TLB，O_DIRECT 什么服务都不要自己全管。

## 2. mmap 的机制细节

- `mmap()` 本身**不读任何数据**：只建 VMA（虚拟地址区间登记）。第一次访问某页才触发缺页。
- 缺页两类，成本天差地别：
  - **major fault**：页不在 page cache，要同步等设备读——冷 mmap 随机访问慢的根源。
  - **minor fault**：页已在 cache，只是没建页表项——只需补映射，微秒级以下。
- 热数据大遍历时 mmap 的隐性成本：每页都要建页表项，大映射随机访问还有 **TLB miss** 压力——"零拷贝"不等于零成本。
- 观测手段：`perf stat -e page-faults,major-faults` 或 `/usr/bin/time -v`；这是 S-Week 4 每组实验必记的计数。

madvise / fadvise 分工：作用对象不同（映射区间 vs fd），提示语义同一套（SEQUENTIAL / RANDOM / WILLNEED / DONTNEED，详见 [[S-Week 1 - Page Cache 与 readahead 专题]]）。

## 3. 选型矩阵（S-Week 4 用数据填实）

| 负载 | 冷热 | 推荐 | 理由 |
|---|---|---|---|
| 顺序大遍历 | 热 | mmap 略优 | 省 per-block syscall 与一次拷贝 |
| 顺序大遍历 | 冷 | read ≈ mmap | 都吃 readahead 红利，瓶颈在设备 |
| 随机点查 | 热 | mmap 明显优 | 纯内存访问，无 syscall |
| 随机点查 | 冷 | **read/io_uring 优** | mmap 每页一次 major fault 且无法堆 QD |
| 自管缓存的引擎 | — | O_DIRECT | 避免双重缓存，控制权在自己 |

适合 mmap 的典型场景：只读、基本常驻内存、随机点查要内存级延迟——索引、字典、只读 mdb 类结构（LMDB 是 mmap 架构的代表）。

## 4. 工业案例：RocksDB 为什么默认 pread

面试加分项，四条理由都能落到机制上：

1. **冷数据随机点查**是 LSM 读的常态：mmap 意味着不受控的 major fault 阻塞查询线程；pread 配自己的 block cache，缺不缺、缓存谁全在自己手里。
2. **错误处理**：磁盘坏块在 mmap 下是 SIGBUS，在 pread 下是返回值——引擎要的是可恢复错误。
3. **与 O_DIRECT/压缩块设计冲突**：SST 里的块是压缩的，读出来必须解压到自己的 buffer，mmap 的"零拷贝"优势用不上。
4. **可观测与限流**：显式 read 的每次 I/O 都可计数、可限速、可打点；缺页是隐式的。

反例记忆锚点：LMDB 全 mmap 且成立，因为它假设"数据基本在内存 + 只读为主 + 不压缩"。**选型结论跟着负载假设走，不存在普适赢家**——这句话本身就是面试要的答案。

## 5. 面试口述模板

```text
mmap 读文件是把文件页映射进地址空间，mmap 调用本身不读数据，首次
访问缺页：页不在 cache 是 major fault 要同步等设备，在 cache 只补
页表是 minor fault。和 read 比它省了 syscall 和一次拷贝，代价是缺
页不可控、错误变 SIGBUS、单线程堆不了队列深度。我的实验数据是热
随机点查 mmap 明显赢，冷随机点查 mmap 输给 read，fault 计数能对上。
所以 RocksDB 默认 pread 加自管 block cache——冷点查是它的常态负载，
而 LMDB 全 mmap 成立是因为假设数据常驻内存。选型跟着负载假设走。
```

追问预案：

- "mmap 拷贝几次、read 拷贝几次？" → 命中时 mmap 0 次、read 1 次；不命中两者都先设备 DMA 进 cache，再各走各的。
- "WILLNEED 和自己先读一遍的区别？" → WILLNEED 异步发起预取立即返回、不占用户 buffer；预热读是同步的还多一次拷贝。
- "mmap 文件被别的进程 truncate 了会怎样？" → 访问被截掉的页收 SIGBUS——共享文件场景要防。
- "为什么 mmap 堆不了队列深度？" → 缺页在访问指令处同步发生，一个线程同一时刻只有一个在途缺页；要并发只能多线程或回到异步 I/O。

## 关联知识

- [[S-Week 4 - mmap 与读路径对比]]（本专题服务的周计划）
- [[S-Week 4 - 前置知识 - mmap 与读路径对比]]（入门版与实验流程）
- [[S-Week 1 - Page Cache 与 readahead 专题]]（预读与 fadvise 底稿）
- [[O_DIRECT 与持久化语义专题]]（第三条路径的专题）
- [[1.3 进程的虚拟地址空间]]（VMA、页表、缺页的进程视角）
- [[LSM-tree 与 B+ tree 专题]]（阶段 2：RocksDB 读路径的下游）
- [[00.存储方向专题清单索引]]
