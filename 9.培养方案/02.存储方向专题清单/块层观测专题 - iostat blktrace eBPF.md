---
title: 块层观测专题 - iostat blktrace eBPF
date: 2026-07-11
tags:
  - 高性能存储/存储方向专题清单
roadmap_week: 阶段 1（S-Week 8、S-Week 9）
sort_order: "05.10"
status: active
---

# 块层观测专题 - iostat blktrace eBPF

> [!info] 所属路线
> - 培养方案阶段：阶段 1（S-Week 8 blktrace 生命周期；S-Week 9 eBPF 毛刺定位）
> - 排序：05.10
> - 用途：把 [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]] 的块层（第 ⑤ 层）单独讲透：一笔 I/O 在块层经历什么事件、三代观测工具各看到什么、p99 毛刺怎么一层层定位。

> [!goal] 目标
> 讲清三件事：块层生命周期（bio → request → 下发 → 完成）、iostat 每列的来源与误读边界、blktrace 与 eBPF 工具的选型决策树。支撑 S-Week 8/9 的实验设计和"线上 p99 涨了怎么查"这道必答题。

---

## 1. 块层在做什么

文件系统把"读文件第 N 页"翻译成"读设备第 X 扇区"后，请求进入块层：

- **bio**：一段连续块 I/O 的描述（设备、起始扇区、长度、方向）。
- **request**：排队与下发的单位。相邻 bio 会被**合并**（merge）进同一个 request——这是"顺序负载平均请求大"的直接机制。
- **blk-mq**：per-CPU 软件队列暂存与合并，再映射到硬件 dispatch 队列（NVMe 的多个 SQ）。NVMe 默认调度器 `none`：设备自身队列并行度足够，软件电梯排序收益为负。

## 2. blktrace 事件字母表与延迟分解

blkparse 输出里每行一个事件，常见字母：

| 事件 | 含义 |
|---|---|
| Q | bio 进入块层（queued） |
| G | 分配到 request（get request） |
| P / U | plug / unplug（攒一小批再下发） |
| M | 合并进已有 request（back merge；F 为 front merge） |
| I | request 插入调度/软件队列 |
| D | 下发给驱动（issued） |
| C | 设备完成（completed） |

btt 把逐笔事件汇总成分段延迟，最关键的两段：

- **Q2D**：内核侧（排队、合并、调度、等待队列空位）。
- **D2C**：设备侧（NVMe 执行 + 中断返回）。

定位第一分叉：**Q2D 大 → 排队问题（QD 过深、限流、调度）；D2C 大 → 设备问题（介质慢、GC、带宽被抢）**。

## 3. iostat -x：每列从哪来、哪里会误读

| 列 | 来源 | 误读边界 |
|---|---|---|
| r/s, w/s | 完成的合并后请求数 | 不是应用发的 syscall 数（合并、预读都在中间） |
| rareq-sz | 平均请求大小 | 判断预读/合并是否生效的最快证据 |
| r_await | 完成请求的平均总延迟 ≈ Q2C | 含排队；和 biolatency 默认口径（D2C）不同 |
| aqu-sz | 平均在途请求数 | 由 Little's law 可对账 |
| %util | 有请求在途的时间占比 | **NVMe 上 100% ≠ 饱和**：并行设备"忙"和"满"是两回事 |

Little's law 对账（S-Week 8 实验）：

$$
L = \lambda \times W
$$

L 对应 aqu-sz，λ 对应每秒完成请求数，W 对应平均延迟（换算秒）。对不上时先查负载是否稳态、采样窗口是否对齐。

判断 NVMe 是否真饱和的三件套：IOPS 是否还随 QD 上涨、await 是否起飞、设备规格书标称值对照。

## 4. eBPF 工具箱与三代工具选型

| 工具 | 层 | 形态 | 适用 |
|---|---|---|---|
| iostat | 块层汇总 | 秒级均值 | 第一眼：有没有异常、哪块盘 |
| blktrace / btt | 块层逐事件 | 短窗口全量 | 取证：单笔 I/O 的事件链与 Q2D/D2C |
| biolatency | 块层 | 常驻直方图 | 分布形态：右移 vs 双峰 |
| biosnoop | 块层逐笔 | 常驻流式 | 抓肇事者：谁在毛刺时刻发大 I/O |
| fileslower / ext4slower | VFS / 文件系统层 | 阈值触发 | 区分"上面慢"还是"下面慢" |

选型口诀：**iostat 看有没有，biolatency 看形态，biosnoop 看是谁，blktrace 看细节，fileslower 定层**。

eBPF 开销低的原因：直方图聚合在内核态完成，只上送汇总；blktrace 每个事件都要经 relayfs 导出到用户态。代价是 eBPF 直方图丢了单笔事件链——所以取证仍要 blktrace 短窗口补刀。

## 5. p99 毛刺定位 runbook（S-Week 9 模板）

```text
1. 确认现象：应用侧逐笔延迟 CSV，毛刺是脉冲还是持续、幅度多少。
2. 定层：fileslower 有记录而 biolatency 正常 → 文件系统/锁/writeback；
   biolatency 同步恶化 → 块层以下。
3. 找形态：biolatency 整体右移（全局变慢）vs 长出第二个峰（两类来源）。
4. 抓肇事者：biosnoop 对齐毛刺时间戳，看同盘还有谁、请求多大。
5. 取证：blktrace 短窗口，btt 分解毛刺样本 Q2D/D2C。
6. 缓解并量化：dirty_ratio / ionice / cgroup io.max 三选一，前后对比 p99。
```

常见毛刺来源速查：writeback 突发（Dirty 堆积后集中刷）、readahead 误伤（随机负载被预读挤占）、带宽挤占（大块顺序流）、cgroup 限速、SSD 内部 GC（稳态 vs 空盘）。

## 6. 面试口述模板

```text
线上存储 p99 涨了，我先用应用侧延迟确认现象量级，再用分层工具定位：
fileslower 和 biolatency 一上一下先分清是文件系统层还是块层的问题；
biolatency 直方图看形态，整体右移是全局变慢，双峰说明混进了另一类
I/O；biosnoop 按时间戳抓出毛刺时刻同盘的进程和请求大小；需要细节
时 blktrace 短窗口取证，btt 把延迟拆成 Q2D 和 D2C——排队变长和设
备变慢的处理路径完全不同。我在自己的实验里用后台 writeback 突发复
现过这个流程，缓解手段（调 dirty 阈值、cgroup 限速）也量化过效果。
```

追问预案：

- "%util 100% 说明什么？" → 只说明采样期内一直有请求在途；NVMe 并行队列下判断饱和要看 IOPS 增量和 await 拐点。
- "biolatency 和 iostat await 对不上？" → 口径不同：biolatency 默认 D2C，加 -Q 才含排队；await 是 Q2C。
- "为什么不常开 blktrace？" → 全量事件导出开销大、trace 数据量大，且写回被测盘会污染实验；常驻观测用 eBPF 直方图。

## 关联知识

- [[S-Week 8 - 块层与 blktrace]]、[[S-Week 9 - eBPF 观测]]（本专题服务的周计划）
- [[S-Week 8 - 前置知识 - 块层与 blktrace]]、[[S-Week 9 - 前置知识 - eBPF 观测]]（入门版）
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]（块层在全路径中的位置）
- [[存储性能分析专题 - fio 与 benchmark matrix]]（负载生成侧）
- [[00.存储方向专题清单索引]]
