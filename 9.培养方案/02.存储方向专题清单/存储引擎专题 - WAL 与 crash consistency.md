---
title: 存储引擎专题 - WAL 与 crash consistency
date: 2026-07-12
tags:
  - 高性能存储/存储方向专题清单
roadmap_week: 阶段 2（S-Week 13-16 实验主线）
sort_order: "03.10"
status: active
---

# 存储引擎专题 - WAL 与 crash consistency

> [!info] 所属路线
> - 培养方案阶段：阶段 2 `mini-kv-engine` 的核心主题，横跨 S-Week 13（checksum 恢复）、S-Week 14（fsync 策略）、S-Week 15（group commit）、S-Week 16（崩溃注入）
> - 排序：03.10
> - 用途：把 [[O_DIRECT 与持久化语义专题]] 的"持久化阶梯"接到存储引擎里：一次 put 从内存到"崩溃也不丢"的完整语义链，以及怎么证明它是对的。

> [!goal] 目标
> 讲清四件事：WAL 的第一性原理与 bitcask log 为什么天然是 WAL；一次 put 的崩溃窗口分别丢什么；checksum + 前缀截断怎么把损坏 log 恢复到一致；group commit 用什么换什么、崩溃注入怎么闭环验证。这是存储引擎面试的必考主线，也是 mini-kv-engine 最值钱的故事。

---

## 1. WAL 的第一性原理

WAL（write-ahead log）只有一句话：**任何修改先顺序写进 log 并持久化，然后才算生效**。它同时买到两样东西：

1. **性能**：把"改 B+ tree 页 / 改 memtable"这类随机写推迟，磁盘关键路径上只有顺序 append。
2. **原子性与可恢复性**：崩溃后重放 log 就能重建崩溃前的已提交状态；一条记录要么完整在 log 里，要么（靠 CRC）被识别为不完整而丢弃。

bitcask 的特殊之处：它的 data log **就是**唯一数据本体，内存 KeyDir 只是 log 的缓存——"WAL + 数据结构"合二为一，重放即恢复。RocksDB 则是"WAL + memtable + SSTable"三件套：WAL 保护 memtable 里还没 flush 的部分，SST 落盘后对应 WAL 就可以回收。

## 2. 一次 put 的崩溃窗口

![[图片/9.培养方案/02.存储方向专题清单/9_2_5_1.svg|900]]

对着图记三条规则（这就是 S-Week 16 崩溃注入的校验不变量）：

$$
committed \subseteq recovered \subseteq attempted
$$

- **append 中途崩**：盘上留半条记录（torn write），恢复扫描靠 CRC 识别并截断——允许，因为还没应答。
- **fsync 前崩**：断电丢 page cache 里的数据；kill -9 不丢（内核还活着会照常回写）。这个差别是崩溃注入实验设计的关键前提。
- **fsync 后、应答前崩**：数据在，客户端不知道——恢复后"多"出一条，语义上允许（客户端会重试，幂等性由上层处理）。
- **应答后崩**：必须恢复出来，丢一条就是 bug。

## 3. torn write、checksum 与前缀截断

- **torn write 怎么发生**：一条记录跨多个扇区/页，崩溃时只有部分扇区持久化。单扇区（512B/4K）写通常原子，跨界不保证。
- **checksum 防什么**：介质位翻转、torn write、错位读——一切"读出来 ≠ 写进去"。**防不了**：写之前内存里就坏的数据、逻辑 bug（写错 key）、整条记录干净消失（这要靠 WAL 序号/长度链，或上层对账）。
- **为什么截断而不是跳过**：append-only log 里一条坏记录之后没有可信的对齐锚点——你不知道下一条从哪开始；就算猜到了，"中间丢一条"会让后续重放建立在错误状态上。截断到最后一条完整记录，保证**前缀一致**。
- 工程细节：CRC 用 crc32c（SSE4.2 硬件加速，RocksDB 同款）；覆盖 header + payload、不含自身；先校验后使用。

## 4. 持久化契约：三档 fsync

把"put 返回意味着什么"写成显式契约（对齐 Redis AOF 的三档）：

| 策略 | 进程崩溃 | 断电 / 内核崩溃 | 成本 |
|---|---|---|---|
| always（每条 fdatasync） | 零丢失 | 零丢失 | 吞吐被设备 flush 时间封顶 |
| everysec（后台每秒刷） | 零丢失 | 最多丢约 1 秒 | 接近内存速度 |
| os（交给 writeback） | 零丢失 | 丢 writeback 窗口内全部 | 最快最不保 |

fsync/fdatasync 的语义差、append 场景 size 元数据逃不掉、目录 fsync（rename 后必须补）——全部沿用 [[O_DIRECT 与持久化语义专题]] 的结论，这里不重复。

## 5. group commit：吞吐从哪来、代价是什么

fsync 按次收费不按字节收费（小 IO 时），所以 always 档的吞吐上限是：

