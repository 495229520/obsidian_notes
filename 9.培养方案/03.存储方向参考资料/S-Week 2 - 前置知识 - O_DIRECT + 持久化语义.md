---
title: S-Week 2 - 前置知识 - O_DIRECT + 持久化语义
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向参考资料
  - 计划
aliases:
  - 存储 Week 2 前置知识
  - O_DIRECT 前置知识
status: active
---

# S-Week 2 - 前置知识 - O_DIRECT + 持久化语义

## 索引

- [[#0. 先建立直觉：write 返回的那一刻，数据在哪]]
- [[#1. 写路径全景：从 write 到落盘]]
- [[#2. O_DIRECT：绕过 page cache 意味着什么]]
- [[#3. 对齐要求与对齐内存的写法]]
- [[#4. fsync 家族：持久化阶梯逐级看]]
- [[#5. 为什么 append 场景 fdatasync 省不了多少]]
- [[#6. 五组写策略实验的设计要点]]
- [[#7. 观测与佐证：iostat 看写路径]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 2 - O_DIRECT + 持久化语义]] 的总前置知识：动手前通读 0-2 节建立写路径图景，改代码前看 3 节，做五组写策略实验前看 4-6 节，跑实验时对照 7 节。深挖版见 [[O_DIRECT 与持久化语义专题]]。

---

> 上周回答的是"读延迟差在哪"；本周回答两个写侧的生死问题：**write 返回后掉电，数据还在吗**；**O_DIRECT 到底绕过了什么、要付什么代价**。这两个问题答错，数据库就会丢数据——所以它们是存储岗面试的第一高频区。

---

## 0. 先建立直觉：write 返回的那一刻，数据在哪

做一次最朴素的观察：

```bash
# 写 1 GiB，write 循环几乎瞬间返回
dd if=/dev/zero of=testfile bs=1M count=1024
# 但此刻看 iostat，盘上的写流量还在持续——内核在背后慢慢刷
iostat -x 1
```

`write` 返回只意味着数据进了 page cache 并被标记为脏页（dirty）；真正写到盘上是内核 writeback 线程稍后异步做的。**此刻掉电，这 1 GiB 全部丢失，而应用以为自己写成功了。**

> [!important] 第一性原理
> 本周所有实验围绕一个问题：**每一级"写完了"分别意味着什么、各要多少钱**。从 write 返回（免费但不保证）到每笔 fsync（最贵但最强），中间的每一档都是吞吐和安全的交换。数据库设计的核心权衡之一就在这条阶梯上。

## 1. 写路径全景：从 write 到落盘

| 阶段 | 发生什么 | 掉电后果 |
|---|---|---|
| write 返回 | 数据进 page cache，页标脏 | 全丢 |
| writeback 触发 | 脏页比例/年龄超阈值，内核异步刷 | 已刷部分保住 |
| fsync 返回 | 强制刷该文件脏页 + 元数据 + 向设备发 cache flush | 数据安全（正常语义下） |

writeback 的触发点由 `vm.dirty_background_ratio`（后台开始刷）和 `vm.dirty_ratio`（写进程被阻塞强制刷）控制——这两个参数在 S-Week 9 的毛刺注入实验里会再见面。

注意最后一步"设备 cache flush"：数据到了盘，还可能停在盘内部的易失写缓存里。fsync 的完整语义包含向设备下发 flush 命令，这是它贵的主要原因。

## 2. O_DIRECT：绕过 page cache 意味着什么

`open(path, O_RDONLY | O_DIRECT)` 后，I/O 不经过 page cache，DMA 直接在用户 buffer 和设备之间搬数据：

- **读**：没有缓存命中这回事——所以"预热"无效，每次读都是真实设备延迟。这正是 benchmark 想要的确定性。
- **写**：不产生脏页，write 返回时数据已到设备（但可能在盘的写缓存里，见下）。
- **放弃的服务**：readahead、写合并、内核帮你处理非对齐——全都要自己来。

> [!warning] O_DIRECT ≠ 持久化
> 绕过 page cache 和落盘保证是两个正交的事。O_DIRECT 写完的数据可能停在设备易失缓存；文件大小等元数据也还在文件系统层。**要保证，照样 fsync/fdatasync。** 面试里把这两件事混为一谈是标志性扣分点。

谁在用 O_DIRECT：自管缓存的数据库（RocksDB、InnoDB 的 `innodb_flush_method=O_DIRECT`）——它们有自己的 buffer pool，不想让 page cache 再缓存一份（双重缓存浪费内存），也不想让内核的回写节奏干扰自己的刷盘策略。

## 3. 对齐要求与对齐内存的写法

O_DIRECT 的三对齐：**buffer 地址、文件偏移、I/O 长度**都必须是设备逻辑块大小的整数倍，违反任意一条 `EINVAL`：

```bash
cat /sys/block/nvme0n1/queue/logical_block_size   # 常见 512 或 4096
```

```cpp
void* raw = nullptr;
if (posix_memalign(&raw, 4096, block_size) != 0) {
    throw std::system_error(errno, std::generic_category());
}
// RAII：free 由智能指针接管
std::unique_ptr<void, decltype(&std::free)> buf(raw, &std::free);
```

实验纪律：故意做一次不对齐调用（比如 offset 加 512 而块大小 4096），把 EINVAL 记录进实验笔记——第一手证据比背书有说服力。工程上统一按 4096 对齐最稳，同时覆盖 512 和 4K 扇区盘。

## 4. fsync 家族：持久化阶梯逐级看

| 调用 | 刷什么 | 含设备 flush | 典型用途 |
|---|---|---|---|
| `fsync(fd)` | 数据 + 全部元数据 | 是 | 最强保证，WAL 提交点 |
| `fdatasync(fd)` | 数据 + 读回所必需的元数据（如 size） | 是 | 覆盖写场景省元数据开销 |
| `O_SYNC` 打开 | 每笔 write 自带 fsync 语义 | 是 | 免手动调用，但每笔都贵 |
| `sync_file_range` | 只发起/等待指定范围的**回写** | **否** | 回写调度工具，**不是持久化保证** |
| `sync()` | 全系统脏页 | 视实现 | 实验前清场，不用于程序内保证 |

sync_file_range 是最常被误用的：它不刷设备缓存、不管元数据，掉电照样丢。它的正确用途是控制回写节奏（比如避免脏页堆积后的突发抖动），不是替代 fsync。

fsync 为什么贵：一次完整的设备 cache flush。消费级 SSD 没有掉电保护电容，flush 是真刷介质，毫秒级；企业盘带电容（PLP），flush 可以立即应答——**同一段代码在两种盘上每笔 fsync 的吞吐能差一到两个数量级**，报告里必须写明盘的类别。

## 5. 为什么 append 场景 fdatasync 省不了多少

fdatasync 的省钱逻辑：跳过"与读回数据无关"的元数据（mtime 等）。但**文件大小属于必需元数据**——读回 append 的数据必须知道新的 size。于是：

- **append 写**：每笔都改 size → fdatasync 也得刷元数据 → 和 fsync 几乎一样贵。
- **原地覆盖写**（预分配后 pwrite 固定范围）：size 不变 → fdatasync 只刷数据 → 明显便宜。

这就是数据库 WAL 常见的"预分配 + 覆盖写"设计动机之一：把 fdatasync 的收益区间买回来。本周实验 B（每笔 fdatasync）和 C（每笔 fsync）的差距大小，直接验证这个机制——用 append 模式跑，两者差距应该很小；有余力加一组预分配覆盖写对照，差距就出来了。

## 6. 五组写策略实验的设计要点

周计划的 A-E 五组（仅 write / 每笔 fdatasync / 每笔 fsync / 每 100 笔 fsync / O_SYNC）设计上的关键点：

- **同一文件、同一笔数（4 KiB × 10000）**，只变同步策略——单变量原则。
- 每笔延迟逐条进 CSV：fsync 类策略的延迟分布重点看 p99（个别 flush 会撞上后台回写）。
- 总吞吐（笔/秒）与丢失窗口一起报告：D 策略吞吐接近 A，但掉电最多丢 100 笔——**这两个数字必须同时出现**，这就是 group commit 的完整表述。
- 预期量级（拿数据验证）：A 是内存速度；C 受设备 flush 时间限制，消费盘可能只有几百笔/秒；A vs C 差 1-2 个数量级。

$$
Throughput_{perFsync} \approx \frac{1}{T_{flush}}
$$

## 7. 观测与佐证：iostat 看写路径

写侧的 iostat 证据链（对照上周读侧）：

| 现象 | iostat 特征 |
|---|---|
| 仅 write（策略 A） | 程序结束后 w/s 仍持续一段——writeback 在善后 |
| 每笔 fsync（策略 C） | w/s ≈ 程序笔速 × 常数，f/s（flush 次数列，新版 iostat 有）同步出现 |
| 批量 fsync（策略 D） | 写流量呈周期脉冲 |

`grep Dirty /proc/meminfo` 在策略 A 运行时应看到 Dirty 值上涨、结束后回落——写侧版的"三重证据"。

## 8. 常见错误

- **把 O_DIRECT 当持久化**：绕过 cache ≠ 落盘保证，该 fsync 还得 fsync。
- **对齐只对了 buffer 地址**：offset 或 length 忘了对齐，EINVAL 找半天。
- **用 malloc 的 buffer 跑 O_DIRECT**：地址碰巧对齐时能跑通，换个环境就 EINVAL——必须 posix_memalign。
- **拿 sync_file_range 当轻量 fsync**：它不是持久化保证，语义完全不同。
- **append 模式下期待 fdatasync 大幅省钱**：size 元数据逃不掉，这是机制不是 bug。
- **测 fsync 吞吐时文件在 page cache 富余的机器上反复复用**：前一组的脏页回写会污染下一组，组间 sync + 间隔。
- **忘了记录盘有没有掉电保护**：同一实验在消费盘和企业盘上差几十倍，不写明盘型号的数据没有可比性。

## 9. 学习检查清单

- [ ] 能画出 write → page cache（脏页）→ writeback / fsync → 设备（含设备缓存 flush）的完整链。
- [ ] 能说出 O_DIRECT 放弃的三项服务和换回的两样东西。
- [ ] 能默写三对齐要求和查逻辑块大小的命令。
- [ ] 能按"刷什么 + 是否含设备 flush"区分 fsync / fdatasync / O_SYNC / sync_file_range。
- [ ] 能解释 append 场景 fdatasync ≈ fsync 的原因及预分配覆盖写的对策。
- [ ] 能说出 group commit 用什么换什么，并给出丢失窗口的量化表述。
- [ ] 知道设备掉电保护（PLP）对 fsync 成本的数量级影响。

## 10. 关键要点总结

- write 返回 = 数据进了脏页，掉电全丢；持久化是一条花钱的阶梯，不是默认品。
- O_DIRECT 交换的是服务换控制权，且与持久化正交——数据库用它是因为自管缓存，不是因为它快。
- fsync 贵在设备 cache flush；append 下 fdatasync 省不了 size 元数据。
- group commit = 丢失窗口换吞吐，两个数字必须一起报。
- 实验纪律延续上周：单变量、逐笔 CSV、分位数、iostat 旁证、环境（盘型号/PLP）记录在案。

## 关联知识

- [[S-Week 2 - O_DIRECT + 持久化语义]]（本篇服务的周计划）
- [[O_DIRECT 与持久化语义专题]]（深挖版与面试口述）
- [[S-Week 1 - 前置知识 - 环境搭建 + Page Cache 基线]]（page cache 与脏页基础）
- [[S-Week 1 - Page Cache 与 readahead 专题]]（被绕过的那一层）
- [[4.2 重定向、同步]]（sync 函数族笔记）
- [[存储引擎专题 - WAL 与 crash consistency]]（阶段 2 下游）

## 参考

- OSTEP 第 39/40/42 章（Files and Directories、FS Implementation、Crash Consistency）：[网站](https://pages.cs.wisc.edu/~remzi/OSTEP/)
- `man 2 open`（O_DIRECT/O_SYNC 段）、`man 2 fsync`、`man 2 fdatasync`、`man 2 sync_file_range`
- LWN: Ensuring data reaches disk（write 到落盘的权威梳理）
- 内核文档：Documentation/admin-guide/sysctl/vm.rst（dirty_ratio 族）
