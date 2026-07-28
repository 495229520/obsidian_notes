---
title: S-Week 14 - 前置知识 - WAL 与 fsync 策略
date: 2026-07-12
tags:
  - 高性能存储
  - 存储方向参考资料
  - 计划
aliases:
  - 存储 Week 14 前置知识
  - fsync 策略前置知识
status: active
---

# S-Week 14 - 前置知识 - WAL 与 fsync 策略

## 索引

- [[#0. 先建立直觉：put 返回的那一刻，你敢承诺什么]]
- [[#1. WAL 的第一性原理与 bitcask 的特殊性]]
- [[#2. 三档策略：对齐 Redis AOF]]
- [[#3. fsync 家族复习与 append 场景的坑]]
- [[#4. 目录 fsync：最容易漏的一刀]]
- [[#5. everysec 的实现：项目的第一根线程]]
- [[#6. 契约矩阵怎么写]]
- [[#7. 初测设计：与 S-Week 2 对账]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 14 - WAL 与 fsync 策略]] 的总前置知识：写契约文档前读 0-2 节，动代码前读 3-5 节，跑初测前对照 7 节。持久化语义的完整地基在 [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]] 和 [[O_DIRECT 与持久化语义专题]]，本篇只补"用在自己引擎里"的增量。

---

> S-Week 2 你测过 fsync 家族的价格；本周把它变成**产品承诺**。区别在于：benchmark 里 fsync 慢一点只是数字难看，引擎里 fsync 的位置错一行就是丢用户数据。本周的核心交付不是代码，是一份**持久化契约**——put 返回时，引擎对数据在哪、什么情况下会丢，给出书面承诺。

---

## 0. 先建立直觉：put 返回的那一刻，你敢承诺什么

v0 的 put：`write` 返回就应答。此刻数据在 page cache——回忆 S-Week 2 的结论：

- **进程崩溃（kill -9）**：不丢。脏页属于内核，进程死了内核照常回写。
- **断电 / 内核崩溃**：丢。脏页没了，而你已经告诉客户端"成功"。

所以"put 成功"这句话在 v0 里是含糊的。本周把它变清楚：**应答语义 = 数据已按配置档位持久化**。三档，明码标价。

> [!important] 第一性原理
> 持久化不是布尔值，是一条**花钱的阶梯**。引擎的职责不是"永远最安全"，而是把阶梯明码标价、让使用者选，并且**兑现所选档位的承诺**。Redis、MySQL、PostgreSQL 全都这么做——没有一个默认最强档。

## 1. WAL 的第一性原理与 bitcask 的特殊性

WAL 的定义只有一句：**修改先顺序写日志并持久化，然后才算生效**。它同时买到顺序 I/O 的性能和"重放即恢复"的原子性。

bitcask 的特殊性要能一句话讲清（面试高频）：**它的 log 不是数据的影子，就是数据本身**——KeyDir 只是 log 的内存缓存。所以不存在"WAL 和数据文件双写"的开销，也不存在"WAL 写完、数据结构没更新"的危险窗口（重放会重建）。对照 RocksDB：WAL 保护的是 memtable 这段易失内存，SSTable 落盘后对应 WAL 才能回收——两种架构的对照表在 [[存储引擎专题 - WAL 与 crash consistency]]。

## 2. 三档策略：对齐 Redis AOF

| 档位 | 动作 | 断电承诺 | 参照 |
|---|---|---|---|
| always | 每条 append 后 fdatasync 再应答 | 已应答零丢失 | Redis `appendfsync always` |
| everysec | 后台线程每秒 fdatasync 一次 | 最多丢约 1 秒 | Redis `appendfsync everysec`（其默认档） |
| os | 不主动刷，交给内核 writeback | 丢 writeback 窗口（默认 30 秒级）内全部 | Redis `appendfsync no` |

对齐工业命名不是偷懒，是让面试官立刻听懂你的三档在说什么，并且能追问到 Redis 的实现细节（everysec 在后台线程刷、主线程发现上次刷还没完成会阻塞——这个细节本周实现时会亲身遇到）。

## 3. fsync 家族复习与 append 场景的坑

结论从 S-Week 2 直接搬，两条本周真正踩到的：

- **fdatasync vs fsync**：fdatasync 跳过非关键元数据（mtime 等），但 **append 每笔都改文件 size，size 是"读回必需"的元数据，fdatasync 也必须刷它**——所以 append 场景两者几乎一样贵。想买回 fdatasync 的收益要预分配 + 覆盖写（fallocate 后 pwrite），这是 design_note 里值得记一笔的备选方案（v1 不做，说清为什么：实现复杂度 vs 我的吞吐目标）。
- **O_DSYNC 打开标志**：等价于每笔 write 自带 fdatasync，可以替代 always 档的显式调用——少一次系统调用，但灵活性差（没法运行时切档）。作为 design_note 的备选记录。

## 4. 目录 fsync：最容易漏的一刀

`fsync(fd)` 刷的是**文件内容和它自己的元数据**，不包括"目录里有没有这个文件名"——目录项属于**目录文件**。两个场景必须补目录 fsync：

1. **创建新 log 文件后**（文件本身 fsync 了，目录项没持久化 → 崩溃后文件"消失"）；
2. **merge 的 rename 切换后**（rename 改的就是目录项）。

做法：`open` 目录本身（`O_RDONLY | O_DIRECTORY`）拿 fd，对它 fsync。这一刀漏掉的后果是"数据都在但文件找不到了"——比丢数据更诡异的故障现场，fsyncgate 类事故的常客。

## 5. everysec 的实现：项目的第一根线程

这是 mini-kv-engine 第一次引入并发，把它做小：

- flusher 线程只做一件事：循环 `sleep 1s → 若有新数据则 fdatasync`。
- 与写路径的共享状态压到最少：一个"上次刷到的 offset"或干脆一个 dirty 标志 + mutex；**写路径本身保持单线程**（多线程写是 S-Week 15 的主题，别提前打开这扇门）。
- 关闭路径要正确：析构时通知 flusher 退出（条件变量 + stop 标志）、join、最后补一次 fsync——退出时丢数据是最难看的 bug。
- 用 `std::jthread` + `std::stop_token`（C++20）能把"通知退出"写得非常干净，正好是 [[17.8 Modern C++ 同步设施与实战并发模式]] 的实战位。

## 6. 契约矩阵怎么写

`durability_contract.md` 的骨架——每格必须是可验证的陈述，不是形容词：

| 策略 | 进程崩溃（kill -9） | 断电 / 内核崩溃 |
|---|---|---|
| always | 已应答零丢失 | 已应答零丢失（假设设备 flush 诚实） |
| everysec | 已应答零丢失 | 丢最近 ≤ 1 秒已应答写入 |
| os | 已应答零丢失 | 丢 writeback 窗口内全部 |

三个纪律：进程崩溃列全是"零丢失"（page cache 由内核善后——想不通回 S-Week 2）；"假设设备 flush 诚实"这句边界要写（消费盘固件说谎的 fsyncgate 背景）；每格在 S-Week 16 都会变成崩溃注入的一条校验预期——**现在写的承诺，两周后自己验收**。

## 7. 初测设计：与 S-Week 2 对账

单线程写吞吐 × 三档 × value 大小（128 B / 4 KiB / 64 KiB），每组 3 次。核心对账：always 档吞吐应满足

$$
IOPS_{always} \approx \frac{1}{t_{fsync}}
$$

其中单次 fsync 延迟直接用 S-Week 2 的实测值。对不上数量级就找原因：云盘虚拟化层把 flush 应答造假、value 大到变成带宽瓶颈、everysec 线程抢了写路径的锁。os 档应接近纯内存速度、everysec 居中——三条曲线的相对形态本身就是正确性的旁证。

## 8. 常见错误

- **应答在 fsync 之前**：always 档最典型的语义 bug，崩溃注入周必被抓——写路径顺序固定为 append → fsync → 应答。
- **fsync 了文件、忘了目录**：新建文件和 rename 之后各补一刀目录 fsync。
- **everysec 用 sleep 轮询 dirty 标志却不加同步**：数据竞争；标志用 atomic 或 mutex 保护。
- **退出时不 join flusher**：进程正常退出反而丢最后一秒数据。
- **把 O_DIRECT 和持久化混为一谈**：绕过 page cache ≠ 落盘保证（S-Week 2 的第一大坑，面试标志性扣分点）。
- **契约里写"尽量不丢"**：契约每格必须可验证——"最多丢 1 秒"能测，"尽量"不能。
- **拿 everysec 的吞吐宣传、闭口不提丢失窗口**：两个数字必须一起报，这是 benchmark 报告纪律（S-Week 2 就立过的规矩）。

## 9. 学习检查清单

- [ ] 能一句话讲清"bitcask 的 log 就是 WAL"以及它消掉了哪个危险窗口。
- [ ] 能按三档 × 两种崩溃类型填满契约矩阵，每格说出依据。
- [ ] 能解释 append 场景 fdatasync ≈ fsync 的机制和预分配对策。
- [ ] 能说出哪两个时刻必须 fsync 目录、漏掉的故障现场长什么样。
- [ ] flusher 线程的启动/退出路径正确（stop 通知、join、补刷）。
- [ ] always 档吞吐与 S-Week 2 的 fsync 延迟对上账。
- [ ] 知道 Redis 三档的名字和 everysec 的阻塞细节。

## 10. 关键要点总结

- 持久化是明码标价的阶梯，引擎的职责是兑现所选档位的承诺——契约先行，代码兑现。
- bitcask 的 log 即数据本体：无双写开销、无"日志与结构不一致"窗口。
- append 场景 size 元数据逃不掉，fdatasync 省不了多少；目录 fsync 是最容易漏的一刀。
- everysec 用一根职责最小的后台线程实现，退出路径的正确性和运行时同样重要。
- 三档初测与 S-Week 2 数据对账——引擎的数字要能被自己的旧实验解释。

## 关联知识

- [[S-Week 14 - WAL 与 fsync 策略]]（本篇服务的周计划）
- [[S-Week 2 - 前置知识 - O_DIRECT + 持久化语义]]（fsync 家族地基）
- [[O_DIRECT 与持久化语义专题]]（持久化阶梯与 group commit 深挖）
- [[存储引擎专题 - WAL 与 crash consistency]]（崩溃窗口全景）
- [[17.8 Modern C++ 同步设施与实战并发模式]]（jthread/stop_token）
- [[S-Week 16 - 崩溃注入 1000 次]]（契约的最终验收）

## 参考

- Redis 官方文档：Persistence（AOF 三档与 everysec 实现说明）
- LWN: *Ensuring data reaches disk*（write 到落盘的权威梳理）
- `man 2 fsync`、`man 2 fdatasync`、`man 2 open`（O_DSYNC/O_DIRECTORY 段）
- PostgreSQL 文档：`wal_sync_method`（多档同步方式的工业对照）