$$
IOPS_{always} \approx \frac{1}{t_{fsync}}
$$

group commit 让 N 条请求共享一次 fsync：单 writer 从队列取一批 → 一次 append → 一次 fdatasync → 集体唤醒。吞吐近似：

$$
T_{group} \approx \frac{B}{t_{fsync}}
$$

其中 B 是每批平均聚合条数。**代价**：每条请求的应答延迟加上攒批等待（max_batch / max_wait 双条件先到先触发）；批越大吞吐越高、p50 越差。RocksDB 的 write group（JoinBatchGroup）、MySQL 的 `sync_binlog=N`、PostgreSQL 的 `commit_delay` 全是同一个权衡。为什么必须单 writer：append log 的 offset 分配必须串行，单 writer + 队列把竞争点从文件挪到内存，请求还天然聚成批。

## 6. 怎么证明恢复是对的：崩溃注入方法论

- **不变量**：上面三条集合包含关系 + 完整性（恢复后每条记录 CRC 合法）。
- **oracle**：父进程 fork 引擎子进程，子进程每次应答后经管道上报 (key, seq)；oracle 活在父进程内存里，不随子进程死。
- **注入覆盖**：按操作计数随机 + 按时间随机两种模式，盯住危险窗口（append 后 fsync 前、fsync 后应答前、everysec 刷盘间隙、merge rename 中间态）。
- **边界声明**：kill -9 只测进程崩溃语义；断电语义要 dm-flakey / dm-log-writes / 虚拟机断电——报告里写明，面试里主动说。
- 1000 次零丢失是总纲 S2 的硬验收；注入出来的每个真 bug 都按"现象 → 根因 → 修法 → 回归测试"记录，比结果本身更值钱。

## 7. 工业对照速查

| 系统 | WAL 组织 | 提交策略 | 恢复 |
|---|---|---|---|
| mini-kv-engine | data log 即 WAL | 三档可配 + group commit | 全量扫描 + CRC 截断 |
| RocksDB | 独立 WAL 保护 memtable | write group，`sync` 可配 | WAL 重放进 memtable，MANIFEST 定位 SST |
| Redis AOF | 命令追加日志 | always / everysec / no | 重放命令流 |
| PostgreSQL | WAL 段文件 | `synchronous_commit` 多档 | REDO 到一致点 |

## 8. 面试口述模板

```text
我的引擎里 crash consistency 是一条完整链：写路径是序列化 → append
→ 按策略 fsync → 应答，四个间隙我都能说出崩溃后果——fsync 前崩断电
会丢但没应答所以允许，应答后必须恢复出来。恢复靠两样东西：每条记录
的 crc32c 识别 torn write，加前缀截断保证一致——不跳过坏记录继续，
因为后面没有可信锚点。持久化契约做成三档，always 档吞吐被设备 flush
封顶，所以用 group commit：单 writer 攒批共享一次 fsync，吞吐乘批
大小，代价是攒批等待进了每条的延迟。最后用崩溃注入闭环：fork + 随机
kill -9 一千次，按 committed ⊆ recovered ⊆ attempted 校验，零丢失。
我还能说清 kill -9 和断电的语义边界，以及注入挖出来的真 bug。
```

追问预案：

- "崩溃发生在 log 写完但 index 没更新时怎么办？" → 我的 index 在内存，重放即重建，这个窗口不存在；RocksDB 里对应"WAL 写完 memtable 没插入"，同样靠 WAL 重放解决——这是 WAL 的定义性收益。
- "checksum 校验过了就一定没问题吗？" → 只保证"读到的 = 写进去的"；写进去之前就错（内存损坏、逻辑 bug）它不管，那是 ECC 内存和测试的领域。
- "everysec 最多丢 1 秒，怎么验证？" → 崩溃注入统计 os/everysec 档的实际丢失量对照承诺窗口，有数据。
- "为什么不用 O_DIRECT 写 WAL？" → 可以（RocksDB 有选项），省 page cache 干扰但要自己对齐攒块；我的量级下 buffered + fdatasync 语义等价且实现简单，这是 design_note 里的一条显式取舍。

## 关联知识

- [[S-Week 13 - checksum 与崩溃恢复]] / [[S-Week 14 - WAL 与 fsync 策略]] / [[S-Week 15 - group commit 与多线程写入]] / [[S-Week 16 - 崩溃注入 1000 次]]（本专题服务的四周）
- [[O_DIRECT 与持久化语义专题]]（持久化阶梯地基）
- [[存储引擎专题 - bitcask 与哈希索引]]（log 即 WAL 的模型来源）
- [[LSM-tree 与 B+ tree 专题]]（WAL 在 LSM 里的角色）
- [[17.4 条件同步：条件变量与信号量]]、[[17.6 并发队列：有界队列与无锁队列]]（group commit 的并发原语）
- [[00.存储方向专题清单索引]]
