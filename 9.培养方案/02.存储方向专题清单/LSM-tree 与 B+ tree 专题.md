---
title: LSM-tree 与 B+ tree 专题
date: 2026-07-12
tags:
  - infra
  - 存储
  - 存储引擎
  - 面试
roadmap_week: "阶段 2（S-Week 17 RocksDB 对照；DDIA 第 3 章下半）"
sort_order: "03.20"
status: active
---

# LSM-tree 与 B+ tree 专题

> [!info] 所属路线
> - 培养方案阶段：阶段 2 S-Week 17（benchmark 收口周，对照 RocksDB 写差异表时的理论底稿）；即使 mini-kv-engine v2（LSM）被砍，本专题也必须过——"LSM 为什么写快读慢"是总纲点名的面试题
> - 排序：03.20
> - 用途：把 DDIA 第 3 章下半收敛成一张图 + 一个三放大框架，并给出"我的 bitcask 和 RocksDB 差在哪"的对照维度。

> [!goal] 目标
> 讲清三件事：两大索引家族的结构与读写路径差异；读/写/空间三放大为什么不可同时最小；bloom filter、leveled vs tiered compaction 各自补救什么。落点是能对照 RocksDB 说清自己实现的差距。

---

## 1. 一张图：原地更新 vs 追加合并

![[图片/9.培养方案/02.存储方向专题清单/9_2_6_1.svg|920]]

- **B+ tree**：数据按页组织成平衡树，写 = 定位到页原地修改（页满则分裂），读 = 根到叶一条路径。磁盘上是**随机写**，但读路径最短且天然支持范围扫描。InnoDB、LMDB、etcd 的 bbolt 都是这一族。
- **LSM-tree**：写只进内存 memtable（+WAL 保命），写满了整体顺序刷成不可变 SSTable，后台 compaction 逐层归并。磁盘上**只有顺序写**，代价是读要逐层找、compaction 反复搬数据。RocksDB、LevelDB、Cassandra 都是这一族。

## 2. LSM 为什么写快读慢（总纲原题）

**写快**的三层原因：

1. 关键路径上只有"append WAL + 插 memtable"，没有任何磁盘随机写。
2. flush 是整块顺序写，吃满顺序带宽。
3. 随机更新被 memtable 天然合并（同 key 多次改只 flush 最后一次）。

**读慢**的机制：一个 key 可能在 memtable、也可能在任何一层 SSTable——最坏要逐层查（L0 各文件还可能重叠，每个都要看）。这就是**读放大**。

**bloom filter 补救什么**：每个 SSTable 带一个位图摘要，几乎零成本回答"这个 key **肯定不在**本文件"。它把"不存在的 key"和"在深层的 key"的无效文件访问挡在内存里——补救的是读放大里"白查一层"的那部分；对"key 确实在很深的层"帮助有限，且有误报率（约 1% 时每 key 约 10 bit）。范围查询它帮不上忙（无法对区间做摘要）。

## 3. 三放大三角

| 放大 | 定义 | LSM | B+ tree | bitcask |
|---|---|---|---|---|
| 写放大 | 物理写字节 ÷ 逻辑写字节 | compaction 反复搬，leveled 常见 10-30 倍 | 页粒度写（改 1 行写 1 页）+ 分裂 | 接近 1（顺序 append），merge 时一次性付 |
| 读放大 | 一次读碰的物理位置数 | 逐层查，靠 bloom filter 压 | 树高（3-4 层，上层常驻缓存） | 1（索引在内存） |
| 空间放大 | 物理占用 ÷ 逻辑数据量 | 旧版本滞留待 compaction | 页内碎片（填充率约 2/3） | 垃圾滞留待 merge |

**三者不可同时最小**——每个引擎都是在三角里选边：

