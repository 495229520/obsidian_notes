---
title: S-Week 17 - 前置知识 - benchmark 与 design_note 收口
date: 2026-07-12
tags:
  - 高性能存储/存储方向参考资料/总结
aliases:
  - 存储 Week 17 前置知识
  - mini-kv 收口前置知识
status: active
---

# S-Week 17 - 前置知识 - benchmark 与 design_note 收口

## 索引

- [[#0. 先建立直觉：成绩单的可信度比数字大小值钱]]
- [[#1. 四个 workload：测什么、为什么是它们]]
- [[#2. 恢复时间：模型与测法]]
- [[#3. DDIA 第 3 章下半：阅读地图]]
- [[#4. RocksDB 对照读法：只读三个 Wiki 页]]
- [[#5. design_note 的决策模板]]
- [[#6. issue reproduction：四步法与时间盒]]
- [[#7. 简历 bullet 与 Q&A 沉淀]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 17 - benchmark 与 design_note 收口]] 的总前置知识：写 bench 工具前读 1-2 节，读 DDIA/RocksDB 前对照 3-4 节的地图，写 design_note 前用 5 节的模板。benchmark 纪律的完整版在 [[存储性能分析专题 - fio 与 benchmark matrix]] 与 [[benchmark 报告与可复现工程专题]]，本篇只讲存储引擎特有的部分。

---

> 收口周的产出决定这个项目在简历上的成色。一个残酷的事实：面试官不会跑你的代码，他们看的是**报告的可信度**（环境、方法、边界是否诚实）和**你对取舍的理解深度**（design_note 与 RocksDB 差异表）。数字本身反而是最不重要的——一个诚实的"消费盘上 800 IOPS"胜过一个可疑的"十万 IOPS"。

---

## 0. 先建立直觉：成绩单的可信度比数字大小值钱

S-Week 3 立过的规矩全部适用，搬过来就是：环境三元组（机器/内核/盘型号 + 有无 PLP）写在报告第一节；每组 ≥3 次报中位数与波动；逐条延迟进 CSV、分位数事后算；单变量原则——一次只动一个旋钮。存储引擎 benchmark 新增一条：**每个数字旁边标注 fsync 档位**。同一引擎 always 和 os 档差一两个数量级，不标档位的数字毫无意义。

## 1. 四个 workload：测什么、为什么是它们

命名向 RocksDB 的 `db_bench` 看齐（面试官秒懂）：

| workload | 内容 | 暴露什么 |
|---|---|---|
| fillseq | 顺序 key 逐条 put | 纯写路径的上限；对 bitcask 与 fillrandom 应几乎无差——**验证这一点本身就是实验结论**（append-only 对 key 顺序不敏感，对照 B+ tree 的行为差异） |
| fillrandom | 随机 key 逐条 put | 同上 + KeyDir 的 rehash 抖动是否可见 |
| readrandom | 均匀随机 get | 读路径：索引查找 + pread；配合冷/热 page cache 两种状态各测一轮（S-Week 1 的方法直接复用） |
| recovery | 不同 log 大小下的启动时间 | 恢复扫描的线性系数（见 2 节） |

矩阵：4 workload × 3 档 fsync × value 128 B / 4 KiB / 64 KiB。写侧重点看"三档的吞吐比值"是否与 S-Week 14/15 的机制预测一致；读侧 fsync 档位无关，可以砍维度——**会砍维度也是 benchmark 能力**，全笛卡尔积跑三天是新手行为。

## 2. 恢复时间：模型与测法

恢复 = 顺序读全量 log + 逐条解析校验 + 建索引，模型：

$$
T_{recover} \approx \frac{S_{log}}{B_{seqread}} + N \times c_{parse}
$$

S 是 log 字节数、B 是顺序读吞吐、N 是记录条数、c 是每条的解析+CRC+插索引开销。测法：预写出 100 MB / 1 GB / 4 GB 三档 log（value 大小固定），各测冷 cache 启动时间（`echo 3 > drop_caches` 后测——不清 cache 测出来的是内存解析速度，不是恢复时间）。

预期两个形态：小 value 时 N 大、第二项主导（CPU bound）；大 value 时第一项主导（盘带宽 bound）。画"恢复时间 vs log 大小"曲线并标出斜率含义——这条曲线直接回答面试题"恢复时间被什么主导"，也是 hint file 优化收益的定量依据（hint file 把第一项的 S 缩成索引大小）。

## 3. DDIA 第 3 章下半：阅读地图

带着三个问题读（每个问题读完要能写三句话答案）：

1. **SSTable 比原始 log 多了什么**：按 key 有序 → 归并像 merge sort、稀疏索引可行（每几 KB 一个索引项，不用每 key 一项——对照你的全量 KeyDir）。
2. **LSM 的读写路径**：写进 memtable（+WAL）、flush、逐层 compaction；读逐层找 + bloom filter。机制细节在 [[LSM-tree 与 B+ tree 专题]] 已沉淀，读书时对照校准。
3. **B-tree 与 LSM 的取舍段落**：DDIA 给的对比维度（写放大来源、崩溃恢复方式、碎片）直接抄进差异表的骨架。

## 4. RocksDB 对照读法：只读三个 Wiki 页

纪律：**读 Wiki 不读源码**（总纲的"不过早深挖"红线），每页限时一小时、带着"和我的实现差在哪"做笔记：

| Wiki 页 | 对照点 |
|---|---|
| Write Ahead Log + WAL Recovery Modes | WAL 文件组织（独立于数据）、四种恢复模式 vs 我的"CRC + 前缀截断"（对应它的 `kPointInTimeRecovery`） |
| Write Path（含 JoinBatchGroup） | write group 的 leader/follower vs 我的单 writer + MPSC 队列——形态不同、目标相同（攒批共享 fsync） |
| MANIFEST | 版本化元数据 vs 我的"重放即真相"——为什么它需要 MANIFEST（SST 文件集合在变）而我不需要 |

产出六维度差异表（索引/WAL/提交/恢复/空间回收/读优化），落进 design_note——模板见 [[存储面试问题清单 - 存储引擎]] Q8。

## 5. design_note 的决策模板

每个决策一节，五段式，全项目至少写满八个决策：

```text
## 决策：fsync 策略做成三档可配
问题：put 应答语义与吞吐的矛盾
备选：A. 固定 always（最安全）B. 固定 everysec（Redis 默认）
      C. 三档可配（选定）D. O_DSYNC 打开标志
取舍：C 让 benchmark 能量化每档的价格（本项目的实验属性优先）；
      D 省一次 syscall 但不能运行时切换，作为 always 档的
      实现细节备选记录
数据：三档吞吐对比见 results/bench/fsync_policy.csv
局限：everysec 的丢失窗口上界未在断电场景实测（见边界声明）
```

候选决策清单（对着过一遍，写过的打勾）：record 格式与长度上限 / 错误处理风格 / KeyDir 选型 / 墓碑表示 / CRC 选型与覆盖 / 截断 vs 跳过 / 三档 fsync / 单 writer + 有界队列 / 攒批参数 / 注入策略。**每条都有"备选 + 否决理由"才算 design_note，只有结论的叫使用说明**。

## 6. issue reproduction：四步法与时间盒

沿用推理版的 Reproduce → Minimize → Analyze → Contribute 四步法，本周只走前两步半：

- 选题：fio 或 RocksDB 的 GitHub issues 里挑**带复现步骤、近期活跃、与 I/O 行为相关**的（label: bug + 能在你的云主机复现的环境）；避开需要特殊硬件或 Windows 的。
- 时间盒**一天**：复现成功 → 写复现记录（环境、步骤、预期 vs 实际、日志片段）；复现不出来 → 记录"在我的环境不复现 + 差异猜测"，同样是合格产出，果断收手。
- 产出落在 `docs/issue_repro_01.md`，格式向"别人能照着跑"看齐。这是阶段 2 验收项（1-2 个）的第一个，也是后续开源贡献叙事的起点。

## 7. 简历 bullet 与 Q&A 沉淀

- bullet 写法沿用 [[S-Week 7 - 简历化与投递启动]] 的"动词 + 技术点 + 可验证数字"，S2 的模板已在周计划里给出——数字必须与报告一致，面试官会追问出处。
- Q&A 收口：把 [[存储面试问题清单 - 存储引擎]] 的十题全部过成"第一句结论 + 自己的数据"，并入 `interview_qa.md`；随机抽 10 题脱稿演练一遍，卡壳的题回炉对应周次。
- v2（LSM）去留写进 README：砍，就写"为什么砍 + 我知道要补什么"（边界声明的加分写法）；留，则明确排期不影响 S3 启动。

## 8. 常见错误

- **报告不标 fsync 档位 / 盘型号**：数字之间不可比，整份报告作废级错误。
- **恢复时间不清 page cache 就测**：测出来的是内存解析速度，虚快一个数量级。
- **fillseq 和 fillrandom 差距巨大却不解释**：对 bitcask 这不该发生——大概率是 KeyDir rehash 或测试代码的 key 生成开销混进来了，追根因就是实验结论。
- **design_note 只写结论不写备选**：变成使用说明，面试深挖一问就穿。
- **RocksDB 读进源码里出不来**：Wiki 三页、每页一小时，超时就停——总纲红线。
- **issue 复现无限加时**：时间盒一天，"不复现 + 差异记录"也是合格产出。
- **benchmark 期间后台还挂着崩溃注入循环**：S-Week 16 的脚本会污染数字，收口周环境要干净。
- **简历数字与报告不一致**：面试现场对不上出处，信任崩塌。

## 9. 学习检查清单

- [ ] 四个 workload 各自"暴露什么"能一句话说清。
- [ ] 恢复时间模型两项各自的主导条件能推导，曲线测法正确（冷 cache）。
- [ ] DDIA 三个问题各有三句话答案。
- [ ] RocksDB 三个 Wiki 页读完，六维度差异表填满。
- [ ] design_note 八个以上决策、每个五段式齐全。
- [ ] issue reproduction 一个完成（或"不复现"记录完成）。
- [ ] 十道引擎面试题脱稿过、简历 bullet 数字与报告一致。

## 10. 关键要点总结

- 收口的核心资产是可信度：环境三元组、fsync 档位标注、边界声明，一个都不能少。
- 四 workload 对齐 db_bench 命名；会砍维度和会跑矩阵同样重要。
- 恢复时间 = 顺序读项 + 逐条解析项，主导项随 value 大小切换——一条曲线答一道面试题。
- RocksDB 只读三个 Wiki 页；差异表的灵魂是"我的哪个假设换掉了它的哪块复杂度"。
- design_note 五段式：问题/备选/取舍/数据/局限——只有结论的不叫设计笔记。

## 关联知识

- [[S-Week 17 - benchmark 与 design_note 收口]]（本篇服务的周计划）
- [[存储性能分析专题 - fio 与 benchmark matrix]]（benchmark 纪律总纲）
- [[benchmark 报告与可复现工程专题]]（六段式报告与 reproduce.sh）
- [[LSM-tree 与 B+ tree 专题]]（DDIA 下半的沉淀版）
- [[存储面试问题清单 - 存储引擎]]（Q&A 收口的题库）
- [[S-Week 7 - 简历化与投递启动]]（bullet 写法与投递节奏）

## 参考

- DDIA 第 3 章下半（SSTable / LSM / B-tree 对比）
- RocksDB Wiki：Write Ahead Log、Write Path、MANIFEST、Benchmarking tools（db_bench）
- fio / RocksDB 的 GitHub issues（复现选题池）
- Brendan Gregg, *Systems Performance* ch 2（方法论：负载特征化与 USE）
