---
title: S-Week 8 - 块层与 blktrace
date: 2026-07-11
tags:
  - infra
  - 存储
  - 阶段计划
status: active
---

# S-Week 8 - 块层与 blktrace

> [!goal] 本周目标
> 打开块层这个黑盒：用 blktrace / blkparse / btt 追踪一笔 I/O 从进入块层到设备完成的每个事件，把延迟拆成"内核排队"和"设备执行"两段，并搞清 iostat 每一列到底是从块层哪里算出来的。阶段 1 的第一块拼图——阶段 0 只会"测出延迟"，本周开始能"解释延迟花在哪一层"。

## 学习目标

1. **bio 和 request 是什么关系？** bio 描述一段连续的块 I/O（设备、起始扇区、长度）；相邻 bio 可合并进同一个 request，request 才是排队和下发的单位。
2. **blk-mq 多队列模型长什么样？** per-CPU 软件队列 + 映射到 NVMe 硬件队列的 dispatch 队列；NVMe 默认调度器是 `none`，因为设备自己有足够的并行队列，电梯排序反而多余。
3. **blktrace 的事件字母表怎么读？** Q（进入块层）→ G（拿到 request）→ P/U（plug/unplug）→ M（合并）→ I（插入队列）→ D（下发驱动）→ C（完成）。
4. **Q2D 和 D2C 分别是什么？** Q2D 是内核侧排队/调度时间，D2C 是设备执行时间。毛刺定位的第一次分叉就在这里。
5. **iostat 的列从哪来？** await ≈ 平均 Q2C；aqu-sz ≈ 平均在途请求数；%util 只表示"有请求在途的时间占比"——多队列 SSD 上 100% 不等于饱和。

## 1. blktrace 初体验（Day 1-2）

安装并确认权限（blktrace 需要 root，且实验窗口尽量短）：

```bash
sudo apt install -y blktrace
# 管道直出，避免 trace 文件写回被测盘污染实验
sudo blktrace -d /dev/nvme0n1 -o - | blkparse -i - | head -50
```

两组对照负载（复用 S-Week 1 / S-Week 2 的程序）：

- 冷 buffered 顺序读：观察 readahead 触发的大请求与 M（合并）事件。
- O_DIRECT 4K 随机读（QD1）：观察一读一命、无合并、事件序列干净的形态。

要求：对每组负载能从 blkparse 输出里指认出 Q → G → I → D → C 的一条完整链，并解释每个字母。

再用 btt 做统计分解：

```bash
sudo blktrace -d /dev/nvme0n1 -w 30    # 采 30 秒
blkparse -i nvme0n1 -d trace.bin
btt -i trace.bin | head -40            # 看 Q2D / D2C / Q2C 分布
```

## 2. 三种负载的块层画像（Day 3-4）

对三种负载各采一次 trace，填一张对照表：

| 负载 | 平均请求大小 | M 事件多少 | Q2D vs D2C 占比 | 预期解释 |
|---|---|---|---|---|
| buffered 顺序读 | 大（预读+合并） | 多 | D2C 主导 | readahead 批量化 |
| O_DIRECT 4K 随机读 QD1 | 4K | 几乎无 | D2C 主导，Q2D 极小 | 无排队、设备时间即全部 |
| io_uring 4K 随机读 QD32 | 4K | 少 | Q2D 开始出现 | 队列深了，内核侧排队显形 |

必须回答：

- 同样是"顺序读快"，S-Week 1 用 readahead 解释，本周的 trace 证据是什么？（平均请求大小、M 事件、rareq-sz 三处互证）
- QD32 时 p99 变差，多出来的时间在 Q2D 还是 D2C？（对照 S-Week 5 的 QD 扫描结论）

## 3. iostat 对账（Day 5）

用 Little's law 把 iostat 的三列对账一遍：

$$
L = \lambda \times W
$$

其中 L 对应 aqu-sz（平均在途请求数），λ 对应 r/s（每秒完成请求数），W 对应 r_await（平均延迟，注意毫秒换算）。跑一组稳定负载，验证：

$$
aqu \approx \frac{rps \times await_{ms}}{1000}
$$

误差超过 20% 时找原因（负载不稳、采样窗口错位）。把"%util 100% 在 NVMe 上为什么不代表饱和"用自己的数据讲一遍：util 只统计"至少一个请求在途"，而 NVMe 可以并行几十个请求——饱和要看 IOPS 是否还能涨、await 是否起飞。

产出 `docs/blk_lifecycle.md`：一笔 I/O 的块层生命周期图 + 三种负载画像表 + iostat 列来源对照。

## 4. 推理保温（约 25%）

- [[Week 8 - Prefill Decode + Open Source Repro]] 收尾：复现实验定稿、issue/报告提交。本周之后推理线转入维护态（harness 复跑 + KV cache 复习）。

## 5. 面试保底（约 15%）

> 阶段 1 网络补强第 1 讲：阶段 0 只过了粘包和 TIME_WAIT，从本周起四周把网络八股补成体系，赶在秋招正式批笔试面试前收口。

- 算法（5-8 题）：DP 进阶。参考 [[5.2.20 背包问题]]、[[5.2.21 子序列动态规划]]，配 [[CodeTop 高频题 Top300]] DP 高频题。
- 八股（1 章）：TCP 状态机全景。过 [[7.5 TCP协议基础]]、[[8.1 TCP原理]]（三次握手/四次挥手为什么是三次和四次）、[[8.11 TCP连接关闭与异常状态]]，串上 [[10.2 TIME_WAIT状态]]。验收：能手画 TCP 状态转移图并解释每条边。
- 项目问答：10 个 Q&A（本周素材：块层生命周期、Q2D/D2C、%util 误读）。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `results/blktrace/*.txt` | blkparse / btt 原始输出 | 不手动修改，标注负载与时间窗 |
| `scripts/blk_profile.sh` | 一键采集：负载 + blktrace + iostat 同窗口 | 可复跑 |
| `docs/blk_lifecycle.md` | 生命周期图 + 三负载画像 + iostat 对账 | 每个结论有 trace 证据 |

## 7. 验收标准

- [ ] 能脱稿讲一笔 I/O 的块层生命周期（Q/G/M/I/D/C 每个事件干什么）。
- [ ] 三种负载的画像表完成，平均请求大小与合并行为的差异有解释。
- [ ] btt 的 Q2D / D2C 分解跑通，QD1 与 QD32 的对比结论明确。
- [ ] Little's law 对账完成，误差有解释。
- [ ] 能用自己的数据讲清 %util 的语义边界。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- 一次 write 系统调用最终变成几个 bio？什么情况下会被合并？
- blktrace 里 Q2D 大、D2C 小说明什么？反过来呢？
- iostat 的 await 包含哪些成分？和 fio 的 clat 是什么关系？
- %util 100% 在 NVMe 上为什么不等于饱和？那怎么判断饱和？
- NVMe 为什么默认不用 mq-deadline 而用 none？

## 关联知识

- [[S-Week 7 - 简历化与投递启动]]
- [[S-Week 9 - eBPF 观测]]
- [[S-Week 8 - 前置知识 - 块层与 blktrace]]
- [[块层观测专题 - iostat blktrace eBPF]]
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]
- Systems Performance ch 9（Disks）
