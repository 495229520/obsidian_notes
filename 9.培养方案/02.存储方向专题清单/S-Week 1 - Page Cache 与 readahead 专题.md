---
title: S-Week 1 - Page Cache 与 readahead 专题
date: 2026-07-10
tags:
  - 高性能存储/存储方向专题清单/计划
roadmap_week: 阶段 0（S-Week 1、S-Week 4）
sort_order: "01.05"
status: active
---

# S-Week 1 - Page Cache 与 readahead 专题

> [!info] 所属路线
> - 培养方案阶段：阶段 0（S-Week 1 冷/热基线；S-Week 4 mmap 与 fadvise 实验再次用到）
> - 排序：01.05
> - 用途：把 [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]] 的第 ④ 层（唯一短路点）单独讲透：缓存怎么组织、冷热怎么制造、预读为什么让"冷顺序读"不冷。

> [!goal] 目标
> 讲清三件事：page cache 的读写行为、drop_caches 的确切语义、readahead 的触发与控制。支撑 S-Week 1 的四组实验设计，以及"冷顺序读为什么快"这个必答题。

---

## 1. Page cache 的本质

内核以 page（4 KiB）为单位缓存**文件内容**，按（文件，页号）索引。三个关键性质：

- **机会主义占用**：空闲内存尽量拿来缓存，内存紧张时按 LRU 近似策略回收 clean page。所以 `free` 少不等于内存不够，看 `available`。
- **读写共用**：读未命中把数据装进来；写先写进来标记 dirty，异步刷盘。cache 是读写路径的汇合点。
- **对应用透明**：不改一行代码就生效——也正因为透明，benchmark 不控制它就会测出"假数据"。

## 2. 读命中与未命中

```text
命中：   pread → 找到 page → memcpy 到用户缓冲区 → 返回（~1 µs）
未命中： pread → 分配 page → 发起设备读（同步等待）→ 装入 page cache
         → memcpy → 返回（~20-100 µs，NVMe）
```

注意未命中路径是"**先入 cache 再拷贝**"，所以任何一次冷读都自动完成了加热——这决定了实验纪律：每组冷实验前必须重新 drop_caches，冷读只有第一遍算数。

## 3. 写与 dirty page（概念级，S-Week 2 深化）

- `write` 返回只保证数据进了 page cache（标 dirty），**不保证落盘**。
- 内核 writeback 线程按脏页比例/年龄异步刷盘；`sync` / `fsync` 强制刷。
- 掉电时 dirty page 丢失——这就是持久化语义问题，[[O_DIRECT 与持久化语义专题]] 与 [[S-Week 2 - O_DIRECT + 持久化语义]] 的主线。

## 4. 观察 page cache：free 与 /proc/meminfo

```bash
free -h                     # buff/cache 列：page cache 主体
grep -E 'Cached|Dirty|Buffers' /proc/meminfo
```

| 指标 | 含义 | 实验用法 |
|---|---|---|
| Cached | page cache 大小（不含 swap cache） | 读 8 GiB 文件后应上涨约 8 GiB |
| Dirty | 脏页量 | sync 后应归零附近，drop 前检查 |
| Buffers | 块设备元数据缓冲 | 通常远小于 Cached，了解即可 |

## 5. drop_caches 的确切语义

```bash
sync && echo 3 | sudo tee /proc/sys/vm/drop_caches
```

- `1` = 丢 page cache；`2` = 丢 dentry/inode 缓存；`3` = 都丢。
- **只丢 clean page**：它是"释放"不是"写回"，脏页绝不会被它刷盘或丢弃——所以不 sync 就 drop，脏页留存，冷得不彻底。
- 它是一次性动作不是开关：drop 之后新的读写立刻重新填充 cache。
- 它不影响 O_DIRECT 路径（本来就不进 cache）。
- 全机生效、root 权限：只在专用实验机做，且命令需人工确认（CLAUDE.md 安全边界）。

