---
title: 存储引擎专题 - bitcask 与哈希索引
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向专题清单
  - 索引
roadmap_week: 阶段 2（S-Week 12 实验主线；S-Week 13-17 全程复用）
sort_order: "03.00"
status: active
---

# 存储引擎专题 - bitcask 与哈希索引

> [!info] 所属路线
> - 培养方案阶段：阶段 2 `mini-kv-engine` 的模型地基（S-Week 12 落地 v0，此后每周都建在它上面）
> - 排序：03.00
> - 用途：把 S1 学到的"顺序 I/O 快、随机 I/O 慢"从测量结论变成设计依据——bitcask 是把这条结论用到极致的最小存储引擎。

> [!goal] 目标
> 讲清三个问题：bitcask 为什么写快读也快；它付了哪三笔代价、各有什么对策；它在存储引擎谱系里的位置（什么时候选它、什么时候必须换 LSM/B+ tree）。这是 mini-kv-engine 面试叙事的第一章。

---

## 1. 模型全景：两层结构、三件套

![[图片/9.培养方案/02.存储方向专题清单/9_2_4_1.svg|900]]

bitcask（Riak 的默认引擎）只有三件套：

| 组件 | 位置 | 内容 |
|---|---|---|
| append-only log | 磁盘 | 唯一的数据本体，只在尾部写 |
| KeyDir（hash index） | 内存 | key → (file_id, offset, size)，永远指向最新版本 |
| merge/compaction | 后台 | 把散落的最新版本抄进新文件，回收垃圾 |

所有磁盘写都是顺序 append；所有读都是"查内存索引 + 一次 pread"。磁盘只做它最擅长的两件事。

## 2. 为什么写快、读也快

- **写快**：顺序 append 吃满顺序写带宽，没有原地更新、没有页分裂、没有先读后写。S-Week 1/3 的顺序 vs 随机数据在这里第一次变成设计依据。
- **读快**：hash 查找 O(1) 拿到精确 offset，一次 `pread` 直达——没有树的多层下探，没有 LSM 的逐层找。加上 page cache 帮忙，热 key 甚至不碰盘。
- **恢复语义简单**：log 就是全部真相，索引丢了重放即得（这也是它天然满足 WAL 语义的原因，见 [[存储引擎专题 - WAL 与 crash consistency]]）。

## 3. 三笔代价与对策

| 代价 | 机制 | 对策 |
|---|---|---|
| 全部 key 必须装进内存 | KeyDir 每个 key 一个条目（key 本身 + 约几十字节管理开销） | 只适合 key 集合有界的场景；key 装不下就换 LSM |
| 启动要全量扫 log | 重建 KeyDir 需逐条读 | hint file（merge 时顺手写一份"key → offset"的紧凑索引），恢复只读 hint |
| 空间放大 | overwrite/delete 的旧记录滞留 log | merge：把每个 key 的最新版本抄进新文件后原子切换 |

内存估算要会口算：N 个 key、平均 key 长 k 字节，KeyDir ≈ N × (k + 常数几十字节)。一亿个 16 字节 key 大约要几 GB 量级内存——这就是"能不能用 bitcask"的第一道判断题。

## 4. record 格式与 tombstone 的设计要点

- 定长头在前（crc / tstamp / key_size / value_size），先读头才知道这条记录多长——恢复扫描才能逐条前进。字节级布局见 [[S-Week 12 - 前置知识 - mini-kv-engine v0 与 bitcask 模型]]。
- delete 不能原地删（append-only），写一条 tombstone；重放遇到它就从索引移除。tombstone 本身也是垃圾，要等 merge 时真正消失——而且**必须等旧文件里的旧版本全部清掉之后**才能丢弃，否则删掉的 key 会"复活"。
- overwrite = append 新记录 + 索引改指向；旧记录原地变垃圾。空间放大从这里开始积累。

## 5. 谱系定位：什么时候选它

| 负载特征 | 选型 |
|---|---|
| 点查为主、key 集合装得进内存、value 可大可小 | bitcask 完胜（读一跳、写顺序） |
| 需要范围扫描 / 前缀扫描 | hash 索引无能为力 → B+ tree 或 LSM |
| key 多到内存装不下 | LSM（索引分层下盘）|
| 读延迟要求极稳、写不频繁 | B+ tree |

一句话总结谱系：**bitcask 把索引全放内存换读一跳；B+ tree 把索引放盘上原地更新换范围查询；LSM 把一切变成顺序写换写吞吐**（详见 [[LSM-tree 与 B+ tree 专题]]）。

## 6. 面试口述模板

```text
我的 mini-kv-engine 用的是 bitcask 模型：磁盘上只有一个 append-only
log，内存里一张 hash 索引指向每个 key 的最新 offset。写永远是顺序
append，读是查索引加一次 pread，所以写吃满顺序带宽、读只有一跳。
代价有三个：全部 key 要装进内存，我口算过内存上界；启动要全量重放，
工业上用 hint file 缓解；overwrite 留下垃圾，靠 merge 回收。删除写
tombstone，重放时从索引移除。它和 LSM 的分界在于 key 能不能装进内存
和要不要范围扫描——我的场景是点查为主，所以 bitcask 是正确起点，
而且它的 log 天然就是 WAL，crash recovery 的故事从第一天就是对的。
```

追问预案：

- "key 装不下内存怎么办？" → 换 LSM：索引分层放盘（SSTable + 稀疏索引），代价是读放大。
- "为什么不用 std::map？" → 不需要有序性，unordered_map 的 O(1) 更合适；要范围扫描时 hash 整个模型都不对，不是换 map 能救的。
- "启动重放太慢怎么优化？" → hint file / 定期 checkpoint 索引快照；恢复时间与 log 大小的实测曲线在 S-Week 17。
- "tombstone 什么时候能真正删掉？" → merge 时确认所有旧文件中该 key 的旧版本都已清除之后；提前删会让旧值复活。

## 关联知识

- [[S-Week 12 - mini-kv-engine v0 与 bitcask 模型]]（本专题服务的周计划）
- [[S-Week 12 - 前置知识 - mini-kv-engine v0 与 bitcask 模型]]（入门版与 record 格式细节）
- [[存储引擎专题 - WAL 与 crash consistency]]（log 即 WAL 的下游展开）
- [[LSM-tree 与 B+ tree 专题]]（谱系的另外两极）
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]（顺序 vs 随机的测量地基）
- [[00.存储方向专题清单索引]]
