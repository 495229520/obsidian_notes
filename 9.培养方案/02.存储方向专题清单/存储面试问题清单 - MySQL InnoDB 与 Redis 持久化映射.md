---
title: 存储面试问题清单 - MySQL InnoDB 与 Redis 持久化映射
date: 2026-07-21
tags:
  - 高性能存储
  - 存储方向专题清单
  - 清单
roadmap_week: 求职全程（12 月投递前八股清账时过一遍）
sort_order: "99.50"
status: active
---

# 存储面试问题清单 - MySQL InnoDB 与 Redis 持久化映射

> [!info] 本篇定位
> 国内 infra/存储岗面试常掺 MySQL/Redis 八股。**不要当新知识学**——InnoDB 和 Redis 的持久化设计几乎每一条都是你在存储引擎主线里已经实现或实验过的机制换了个名字。本篇是一张"翻译表"：把已会的存储引擎语言翻译成面试官的数据库语言。背景不够时回 [[存储引擎专题 - WAL 与 crash consistency]] 和 [[LSM-tree 与 B+ tree 专题]]。

---

## 1. InnoDB 映射表

| 八股名词 | 你已有的对应知识 | 一句话答法 |
| --- | --- | --- |
| redo log | WAL（[[6.1 WAL 与 Crash Consistency]]） | 物理日志先顺序落盘再改数据页，崩溃后重放到最新一致状态；循环写文件组，checkpoint 推进后旧日志可覆盖——即 [[7.19 副本落后与日志截断]] 里"日志截断"的单机版。 |
| redo 组提交 | group commit（[[S-Week 15 - group commit 与多线程写入]]） | 多事务攒一批共享一次 fsync，用单次提交延迟换整体吞吐；你有自己实测的延迟-吞吐曲线可讲。 |
| undo log + MVCC | 快照与序列号（[[6.15 Snapshot 与 Sequence Number]]） | undo 保留旧版本供快照读，事务按版本链回溯到自己可见的版本；等价于 LSM 里 sequence number + snapshot 的可见性判断。 |
| buffer pool | 自管缓存，绕过 Page Cache（[[1.3 Buffered IO 与 O_DIRECT]]） | InnoDB 用 O_DIRECT + 自己的 LRU 变种管缓存，避免与 Page Cache 双份缓存和不可控回写；这是"绕过慢层"杠杆的教科书案例。 |
| 脏页刷盘 / checkpoint | writeback 与恢复起点（[[6.17 Recovery 路径]]） | 脏页后台异步刷，checkpoint 记录"此前的 redo 都已落盘"，恢复只需从 checkpoint 重放——回答"redo 会不会无限长"就用它。 |
| doublewrite buffer | torn write 防护（[[1.12 Flush FUA Barrier 与设备易失缓存]]、[[存储引擎专题 - WAL 与 crash consistency]]） | 【事实】InnoDB 页是 16K，设备原子写单位通常更小，断电可能写半页（torn page）；先把整页写到 doublewrite 区再写原位，恢复时用完整副本修复。redo 无法独立解决，因为物理 redo 需要一个未撕裂的页做基底。 |
| B+ 树聚簇索引 | [[LSM-tree 与 B+ tree 专题]] | OLTP 读多且要范围扫，B+ 树读放大低、原地更新；LSM 用读放大换写吞吐。答"为什么 MySQL 不用 LSM"就是这张三角权衡表。 |
| binlog 与 redo 的两阶段提交 | 无直接对应，概念级记忆 | redo 是引擎层物理日志，binlog 是 server 层逻辑日志（复制/订阅用）；两者必须原子一致，内部用 prepare→写 binlog→commit 的两阶段协议，崩溃恢复按"binlog 里有没有"决定提交还是回滚。 |
| 崩溃恢复流程 | WAL 重放 + 幂等（[[6.18 WAL 回放幂等性]]） | 先 redo 前滚到崩溃点，再用 undo 回滚未提交事务；与你 mini-kv-engine 的"重放 + 截断尾部损坏记录"同构，多了回滚阶段。 |