冷热状态的三重证据（报告标准写法）：

```text
1. 延迟数量级：热 ~1 µs vs 冷 ~几十 µs
2. iostat -x：热读 r/s = 0、%util = 0；冷读 r/s 大量出现
3. free -h：drop 后 buff/cache 回落，冷读过程中逐步回涨
```

## 6. readahead：预读机制

![[图片/9.培养方案/02.存储方向专题清单/9_2_3_1.svg|860]]

内核对每个打开的文件维护预读状态机：

- **触发**：检测到顺序访问模式（本次读紧接上次结束位置）。
- **行为**：异步提前读取后续内容进 page cache，窗口随命中逐步增大；默认上限 128 KiB（`/sys/block/<dev>/queue/read_ahead_kb`，标准默认值 128）。
- **效果**：顺序读的第 N+1 次请求大概率命中"刚预读进来的 page"——**单次 syscall 看到内存速度，设备在后台以大块顺序 I/O 喂数据**。
- **失效**：随机访问打不出顺序模式，预读不启动（或很快缩窗），每次读都同步等设备。

这就是 S-Week 1 实验矩阵里"冷顺序读明显快于冷随机读"的答案，口述时要点两层：

1. readahead 把"多次小的同步设备读"变成"少数大的异步设备读"。
2. 块层合并进一步把相邻请求拼成大请求（`iostat` 里冷顺序读的 rkB/s 高而 r/s 相对少，即平均请求变大——rareq-sz 可直接验证）。

### 6.1 应用侧控制：posix_fadvise（S-Week 4 实验）

```c
posix_fadvise(fd, off, len, POSIX_FADV_SEQUENTIAL); // 加大预读窗口
posix_fadvise(fd, off, len, POSIX_FADV_RANDOM);     // 关闭预读
posix_fadvise(fd, off, len, POSIX_FADV_WILLNEED);   // 主动预取
posix_fadvise(fd, off, len, POSIX_FADV_DONTNEED);   // 提示丢弃（做冷实验的轻量替代）
```

S-Week 4 的实验点：对同一负载分别加 SEQUENTIAL / RANDOM 提示，量化预读的收益与误伤（随机负载被预读污染 cache、浪费带宽）。

## 7. 面试口述模板

```text
page cache 是内核用空闲内存做的文件内容缓存，读命中只是一次内存
拷贝，未命中要同步等设备，NVMe 上两者差 1-2 个数量级。写默认也
只写进 cache 标脏页，落盘靠 writeback 或 fsync。做实验时冷状态
用 sync 加 drop_caches 制造——sync 在前是因为 drop_caches 只丢
干净页不写回脏页。冷顺序读比冷随机读快的原因是 readahead：内核
检测到顺序模式后异步大块预读，把设备 I/O 批量化了，我用 iostat
的平均请求大小验证过这一点。
```

追问预案：

- "readahead 对什么负载有害？" → 随机大文件负载：预读的数据用不上，浪费带宽并挤占 cache，可用 POSIX_FADV_RANDOM 关掉。
- "怎么不用 root 也让文件变冷？" → `POSIX_FADV_DONTNEED`（只影响指定文件，注意先 fsync 脏页）。
- "mmap 读文件走不走 page cache？" → 走，缺页时装入，S-Week 4 对比 mmap 与 read 路径。

## 关联知识

- [[S-Week 1 - 前置知识 - 环境搭建 + Page Cache 基线]]（入门版与实验流程）
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]（本专题在全路径中的位置）
- [[O_DIRECT 与持久化语义专题]]（绕过 cache 与落盘保证，S-Week 2）
- [[mmap 与读路径对比专题]]（缺页路径与 fadvise 实验，S-Week 4）
- [[S-Week 1 - 环境搭建 + Page Cache 基线]]、[[S-Week 4 - mmap 与读路径对比]]
- [[00.存储方向专题清单索引]]
