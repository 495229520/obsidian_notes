---
title: S-Week 17 - benchmark 与 design_note 收口
date: 2026-07-12
tags:
  - 高性能存储/存储方向阶段计划/总结
status: active
---

# S-Week 17 - benchmark 与 design_note 收口

> [!goal] 本周目标
> 给 v1 出成绩单并收口：写吞吐 / 读延迟 / 恢复时间 × fsync 三档的 benchmark（纪律沿用 S-Week 3），`design_note.md` 定稿（每个决策的备选与取舍），对照 RocksDB 写差异表，README + 简历 bullet + Q&A 落地。本周结束 mini-kv-engine v1 达到总纲验收标准，v2（LSM）明确砍或留。

## 学习目标

1. **存储引擎 benchmark 报什么才可信？** 固定环境三元组（机器/内核/盘）+ 每组 3 次 + 报分布（p50/p99）不只报均值 + 恢复时间随 log 大小的曲线——与 S-Week 3 的 benchmark matrix 同一套纪律。
2. **恢复时间被什么主导？** 全量扫 log ≈ 顺序读吞吐 × log 大小 + 逐条 CRC 开销；工业缓解手段是 hint file / checkpoint——写差异表时对照 RocksDB 的 MANIFEST + SST。
3. **和 RocksDB 差在哪？** 差异表维度：索引结构、WAL 组织、group commit 实现、恢复机制、compaction、读/写/空间放大三角。
4. **LSM 为什么写快读慢、bloom filter 补救什么？** 即使 v2 不做，这道总纲面试题也必须能答——用 DDIA 第 3 章下半补上 SSTable / LSM / B-tree 对比。
5. **design_note 怎么写才加分？** 每个决策：问题 → 备选方案 ≥ 2 → 取舍依据（数据或引用）→ 已知局限。

## 1. benchmark 与 issue 复现（Day 1-2）

- 写 bench 工具，固定 workload：fillseq / fillrandom / readrandom / 恢复时间（不同 log 大小）。
- 矩阵：三档 fsync × value 128B / 4K / 64K，每组 3 次，产出曲线与结论表。
- issue reproduction \#1（阶段 2 验收项）：给 fio 或 RocksDB 复现一个已知 issue，时间盒 1 天，产出复现记录（环境、步骤、现象对照）。

## 2. RocksDB 对照与 DDIA 收尾（Day 3）

- DDIA 第 3 章下半：SSTable / LSM / B-tree 对比。
- RocksDB Wiki 关键页：WAL、Write Path（write group）、MANIFEST——只读关键路径，不通读源码。
- "我的实现 vs RocksDB"差异表落进 design_note。

## 3. 定稿与简历（Day 4-5）

- `design_note.md` 定稿；README（动机 / 架构图 / 实验索引 / 复现指引 / 局限声明 / Agent 使用声明）；`reproduce.sh` 全量跑通。
- v2（LSM）去留决策写进 README：时间不够就砍，砍要声明边界（"知道 LSM 是什么、为什么没做"）。
- 简历 bullet 加入 S2：

```text
实现带 crash consistency 的 KV 存储引擎（Modern C++）：bitcask 模型、
CRC 校验与损坏恢复、可配置 fsync 策略与 group commit，崩溃注入 1000 次
零已提交数据丢失；benchmark 覆盖三档持久化策略的吞吐/延迟/恢复时间。
```

- S2 全部 Q&A 并入 `interview_qa.md`；春招早批开始小规模投递（简历带上 S2）。

## 4. 推理保温（约 25%）

- 阶段中点对账：serving benchmark harness 完整复跑一遍 + 环境漂移检查，为 S-Week 22 的达标对账做准备。

## 5. 面试保底（约 15%）

> 阶段 2 八股按章清账第 6 讲 + 算法期中模拟。

- 算法：90 分钟限时模拟一场（组卷方式沿用 [[AI Infra 岗算法笔试保底清单]]）；[[CodeTop 高频题 Top300]] 前 150 进度盘点——未过半则后五周每周加量。
- 八股（1 章）：设计模式。过 [[16.1 单例模式]]、[[16.2 工厂模式]]、[[16.4 策略模式]]、[[16.5 装饰器模式]]。验收：每个模式说一个自己项目里的真实用例（`sync_policy` 就是策略模式）。
- 项目问答：S2 全部素材并入 `interview_qa.md`，随机抽 10 题脱稿答。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| bench 工具 + `results/bench/*` | 三档 × value 矩阵 + 恢复时间曲线 | 不手动修改 |
| `design_note.md`（定稿） | 全部决策 + RocksDB 差异表 | 每个决策有备选与取舍 |
| `README.md` + `reproduce.sh` | 项目门面 + 一键复现 | 新读者 15 分钟建立全貌 |
| issue 复现记录 | fio / RocksDB 任一 | 步骤可复跑 |
| 简历（更新） | S2 bullet + 投递启动 | Q&A 并入 interview_qa.md |

## 7. 验收标准（对照总纲 S2）

- [ ] 崩溃注入 1000 次零丢失结果并入报告。
- [ ] benchmark：写吞吐 / 读延迟 / 恢复时间 × fsync 策略完成。
- [ ] design_note.md 每个决策有备选方案和取舍。
- [ ] 能对照 RocksDB 说出自己实现和工业实现差在哪。
- [ ] LSM 写快读慢 / bloom filter 两道题能脱稿答。
- [ ] v2 去留决策与边界声明写进 README。
- [ ] issue reproduction 完成 1 个。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- 你的引擎和 RocksDB 最大的三个差距是什么？
- LSM 为什么写快读慢？bloom filter 补救了什么？
- 恢复时间怎么随数据量增长？工业上怎么优化？
- fillrandom 比 fillseq 慢吗？在你的引擎里为什么（不）？
- 读放大 / 写放大 / 空间放大，你的 bitcask 各占哪头？

## 关联知识

- [[S-Week 16 - 崩溃注入 1000 次]]
- [[S-Week 18 - NVMe 命令模型与本地基线]]
- [[S-Week 17 - 前置知识 - benchmark 与 design_note 收口]]
- [[LSM-tree 与 B+ tree 专题]]（RocksDB 对照的理论底稿）
- [[存储面试问题清单 - 存储引擎]]（Q&A 收口题库）
- [[存储性能分析专题 - fio 与 benchmark matrix]]
- [[benchmark 报告与可复现工程专题]]
- [[S-Week 7 - 简历化与投递启动]]（简历与投递方法）
- DDIA 第 3 章；RocksDB Wiki（WAL / Write Path / MANIFEST）