- **leveled compaction**（RocksDB 默认）：每层有序不重叠，空间放大小（约 1.1 倍）、读放大小，但写放大最大——每次下推要和下层重写。
- **tiered compaction**（Cassandra 风格）：同层攒多个 run 再一起合并，写放大小，但空间放大和读放大都大。
- bitcask 把读放大和写放大都压到 1，代价转移到**内存**（key 全驻留）——三角之外的第四种资源。

## 4. WAL 在两族里的角色

- B+ tree 族：WAL（redo log）保护"页的原地修改"——先记日志再改页，崩溃后 REDO；脏页由 checkpoint 节奏刷盘。
- LSM 族：WAL 只保护 memtable 这一小段内存；SSTable 本身不可变、天然崩溃安全，flush 完成对应 WAL 即可回收。
- bitcask：log 即数据，WAL 与数据本体合一（见 [[存储引擎专题 - WAL 与 crash consistency]]）。

## 5. 对照 RocksDB：差异表维度（S-Week 17 交付）

写"我的实现 vs RocksDB"时按这六个维度逐行对：

| 维度 | mini-kv-engine | RocksDB |
|---|---|---|
| 索引结构 | 内存 hash，全量驻留 | memtable(skiplist) + 分层 SSTable + block index |
| WAL 组织 | data log 即 WAL | 独立 WAL，flush 后回收 |
| 提交路径 | 单 writer + group commit | write group（leader 代写，JoinBatchGroup） |
| 恢复 | 全量扫描 + CRC 截断 | WAL 重放 + MANIFEST 定位版本 |
| 空间回收 | 离线 merge（可选实现） | 后台多线程 compaction，leveled/tiered 可配 |
| 读优化 | 无需（读一跳） | bloom filter + block cache + prefix seek |

差距的诚实说法：不是"没做"，而是"我的模型把这些复杂度用内存换掉了；当 key 装不下内存或需要范围扫描时，这六行就是要补的全部工程量"。

## 6. 面试口述模板

```text
LSM 写快是因为关键路径上只有 append WAL 和插 memtable，磁盘只做顺序
写，随机更新还被 memtable 合并掉；读慢是因为 key 可能在任何一层，要
逐层找，这叫读放大，bloom filter 用每 key 十来个 bit 把"肯定不在"
的文件挡在内存里，但它救不了范围查询。工程上是三放大三角：leveled
compaction 空间小读快但写放大重，tiered 反过来。B+ tree 押注读路径：
原地更新、根到叶一条路，代价是磁盘随机写和页分裂。我的 bitcask 是
第三极——用内存索引把读写放大都压到 1，代价是 key 必须全装内存。
对照 RocksDB 我能按索引、WAL、提交、恢复、compaction、读优化六个
维度说出差距，以及每个差距是被什么资源换掉的。
```

追问预案：

- "写放大 10-30 倍怎么来的？" → leveled 下每层容量约 10 倍递增，一条数据平均被逐层重写一次，层数 × 每次下推的归并重写 ≈ 10-30。
- "L0 为什么特殊？" → 直接由 memtable flush 而来，文件间 key 区间重叠，读要每个都查；L0 文件数超阈值会触发写限速（write stall）。
- "bloom filter 的误报有什么后果？" → 白读一次盘，不影响正确性；误报率和每 key 位数指数相关。
- "什么时候 B+ tree 明确更好？" → 读多写少、强范围查询、要求延迟平稳（无 compaction 抖动、无 write stall）。

## 关联知识

- [[S-Week 17 - benchmark 与 design_note 收口]]（本专题服务的周计划）
- [[存储引擎专题 - bitcask 与哈希索引]]（谱系第三极）
- [[存储引擎专题 - WAL 与 crash consistency]]（WAL 角色差异）
- [[5.2.13 红黑树]]（内存平衡树对照：为什么磁盘上用 B+ 不用二叉）
- DDIA 第 3 章下半；RocksDB Wiki（Leveled Compaction / Bloom Filter / MANIFEST）
- [[00.存储方向专题清单索引]]
