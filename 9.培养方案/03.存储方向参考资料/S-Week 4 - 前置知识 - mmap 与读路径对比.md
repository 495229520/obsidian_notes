---
title: S-Week 4 - 前置知识 - mmap 与读路径对比
date: 2026-07-12
tags:
  - infra
  - 存储
  - 参考资料
aliases:
  - 存储 Week 4 前置知识
  - mmap 前置知识
status: active
---

# S-Week 4 - 前置知识 - mmap 与读路径对比

## 索引

- [[#0. 先建立直觉：不调 read 也能"读"文件]]
- [[#1. mmap 的机制：VMA、缺页、页表]]
- [[#2. major fault 与 minor fault]]
- [[#3. 三条读路径的成本对照]]
- [[#4. madvise 与 fadvise：两个控制面]]
- [[#5. 测量程序要点：防优化与 fault 计数]]
- [[#6. readahead 窗口实验：blockdev 的单位陷阱]]
- [[#7. 为什么 RocksDB 默认不用 mmap 读]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 4 - mmap 与读路径对比]] 的总前置知识：动手前通读 0-3 节建立缺页图景，写程序前看 5 节，做 fadvise/readahead 实验前看 4、6 节，写选型结论前看 7 节。深挖版见 [[mmap 与读路径对比专题]]；虚拟内存基础复习 [[1.3 进程的虚拟地址空间]]。

---

> 前三周的读路径都靠系统调用显式发起；本周的 mmap 把文件"变成一段内存"，读文件退化成读内存——听起来是免费午餐，但缺页、页表、TLB 每一项都在暗中标价。本周结束你要能对任意负载给出 read vs mmap 的推荐，并用 fault 计数证明理由。

---

## 0. 先建立直觉：不调 read 也能"读"文件

```cpp
void* addr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
const char* p = static_cast<const char*>(addr);
char c = p[0];          // 没有任何 read 调用，文件第一个字节到手了
```

第一行 `mmap` 返回时**一个字节都没读**——它只是在进程地址空间里登记了一段 VMA（"这段虚拟地址对应这个文件"）。真正的读发生在 `p[0]` 那一刻：CPU 发现该页无映射，触发缺页中断，内核把文件页装进 page cache 并建好页表，指令重新执行。

> [!important] 第一性原理
> mmap 没有消灭 I/O，只是把"何时读"从你调 read 的时刻，**推迟并隐藏**到首次访问的缺页里。所有对比实验本质上都在回答：显式付钱（syscall + 拷贝）和隐式付钱（缺页 + 页表），哪种对当前负载更便宜。

## 1. mmap 的机制：VMA、缺页、页表

一次冷 mmap 随机访问的完整路径：

```text
访问 p[x]
 → MMU 查页表：无映射，缺页中断
 → 内核查 VMA：地址合法，对应文件某页
 → page cache 查该页：
     不在 → 发起设备读，同步等待（major fault）
     在   → 直接用（minor fault）
 → 建页表项 → 恢复执行，p[x] 就是普通内存访问
```

之后再访问同一页：纯内存速度，连内核都不进。两个隐性成本：

- **页表体积**：8 GiB 文件全部摸一遍 ≈ 200 万个页表项。
- **TLB 压力**：大映射随机访问时 TLB miss 频繁，"内存速度"也分三六九等。

## 2. major fault 与 minor fault

| | major fault | minor fault |
|---|---|---|
| 页在 page cache？ | 不在 | 在 |
| 要等设备？ | 是（同步，几十微秒起） | 否（只建页表，亚微秒） |
| 对应场景 | 冷 mmap 访问 | 热 mmap 首次访问、fork 后首写 |

本周每组实验必记这两个计数：

```bash
perf stat -e page-faults,major-faults ./mmap_read --mode random --cold
/usr/bin/time -v ./mmap_read ...   # 输出里的 Major/Minor page faults
```

预期形态：冷随机 = major 数 ≈ 访问页数；热随机 = major ≈ 0、minor ≈ 访问页数；同一程序跑第二遍 = 两者都趋近 0（映射还在时）。**fault 计数是本周结论的证据主体**，只有延迟数字没有 fault 计数的对比表不合格。

另一个关键差别：major fault 是**同步阻塞**的——一个线程同一时刻只有一个在途缺页，堆不了队列深度。这是冷随机负载下 mmap 注定输给异步 I/O 的结构性原因（S-Week 5 的 io_uring 会把这点放大）。

## 3. 三条读路径的成本对照

| 成本项 | pread（buffered） | O_DIRECT | mmap |
|---|---|---|---|
| 每次访问入口 | syscall（~百 ns） | syscall | 首次缺页（major：几十 µs / minor：亚 µs），之后零 |
| 数据拷贝（cache 命中） | 1 次（cache → buffer） | — | 0 次 |
| 缓存 | page cache | 自管 | page cache |
| 并发深度 | 同步 1（io_uring 可堆） | 同 | 缺页同步，堆不了 |
| 错误形态 | 返回 -errno | 返回 -errno | **SIGBUS 信号** |

热顺序大遍历 mmap 常赢（省百万次 syscall 和拷贝）；冷随机点查 mmap 常输（每页一个 major fault 还不能并发）。中间地带靠本周数据说话。

## 4. madvise 与 fadvise：两个控制面

同一套提示语义，两个作用对象：

- `posix_fadvise(fd, off, len, ...)`：作用于 **fd 的文件范围**——read 路径用。
- `madvise(addr, len, ...)`：作用于**映射的地址范围**——mmap 路径用。

| 提示 | 效果 |
|---|---|
| SEQUENTIAL | 加大预读窗口（mmap 缺页时多读几页邻居） |
| RANDOM | 关预读，只读缺的那页 |
| WILLNEED | 异步预取进 page cache（不建页表，下次访问仍是 minor fault） |
| DONTNEED | 提示丢弃（madvise 版还会拆映射；脏页语义两者有差异，只读实验不涉及） |

本周实验设计就是拿这两个开关做消融：`MADV_SEQUENTIAL` 顺序 vs 默认、`MADV_RANDOM` 随机 vs 默认——量化预读对 mmap 路径的贡献。`POSIX_FADV_WILLNEED` 后再冷读则回答"预取能不能把冷读救成热读"。

## 5. 测量程序要点：防优化与 fault 计数

```cpp
// 顺序模式：按页步进（4096），逐页读一个字节进校验和
uint64_t checksum = 0;
for (size_t off = 0; off < file_size; off += 4096) {
    checksum += static_cast<const unsigned char*>(addr)[off];
}
// 程序结尾必须使用 checksum（打印），否则整个循环可能被优化掉
```

- **按页步进而不是逐字节**：逐字节读 8 GiB 是在测 memcpy，不是测缺页；每页碰一个字节就足以触发该页装载。
- **校验和防优化**：编译器看到结果未使用会把访问删光——O2 下"0 秒读完 8 GiB"就是这么来的。校验和同时兼任正确性对账（与 pread 版本比对）。
- **随机模式复用前几周的固定 seed 偏移序列**（对齐到页边界），三条路径才可比。
- munmap 用 RAII 包装，模式与前几周的 fd 包装一致。
- 冷热制造与验证照旧：sync + drop_caches、三重证据（延迟 / iostat / fault 计数——本周第三证从 buff/cache 换成 fault）。

## 6. readahead 窗口实验：blockdev 的单位陷阱

```bash
blockdev --getra /dev/nvme0n1    # 输出 256 —— 单位是 512B 扇区！即 128 KiB
sudo blockdev --setra 0 /dev/nvme0n1     # 关预读
# ... 实验 ...
sudo blockdev --setra 256 /dev/nvme0n1   # 必须复原，并记入 env.md
```

- `--getra` 的单位是 **512 字节扇区**：256 = 128 KiB，与 `/sys/block/<dev>/queue/read_ahead_kb` 的 128（KiB）是同一个值的两种单位——别当成两个参数。
- 这是全设备参数，影响机器上所有进程：改前记录原值，脚本里用 trap 保证异常退出也复原（S-Week 11 会再次用到这条纪律）。
- 预期：setra 0 后冷顺序读明显变慢、逼近冷随机读——反向证明 readahead 是"冷顺序快"的功臣（闭环 S-Week 1 的结论）。

## 7. 为什么 RocksDB 默认不用 mmap 读

把本周知识组装成工业案例（面试加分项，四条都要能落回机制）：

1. LSM 的常态负载是**冷数据随机点查**：正是 mmap 最差的象限（每次 major fault 同步阻塞查询线程，且不可控）。
2. RocksDB 有**自己的 block cache**，缓存决策（缓存谁、驱逐谁、算不算命中率）必须在自己手里；page cache + mmap 把这些全变成黑盒。
3. SST 块是**压缩的**：读出来必须解压到自己的 buffer，mmap 的零拷贝优势落空。
4. **错误处理**：坏块在 pread 下是可处理的返回值，在 mmap 下是 SIGBUS——存储引擎要的是可恢复错误。

对照记忆：LMDB 全 mmap 且成立——因为它假设数据基本常驻内存、不压缩、读为主。**结论跟着负载假设走**，这句话是选型题的万能句式，但每次都要能展开到机制层。

## 8. 常见错误

- **把 mmap 调用本身计入"读延迟"**：它只建 VMA，几微秒就返回；真实成本在缺页里，计时要覆盖访问阶段。
- **逐字节遍历**：测成了内存带宽，缺页成本被稀释到看不见。
- **忘了用校验和**，循环被优化掉：0 秒"读完"8 GiB 还以为 mmap 无敌。
- **冷实验前忘记 munmap 或重启进程**：上一轮的页表映射还在，minor fault 都省了，"冷"得不彻底——drop_caches 之外还要保证映射是新建的。
- **blockdev --setra 单位当成 KiB**：设 128 以为是 128 KiB，实际是 64 KiB。
- **改了 readahead 忘复原**：污染后续所有周的实验（trap 复原写进脚本）。
- **对比实验三条路径用了不同的偏移序列**：数据不可比，结论作废。
- **在 32 位思维下映射大文件**：本课程环境是 64 位无此问题，但面试可能问——32 位地址空间放不下大映射，这曾是 mmap 的历史限制。

## 9. 学习检查清单

- [ ] 能讲出 mmap 调用返回时发生了什么、第一次访问时发生了什么。
- [ ] 能区分 major / minor fault 并说出各自的观测命令。
- [ ] 能默写三条读路径的对照表（拷贝次数、入口成本、并发、错误形态）。
- [ ] 能解释"mmap 堆不了队列深度"的机制及其对冷随机负载的影响。
- [ ] 能说出 madvise 与 fadvise 的作用域差别。
- [ ] 知道 blockdev --getra 的单位与复原纪律。
- [ ] 能用四条机制理由解释 RocksDB 默认 pread，并用 LMDB 做反例对照。

## 10. 关键要点总结

- mmap = 把 I/O 推迟并隐藏进缺页；显式成本（syscall + 拷贝）换隐式成本（缺页 + 页表 + TLB）。
- major fault 同步等设备且不可并发——冷随机负载下 mmap 的结构性劣势。
- fault 计数（perf stat）是本周结论的证据主体，与延迟、iostat 组成三重证据。
- 防优化三件套：按页步进、校验和、结果必须被使用。
- 选型没有普适赢家：热点查 mmap、冷点查 read/io_uring、自管缓存 O_DIRECT——结论跟着负载假设走。

## 关联知识

- [[S-Week 4 - mmap 与读路径对比]]（本篇服务的周计划）
- [[mmap 与读路径对比专题]]（深挖版与面试口述）
- [[1.3 进程的虚拟地址空间]]（VMA 与页表基础）
- [[S-Week 1 - Page Cache 与 readahead 专题]]（预读机制与 fadvise）
- [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]]（第三条路径）
- [[146. LRU 缓存]]（缓存逐出思想同源）

## 参考

- `man 2 mmap`、`man 2 madvise`、`man 2 posix_fadvise`、`man 8 blockdev`
- OSTEP 内存虚拟化部分（TLB 章）与第 39/40 章：[网站](https://pages.cs.wisc.edu/~remzi/OSTEP/)
- RocksDB Wiki：IO 相关页（mmap 选项的官方说明）
- perf stat 文档（page-faults / major-faults 事件）
