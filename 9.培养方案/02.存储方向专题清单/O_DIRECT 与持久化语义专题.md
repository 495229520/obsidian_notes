---
title: O_DIRECT 与持久化语义专题
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向专题清单
roadmap_week: 阶段 0（S-Week 2；阶段 2 mini-kv-engine WAL 复用）
sort_order: "01.10"
status: active
---

# O_DIRECT 与持久化语义专题

> [!info] 所属路线
> - 培养方案阶段：阶段 0（S-Week 2 实验主线）；阶段 2 `mini-kv-engine` 的 WAL 与 fsync 策略直接建在本专题之上
> - 排序：01.10
> - 用途：把 [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]] 的 page cache 层从"读"翻到"写"：绕过它意味着什么、写进它之后数据什么时候才算安全。

> [!goal] 目标
> 讲清两组问题：O_DIRECT 绕过了什么、要什么、不保证什么；write 返回后数据在哪、fsync 家族的持久化阶梯各保证到哪一级。这是存储岗笔试面试的第一高频区，也是所有数据库 WAL 设计的语义地基。

---

## 1. O_DIRECT 的本质：放弃服务，换回控制权

`open` 加 `O_DIRECT` 后，读写**绕过 page cache**，DMA 直达块层。它放弃的服务和换回的东西：

| 放弃的（page cache 提供的） | 换回的 |
|---|---|
| 读缓存与 readahead | 延迟确定性（每次都真实到设备） |
| 写合并与异步回写 | 自己决定缓存什么（数据库自管 buffer pool） |
| 对齐宽容（内核帮你拼页） | 避免双重缓存浪费内存 |

所以 O_DIRECT 不等于"更快"：冷随机读它和 buffered 差不多，热读它反而慢几个数量级——它的正确用途是**自管缓存的数据库**（RocksDB / InnoDB）和**要测设备本身的 benchmark**。

## 2. 对齐三要素

buffer 地址、文件偏移、I/O 长度**三者都要对齐**到设备逻辑块大小（常见 512 或 4096，从 `/sys/block/<dev>/queue/logical_block_size` 查），违反任意一条得 `EINVAL`：

```cpp
void* buf = nullptr;
if (posix_memalign(&buf, 4096, block_size) != 0) { /* error */ }
// offset、length 同样按 4096 的倍数走
```

实践纪律（S-Week 2）：故意做一次不对齐调用，把 EINVAL 记录在案——面试讲对齐时有第一手证据。工程上按 4096 对齐最稳（覆盖 512e/4Kn 两类盘）。

## 3. 最容易翻车的一句：O_DIRECT 不等于持久化

O_DIRECT 只是绕过 **page cache**，数据到达的是设备，但可能停在**设备内部的易失写缓存**里；文件元数据（大小变化）也仍走文件系统的正常路径。所以：

- O_DIRECT 写完，掉电仍可能丢数据——**想要持久化保证，照样要 fsync / fdatasync**（它会向设备下发 cache flush，并把必要元数据落盘）。
- "绕过缓存"和"落盘保证"是两个正交的开关，面试里混为一谈直接扣分。

## 4. 持久化语义阶梯

从弱到强（每一级比上一级多保证什么）：

| 级别 | 操作 | 保证 | 不保证 |
|---|---|---|---|
| 0 | `write` 返回 | 数据进了 page cache（脏页） | 掉电全丢 |
| 1 | `sync_file_range` | 指定范围的脏页**发起/等待回写** | 不刷设备缓存、不管元数据——是回写调度工具，**不是持久化保证** |
| 2 | `fdatasync` | 数据 + 读回数据所必需的元数据（如 size）落盘，含设备 flush | mtime 等无关元数据 |
| 3 | `fsync` | 数据 + 全部元数据落盘，含设备 flush | — |

打开标志版：`O_SYNC` ≈ 每笔 write 自带 fsync；`O_DSYNC` ≈ 每笔自带 fdatasync。

append 与覆盖写的关键差别：append 每笔都改文件大小，size 属于"读回必需"的元数据，所以 **append 场景 fdatasync 省不了多少**；原地覆盖写才是 fdatasync 的收益区。这是 S-Week 2 实验 B/C 两组数据的解释。

## 5. fsync 的真实成本与 group commit

每笔 fsync 的延迟下限 ≈ 设备完成一次 cache flush 的时间，所以：

$$
IOPS_{fsync} \approx \frac{1}{T_{flush}}
$$

消费级 SSD（无掉电保护电容）每次 flush 是真刷介质，毫秒级 → 每笔 fsync 的写入只有几百 IOPS；企业盘带 PLP（电容保护）可以把 flush 当空操作应答，差距一到两个数量级。这就是"同一段代码在两台机器上吞吐差 50 倍"的标准答案。

**group commit**：攒 N 笔一起 fsync，吞吐近似乘 N，代价是"最近这一组"的丢失窗口——用**丢数据窗口**换**吞吐**。数据库的 `commit_delay`、`sync_binlog=N`、WAL 批量提交全是这个权衡。S-Week 2 的 D 策略就是它的最小实现，阶段 2 mini-kv-engine 的 WAL 会正式接手。

另一层埋点：journaling 文件系统（ext4）自己也在写日志。应用层 WAL 保证"应用数据的原子与持久"，文件系统 journal 保证"文件系统结构不烂"——两层各管各的，谁也替代不了谁。

## 6. 面试口述模板

```text
O_DIRECT 绕过 page cache 直达块层，代价是 buffer 地址、偏移、长度
三对齐，违反报 EINVAL；它换的是确定性和自管缓存的控制权，不是速度，
所以数据库自带 buffer pool 时才用它。另外它不等于持久化——数据可能
停在盘的易失缓存里，落盘保证还是要 fsync。持久化是个阶梯：write 只
进脏页；sync_file_range 只控制回写不是保证；fdatasync 刷数据加必需
元数据；fsync 全刷，两者都含设备 flush。append 场景 size 每笔都变，
fdatasync 和 fsync 差不多贵，覆盖写才省。每笔 fsync 的上限由设备
flush 时间决定，消费盘毫秒级、企业盘有电容近似免费——所以工程上用
group commit 拿丢失窗口换吞吐。这些我都有自己的五组对比数据。
```

追问预案：

- "write 返回后掉电，丢的是什么？" → 还在 page cache 的脏页全丢；文件系统结构本身由 journal 保护不烂，但你的数据没了。
- "为什么默认不设计成 write 即落盘？" → 写合并与异步回写的吞吐收益巨大；要强语义的少数派自己付 fsync 的钱。
- "O_DIRECT 读为什么预热无效？" → 根本不进 page cache，没有"热"这个状态。
- "fsync 了还会丢吗？" → 正常语义下不丢；但盘固件说谎（flush 应答但没刷）或文件系统 barrier 被关掉时会——这是 fsyncgate 类事故的背景，报告里声明假设即可。

## 关联知识

- [[S-Week 2 - O_DIRECT + 持久化语义]]（本专题服务的周计划）
- [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]]（入门版与实验流程）
- [[S-Week 1 - Page Cache 与 readahead 专题]]（被绕过的那一层）
- [[存储引擎专题 - WAL 与 crash consistency]]（阶段 2 的下游，语义在此复用）
- [[4.2 重定向、同步]]（sync 函数族的系统调用基础）
- [[00.存储方向专题清单索引]]
