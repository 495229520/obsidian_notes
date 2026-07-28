---
title: S-Week 8 - 前置知识 - 块层与 blktrace
date: 2026-07-11
tags:
  - 高性能存储
  - 存储方向参考资料
  - 计划
aliases:
  - 存储 Week 8 前置知识
  - 块层与 blktrace 前置知识
status: active
---

# S-Week 8 - 前置知识 - 块层与 blktrace

## 索引

- [[#0. 先建立直觉：延迟到底花在排队还是设备]]
- [[#1. bio、request 与 blk-mq]]
- [[#2. 合并与 plug：小请求怎么变大]]
- [[#3. blktrace 工具链：blktrace → blkparse → btt]]
- [[#4. 事件字母表精读]]
- [[#5. iostat 列的来源与 Little's law]]
- [[#6. 实验纪律：怎么采 trace 才不污染数据]]
- [[#7. 常见错误]]
- [[#8. 学习检查清单]]
- [[#9. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 8 - 块层与 blktrace]] 的总前置知识：动手前先通读 0-2 节建立块层图景，采 trace 前看 3-4 节和第 6 节，做 iostat 对账时看第 5 节。深挖版见 [[块层观测专题 - iostat blktrace eBPF]]。

---

> 阶段 0 你已经能测出"这次读花了 80 µs"；本周要能回答"这 80 µs 里多少在内核排队、多少在设备上执行"。工具就是 blktrace——块层的逐事件录像机。

---

## 0. 先建立直觉：延迟到底花在排队还是设备

去食堂打饭的延迟 = 排队时间 + 打饭时间。改善手段完全不同：排队长要加窗口或错峰，打饭慢要换菜品。I/O 同理：

```text
应用观测到的延迟（fio 的 clat、iostat 的 await）
  = 块层排队/调度时间（Q2D） + 设备执行时间（D2C）
```

阶段 0 的所有测量都只有总数；blktrace 把它拆开。拆开后 S-Week 5 的现象立刻有了解释：QD 加大后 IOPS 饱和、p99 还在涨——涨的全是 Q2D（排队），D2C（设备）根本没变。

## 1. bio、request 与 blk-mq

文件系统层把"读文件第 N 页"翻译成"读设备第 X 扇区"后：

| 结构 | 是什么 | 类比 |
|---|---|---|
| bio | 一段连续块 I/O 的描述（设备、起始扇区、长度、方向、完成回调） | 一张点菜单 |
| request | 排队与下发的单位，可含多个合并的 bio | 后厨的一张工单（几张菜单并单） |

**blk-mq（多队列块层）** 的两级队列：

- 软件队列：per-CPU，暂存、合并、（可选）调度。
- 硬件队列：映射到设备的硬件队列（NVMe 的多个 SQ），无锁下发。

NVMe 默认调度器是 `none`（`cat /sys/block/nvme0n1/queue/scheduler` 验证）：HDD 时代的电梯排序是为了省寻道，NVMe 无寻道且自带几十路并行队列，软件排序纯属开销。SATA SSD 常见 `mq-deadline`，机械盘用 `bfq`——面试常考这组对比。

## 2. 合并与 plug：小请求怎么变大

- **合并（merge）**：新 bio 与队列中已有 request 的扇区范围相邻，则并入（back/front merge）。顺序负载合并率高 → 平均请求大 → 每字节的固定开销摊薄。
- **plug/unplug**：进程提交 I/O 时先"塞住"（plug）本地攒一小批，提交完或调度出去时"拔塞"（unplug）批量下发——给合并创造时间窗。

这解释了 S-Week 1 的观察链：readahead 发出顺序大读 → 块层进一步合并 → `iostat` 的 rareq-sz 大、r/s 相对少。本周用 M 事件数把这条链的最后一环钉死。

## 3. blktrace 工具链：blktrace → blkparse → btt

三件套分工：

```bash
# ① 采集（内核 relayfs 导出原始事件；root）
sudo blktrace -d /dev/nvme0n1 -w 30          # 采 30 秒，落地 nvme0n1.blktrace.<cpu>
# ② 翻译成人类可读的逐事件流
blkparse -i nvme0n1 | less
# ③ 统计分解（要先用 blkparse -d 转成二进制）
blkparse -i nvme0n1 -d trace.bin > /dev/null
btt -i trace.bin | head -40
```

btt 输出里最重要的三行：

| 指标 | 含义 | 用法 |
|---|---|---|
| Q2D | 进块层到下发驱动 | 内核侧排队/调度成本 |
| D2C | 下发到完成 | 设备真实执行时间 |
| Q2C | 全程 ≈ iostat await 口径 | 与应用侧观测对账 |

## 4. 事件字母表精读

blkparse 每行格式：`设备 CPU 序号 时间戳 PID 事件 方向 扇区+长度 进程名`。事件字母：

| 字母 | 事件 | 备注 |
|---|---|---|
| Q | bio 进入块层 | 计数最接近"文件系统发了多少 I/O" |
| G | 分配 request | |
| P / U | plug / unplug | 攒批窗口的开关 |
| M / F | back / front merge | 合并证据，顺序负载下大量出现 |
| I | request 插入队列 | |
| D | 下发驱动 | Q→D 之间都算内核侧 |
| C | 完成 | D→C 是设备时间 |

读 trace 的最小任务：对一笔 4K 随机读（O_DIRECT QD1），指出它的 Q/G/I/D/C 五行，算出 Q2D 和 D2C；对一段顺序读，数出 M 事件并解释。

## 5. iostat 列的来源与 Little's law

iostat 的数据源是 `/proc/diskstats`（块层完成时累加的计数器），所以它天然是"合并后、含排队"的口径：

- r/s：每秒完成的（合并后）读请求数。
- rareq-sz：平均请求大小 = rkB/s ÷ r/s——判断合并/预读是否生效的最快证据。
- r_await：平均总延迟 ≈ Q2C。
- aqu-sz：平均在途请求数。
- %util：采样期内设备"至少一个请求在途"的时间占比。

三列自洽性可用 Little's law 对账：

$$
L = \lambda \times W
$$

L 是平均在途数（aqu-sz），λ 是到达率（稳态下等于完成率 r/s + w/s），W 是平均逗留时间（await，换算成秒）。例如 20000 IOPS、await 0.4 ms：

$$
L = 20000 \times 0.0004 = 8
$$

此时 aqu-sz 应在 8 附近。对不上先查：负载非稳态、读写混合口径、采样窗口错位。

> [!warning] %util 的语义边界
> util 只回答"忙不忙"，不回答"满不满"。单窗口食堂 util 100% 就是饱和；三十个窗口的食堂只要有一个窗口有人就算"忙"。NVMe 是后者。判断饱和看：加 QD 后 IOPS 还涨不涨、await 是否指数起飞。

## 6. 实验纪律：怎么采 trace 才不污染数据

- **trace 输出不落被测盘**：写到另一块盘/tmpfs，或 `-o -` 管道直连 blkparse。否则 trace 自己的写 I/O 混进数据。
- **短窗口**：blktrace 全量导出开销不小（高 IOPS 下可观），取证用 10-30 秒窗口，不常驻。
- **看丢事件计数**：blkparse 结尾的 dropped 统计不为零时，缩短窗口或减小负载再采。
- **与 iostat 同窗口**：对账实验必须同时段采集，错位几秒结论就飘。
- root 操作照旧走 CLAUDE.md 安全边界：只对实验盘操作，命令入脚本存档。

## 7. 常见错误

- **把 r/s 当成应用 IOPS**：中间隔着预读与合并，冷顺序读时两者差好几倍。
- **拿 biolatency 默认口径对 iostat await**：一个是 D2C 一个是 Q2C，先统一口径再对比（下周展开）。
- **trace 写回被测盘**：数据里多出一坨自己的写流量还不自知。
- **在合并已被拆散的场景找 M 事件**：O_DIRECT 4K 随机读本来就不该有合并，没有 M 是正常形态。
- **只采一次就下结论**：trace 窗口短，撞上 writeback 突发等背景活动会带偏画像，至少两次交叉确认。

## 8. 学习检查清单

- [ ] 能说出 bio 和 request 的区别与合并关系。
- [ ] 能解释 blk-mq 两级队列，以及 NVMe 为什么用 none 调度器。
- [ ] 能从 blkparse 输出指认 Q/G/M/I/D/C 并计算 Q2D、D2C。
- [ ] 能说出 iostat 每列的来源，以及 %util 在 NVMe 上的语义边界。
- [ ] 能用 Little's law 对账 aqu-sz。
- [ ] 知道 trace 采集的三条纪律（不落被测盘、短窗口、查丢事件）。

## 9. 关键要点总结

- 块层观测的核心是把延迟拆成 **Q2D（排队）与 D2C（设备）**，两者的处理路径完全不同。
- 合并 + plug 是"顺序快"的块层机制，M 事件与 rareq-sz 是证据。
- iostat 是块层完成口径的秒级汇总；blktrace 是逐事件录像；两者对账用 Little's law。
- %util 在并行设备上只说明"忙"，饱和判断看 IOPS 增量与 await 拐点。
- trace 纪律：不落被测盘、短窗口、查 dropped、与 iostat 同窗。

## 关联知识

- [[S-Week 8 - 块层与 blktrace]]（本篇服务的周计划）
- [[块层观测专题 - iostat blktrace eBPF]]（深挖版）
- [[S-Week 1 - Linux I O 路径专题 - VFS 到 NVMe 全路径]]（块层的上下文）
- [[S-Week 1 - 前置知识 - 环境搭建 + Page Cache 基线]]（iostat 入门在第 7 节）
- [[Week 6 - Observability + Metrics]]（观测方法论同源）

## 参考

- Systems Performance（Brendan Gregg）ch 9 Disks：util/saturation 语义、blktrace 用法
- `man blktrace`、`man blkparse`、`man btt`、`man iostat`
- 内核文档：Documentation/block/（blk-mq 设计）
- OSTEP 第 36-37 章（I/O 设备与调度的历史脉络）
