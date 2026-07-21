---
title: S-Week 14 - WAL 与 fsync 策略
date: 2026-07-12
tags:
  - 高性能存储/存储方向阶段计划/计划
status: active
---

# S-Week 14 - WAL 与 fsync 策略

> [!goal] 本周目标
> 回答"put 返回时数据到底在哪"：给引擎定义显式的持久化契约，把 fsync 策略做成三档可配（always / everysec / os，对齐 Redis AOF 的三档），并讲清 bitcask 的 data log 本身为什么就是 WAL。S-Week 2 学的 fsync 家族第一次落在自己的写路径上。

## 学习目标

1. **为什么说 bitcask 的 log 就是 WAL？** WAL 的本质是"修改先顺序写日志、再生效"；bitcask 里 log 就是唯一的数据本体，内存索引只是 log 的缓存——重放即恢复，天然满足 write-ahead。
2. **write() 成功后数据在哪？** page cache。进程崩溃不丢（内核还活着，会照常 writeback）；断电或内核崩溃会丢。fsync 返回之后才到设备。
3. **三档 fsync 各承诺什么？** always：应答即落盘，最慢；everysec：后台每秒刷，最多丢约 1 秒；os：完全交给内核 writeback（默认 30 秒级），最快也最不保。
4. **fsync 和 fdatasync 差在哪？** fdatasync 不刷 mtime 这类非关键元数据；append 场景下文件大小是关键元数据，fdatasync 也会保证它落盘。
5. **rename 之后为什么要 fsync 目录？** rename 修改的是目录项，目录本身也是文件；不 fsync 目录，崩溃后可能看到旧目录内容——compaction 的原子切换必须补这一步。

## 1. 持久化契约成文（Day 1）

写 `docs/durability_contract.md`：一张"put 应答语义 × 三档策略 × 崩溃类型"的矩阵，每格写清"最多丢多少"。

| 策略 | 进程崩溃（kill -9） | 断电 / 内核崩溃 |
|---|---|---|
| always | 已应答零丢失 | 已应答零丢失（不考虑盘缓存作弊） |
| everysec | 已应答零丢失（page cache 仍会落盘） | 最多丢约 1 秒 |
| os | 已应答零丢失（同上） | 丢 writeback 窗口内全部 |

进程崩溃列为什么全是"零丢失"，想不通就回去翻 S-Week 2——这是 S-Week 16 崩溃注入实验设计的关键前提。

## 2. 实现（Day 2-4）

- 配置项 `sync_policy = always | everysec | os`。
- always：append 后 fdatasync 再应答。
- everysec：引入后台 flusher 线程（本项目第一根线程，只做定时 fsync；写路径保持单线程，为 S-Week 15 铺垫）。
- os：不主动刷。
- compaction 的 rename 后补目录 fsync。

## 3. 三档初测（Day 5）

- 单线程写吞吐 × 三档策略 × value 大小（128B / 4K / 64K），每组 3 次。
- 与 S-Week 2 实测的单次 fsync 延迟对账——always 档的吞吐上限应满足：

$$
IOPS_{always} \approx \frac{1}{t_{fsync}}
$$

对不上（数量级偏差）就找原因：盘缓存、云盘虚拟化层、value 太大变成带宽瓶颈。

## 4. 推理保温（约 25%）

- 维护态：harness 数据抽查一次，确认结果没有环境漂移。

## 5. 面试保底（约 15%）

> 阶段 2 八股按章清账第 3 讲。

- 算法（5-8 题）：回溯。参考 [[5.2.18 回溯算法]]（子集/排列/组合/棋盘类），配 [[CodeTop 高频题 Top300]] 同类题。
- 八股（1 章）：模板与类型转换。过 [[10.1 函数模版]]、[[10.2 类模版]]、[[13. 类型转换方式]]。验收：能讲清模板实例化发生在编译哪个阶段、四种 cast 各自的边界。
- 项目问答：10 个 Q&A（本周素材：持久化契约矩阵、fsync 家族、目录 fsync）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `docs/durability_contract.md` | 契约矩阵 | 每格有"丢多少"结论 |
| `sync_policy` 实现 + 单测 | 三档可配 | 行为与契约一致 |
| `results/fsync_policy/*.csv` | 三档 × value 大小初测 | 不手动修改 |

## 7. 验收标准

- [ ] 三档策略可配可测，行为与契约文档一致。
- [ ] always 档吞吐与 S-Week 2 的 fsync 延迟对上账（数量级）。
- [ ] 能按三档分别回答"put 返回后掉电，数据还在吗"。
- [ ] everysec 的丢失窗口有测试或推演。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- write 返回成功，数据此刻在哪？谁能把它弄丢？
- fsync、fdatasync、O_DSYNC、sync_file_range 各是什么语义？
- everysec 档最坏丢多少？怎么测出来？
- rename 原子替换之后为什么还要 fsync 目录？
- Redis AOF 的三档和你的实现有什么不同？

## 关联知识

- [[S-Week 13 - checksum 与崩溃恢复]]
- [[S-Week 15 - group commit 与多线程写入]]
- [[S-Week 14 - 前置知识 - WAL 与 fsync 策略]]
- [[存储引擎专题 - WAL 与 crash consistency]]
- [[S-Week 2 - O_DIRECT + 持久化语义]]（fsync 家族实测数据）
- [[O_DIRECT 与持久化语义专题]]
- Redis 文档：AOF 与 appendfsync 三档