## 2. Redis 映射表

| 八股名词 | 你已有的对应知识 | 一句话答法 |
| --- | --- | --- |
| AOF appendfsync 三档（always / everysec / no） | fsync 三档策略（[[1.4 fsync fdatasync 与持久化语义]]、[[S-Week 14 - WAL 与 fsync 策略]]） | always 每条命令 fsync，安全但延迟由 fsync 主导；everysec 后台每秒 fsync，崩溃丢约一秒；no 交给内核回写，丢数窗口不可控。与 mini-kv-engine 三档持久化契约一字不差，报自己的实测量级即可。 |
| AOF rewrite | compaction / 日志压缩（[[LSM-tree 与 B+ tree 专题]]、[[6.11 MemTable Immutable 与 SST 生命周期]]） | 日志无限增长后，按当前内存状态重写一份最小命令集替换旧文件——就是 append-only 系统"用重写换空间放大"的通用套路，bitcask 的 merge 同理（[[存储引擎专题 - bitcask 与哈希索引]]）。 |
| RDB：fork + COW | 写时复制（[[0.2 虚拟内存 Page Table 与 TLB]]） | fork 子进程共享页表打快照，父进程继续写、内核按页 COW。【取舍】代价：fork 瞬间复制页表（大实例可达可感知的停顿量级）；快照期间写入越多，COW 复制的页越多，极端时内存接近翻倍。 |
| RDB vs AOF 怎么选 | 快照 vs 日志的通用权衡 | RDB 恢复快、丢数窗口大；AOF 丢数窗口小、文件大恢复慢；混合持久化 = RDB 基底 + 增量 AOF，等价于"checkpoint + WAL 增量"的标准组合（[[6.17 Recovery 路径]]）。 |
| 单线程为什么快 | 事件循环与 Reactor（[[2.2 io_uring 与 epoll 对比]]） | 纯内存操作 + epoll 事件循环，瓶颈在网络往返不在 CPU；单线程免锁免上下文切换。Redis 6 的 IO 线程只并行化读写 socket 与协议解析，命令执行仍单线程。 |
| 大 key / 慢命令阻塞 | 队列与利特尔法则（总纲分析框架） | 单线程即单队列：一个 O(N) 命令占住执行线程，后面全部排队，p99 立刻反映——与块层一个慢请求抬高整队延迟同构（[[9.5 p99 毛刺定位]]）。 |

## 3. 高频追问的完整答法（三道示例）

**"redo log 和 binlog 什么区别？"**
层次不同（引擎层 vs server 层）、内容不同（物理页改动 vs 逻辑变更）、用途不同（崩溃恢复 vs 复制与订阅）、写法不同（循环覆盖 vs 追加归档）。因为是两份独立日志，才需要内部两阶段提交保证原子一致。

**"everysec 到底会丢多少数据？"**
先给结论：约一秒窗口，再给机制：后台线程每秒 fsync 一次，主线程只 append 到用户态缓冲/文件；再给边界【事实】：如果上一次 fsync 卡住（磁盘忙），Redis 会阻塞写入而不是无限堆积——这一步能把话题引回自己的 fsync 延迟实测和 [[9.1 fio Benchmark Matrix]]。

**"为什么 InnoDB 不直接依赖 Page Cache？"**
三点：双缓存浪费内存；内核回写时机不可控，无法保证"redo 先于数据页落盘"的顺序约束（WAL 正确性依赖写顺序，见 [[1.12 Flush FUA Barrier 与设备易失缓存]]）；数据库比内核更懂自己的访问模式，LRU 变种能抵抗全表扫描污染。

## 4. 使用方式

- 12 月投递前八股清账时过一遍本表，确认每行都能从"已有知识"侧脱稿讲出，而不是背右列。
- 面试中被问到任何一行，答完标准答案后主动挂回自己的项目：「这个机制我在 mini-kv-engine 里实现/实测过……」——把八股题变成项目题，是这张表的真正目的。
