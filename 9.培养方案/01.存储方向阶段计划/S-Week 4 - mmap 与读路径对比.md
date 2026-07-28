---
title: S-Week 4 - mmap 与读路径对比
date: 2026-07-08
tags:
  - 高性能存储
  - 存储方向阶段计划
  - 计划
status: active
---

# S-Week 4 - mmap 与读路径对比

> [!goal] 本周目标
> 把第三条读路径 mmap 加进对比：什么时候 mmap 比 read 快、什么时候更慢、缺页在哪里发生。顺带用 `posix_fadvise` 验证 readahead 的存在。本周结束时，read vs mmap 的选型你要能给出有数据的判断，而不是背结论。

## 学习目标

1. **mmap 读文件的路径是什么？** 缺页中断 → page cache 填充 → 页表映射，之后访问就是内存访问。
2. **mmap 和 read 的本质区别？** read 是显式拷贝（page cache → 用户缓冲区），mmap 是共享映射省一次拷贝，但每页首次访问要缺页。
3. **缺页怎么观测？** `perf stat -e page-faults,major-faults` 或 `/usr/bin/time -v` 的 major/minor fault 计数。
4. **fadvise / madvise 能改变什么？** WILLNEED 预读、SEQUENTIAL 加大 readahead、RANDOM 关掉 readahead、DONTNEED 主动驱逐。
5. **为什么 RocksDB 默认不用 mmap 读？**（为阶段 2 埋点：随机点查冷数据时缺页开销、io 错误处理困难）

## 1. mmap 实验（Day 1-3)

### 1.1 实现

在 `linux-io-lab` 加 `mmap_read.cpp`：

```cpp
void* addr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
// 顺序模式：按页步进累加校验和；随机模式：固定 seed 随机页访问
// madvise(addr, file_size, MADV_SEQUENTIAL / MADV_RANDOM) 作为开关
```

注意点：逐页访问（步长 4096）而不是逐字节，用校验和防止编译器把访问优化掉；RAII 包装 munmap。

### 1.2 对比矩阵（与前两周同一文件、同一偏移序列）

| 实验 | 冷/热 | 对比对象 |
|---|---|---|
| mmap 顺序遍历 | 冷 + 热 | 上周 read 顺序 |
| mmap 随机访问 | 冷 + 热 | 上周 read 随机 |
| mmap + MADV_SEQUENTIAL 顺序 | 冷 | 默认 readahead 的差异 |
| mmap + MADV_RANDOM 随机 | 冷 | 关 readahead 后是否更稳 |

每组记录：总耗时、吞吐、major/minor faults（`perf stat`）。

### 1.3 fadvise / readahead 补充实验

- `posix_fadvise(fd, 0, len, POSIX_FADV_WILLNEED)` 后再冷读：延迟接近热读吗？
- `blockdev --getra /dev/nvme0n1` 查看 readahead 窗口，`--setra` 改小后冷顺序读慢多少（改完记得复原并记录在 env.md）。

### 1.4 必须回答

- 热 cache 下 mmap 顺序遍历为什么常比 read 快？（省一次拷贝、无 syscall per block）
- 冷 cache 随机小访问下 mmap 为什么可能更差？（每页一次 major fault、无法像 io_uring 那样堆队列深度）
- 什么负载适合 mmap？（只读热数据、随机点查内存级延迟要求，如索引/字典）

## 2. 理论配套

- 复习 [[1.3 进程的虚拟地址空间]]，把 mmap 区域、页表、缺页和笔记对上。
- OSTEP 内存虚拟化部分的 TLB 章节快速过一遍（解释大文件 mmap 随机访问的 TLB 压力）。
- `man 2 mmap`、`man 2 madvise`、`man 2 posix_fadvise`。

## 3. 推理保温（约 25%）

- [[Week 6 - Observability + Metrics]] 收尾：完成 `runbook.md`（TTFT 变高怎么分诊、KV cache 满怎么办）。
- 交叉思考写进 runbook：serving 的"KV cache 满 → offload/驱逐"与本周"page cache 驱逐 / DONTNEED"是同构问题——都是缓存分层与逐出策略。

## 4. 面试保底（约 15%）

- 算法（5-8 题）：链表。参考 [[5.2.9 链表常见技巧]]，做 [[206. 反转链表]]、[[25. K 个一组翻转链表]]、[[146. LRU 缓存]]（LRU 和本周缓存主题正好呼应）。
- 八股（1 章）：内存管理。过 [[12.1.13 malloc与new]]、[[6.6 堆与栈的区别]]。
- 项目问答：10 个 Q&A。

## 5. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `src/mmap_read.cpp` | mmap 读程序（顺序/随机/madvise 开关） | 校验和一致，防优化 |
| `results/*.csv` | 含 fault 计数的原始数据 | 不手动修改 |
| `docs/read_vs_mmap.md` | 三条读路径（read / O_DIRECT / mmap）对比报告 | 有选型结论矩阵 |

## 6. 验收标准

- [ ] mmap 四组实验完成，fault 计数记录在案。
- [ ] WILLNEED 预读实验完成，效果量化。
- [ ] readahead 窗口调整实验完成（并已复原设置）。
- [ ] 能给出 read vs mmap 的选型判断矩阵（负载类型 x 冷热 → 推荐路径）。
- [ ] 能解释 RocksDB 为什么默认不用 mmap 读。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- mmap 读文件时数据拷贝了几次？read 呢？
- major fault 和 minor fault 的区别？
- mmap 什么时候比 read 快？什么时候更慢？
- MADV_SEQUENTIAL 和 MADV_RANDOM 分别改变了什么？
- fadvise WILLNEED 和自己先 read 一遍预热有什么区别？

## 关联知识

- [[S-Week 3 - fio 对照与 Benchmark Matrix]]
- [[S-Week 5 - io_uring 异步 IO]]
- [[S-Week 4 - 前置知识 - mmap 与读路径对比]]
- [[mmap 与读路径对比专题]]
- [[1.3 进程的虚拟地址空间]]
- [[146. LRU 缓存]]（缓存逐出思想同源）
