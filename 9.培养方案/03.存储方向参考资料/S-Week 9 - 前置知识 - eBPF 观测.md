---
title: S-Week 9 - 前置知识 - eBPF 观测
date: 2026-07-11
tags:
  - 高性能存储
  - 存储方向参考资料
  - 计划
aliases:
  - 存储 Week 9 前置知识
  - eBPF 观测前置知识
status: active
---

# S-Week 9 - 前置知识 - eBPF 观测

## 索引

- [[#0. 先建立直觉：为什么需要"常驻不伤身"的观测]]
- [[#1. eBPF 三十秒原理]]
- [[#2. 本周工具箱：四个 bcc 工具]]
- [[#3. 观测层次决策树]]
- [[#4. 毛刺从哪来：五个常见来源]]
- [[#5. 毛刺注入实验怎么设计]]
- [[#6. 缓解手段速览]]
- [[#7. 常见错误]]
- [[#8. 学习检查清单]]
- [[#9. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 9 - eBPF 观测]] 的总前置知识：装工具前读 0-2 节，设计毛刺实验前读 3-5 节，写 runbook 时对照第 6 节。深挖版见 [[块层观测专题 - iostat blktrace eBPF]]。

---

> 上周的 blktrace 是"出事后调监控录像"；本周的 eBPF 工具是"常年挂在墙上的体温计"——开销低到可以一直开着，毛刺出现的瞬间就有数据。生产环境的性能定位基本都是这个形态。

---

## 0. 先建立直觉：为什么需要"常驻不伤身"的观测

p99 毛刺的特点是**稍纵即逝**：等你发现再去开 blktrace，现场早没了。所以需要一类工具：

- 开销低到可以常驻（不显著影响被观测系统）；
- 输出直接是聚合结果（直方图、超阈值事件），不用事后处理海量原始数据。

eBPF 正好是这个定位。blktrace 并不被取代——它仍是"抓到现行后取证"的工具，两者配合。

## 1. eBPF 三十秒原理

- 一段受限的字节码程序被加载进内核，挂在事件点上（kprobe 动态探针 / tracepoint 静态点）。
- verifier 静态验证：不许死循环、不许越界访问——所以敢在生产内核里跑。
- 数据聚合（计数、直方图桶）在**内核态的 map** 里完成，用户态定期读汇总——这是开销低的关键：blktrace 每个事件都要导出，eBPF 只导出统计结果。

本周不写 eBPF 程序，只用 bcc 工具集的现成命令；写程序留给以后有需要时（bpftrace 单行脚本是下一级台阶）。

## 2. 本周工具箱：四个 bcc 工具

Ubuntu 安装后命令带 `-bpfcc` 后缀（其他发行版可能不带）：

```bash
sudo apt install -y bpfcc-tools linux-headers-$(uname -r)
```

| 工具 | 层 | 输出 | 典型问题 |
|---|---|---|---|
| biolatency | 块层 | 延迟直方图（2 的幂次桶） | 延迟分布形态变没变？ |
| biosnoop | 块层 | 逐笔：时刻/进程/盘/扇区/字节/延迟 | 毛刺时刻是谁在发 I/O？ |
| fileslower | VFS 层 | 超阈值的慢读/慢写 | 应用感知的慢发生在哪层？ |
| ext4slower | ext4 层 | 慢 read/write/open/fsync | 文件系统操作哪类慢？ |

两个口径细节，对账必须知道：

- biolatency 默认统计**下发到完成**（≈ D2C），加 `-Q` 才包含内核排队（≈ Q2C，与 iostat await 同口径）。
- 直方图单位默认微秒（usecs），`-m` 换毫秒——读图先看单位行。

## 3. 观测层次决策树

```text
应用逐笔延迟出现毛刺
 ├─ fileslower 有记录、biolatency 正常
 │    → 问题在 VFS/文件系统/锁/writeback 等待，设备无辜
 ├─ biolatency 同步恶化
 │    ├─ 整体右移        → 全局变慢（带宽被抢 / 设备降速）
 │    └─ 长出第二个峰    → 混入另一类 I/O（大请求流 / 突发写）
 │         → biosnoop 按时间戳抓肇事进程
 └─ 需要单笔证据链 → blktrace 短窗口 + btt 分解 Q2D/D2C
```

先定层、再看形态、再抓人、最后取证——顺序错了会浪费大量时间在错误的层里翻找。

## 4. 毛刺从哪来：五个常见来源

| 来源 | 机制 | 特征信号 |
|---|---|---|
| writeback 突发 | Dirty 堆积到阈值后集中刷盘 | 毛刺周期性；Dirty 值锯齿；biosnoop 见 kworker 大写 |
| 带宽挤占 | 别的进程大块顺序 I/O | biolatency 双峰；biosnoop 见大请求流 |
| readahead 误伤 | 随机负载触发无效预读 | rkB/s 远大于程序消费量 |
| cgroup 限速 | io.max 节流排队 | Q2D 大、D2C 正常 |
| SSD 内部 GC | 写多了触发垃圾回收 | 无外部肇事者；D2C 直接变大；稳态盘明显 |

第五种最阴险：主机侧完全找不到肇事者，D2C 却涨了——这是"排队 vs 设备"分叉的价值所在。

## 5. 毛刺注入实验怎么设计

原则与阶段 0 一致：**一次只注入一个变量，前后有干净基线**。

```text
1. 基线：前台 io_uring 4K 随机读 QD4，跑 5 分钟，记录 p99。
2. 注入：只开一种干扰（writeback 突发 或 fio 大块顺序读）。
3. 观测：biolatency 直方图前后对比 + biosnoop 对齐毛刺时间戳。
4. 停止干扰：确认 p99 回落到基线（可逆性证明因果）。
5. 重复一轮：排除偶发。
```

干扰源示例：

```bash
# writeback 突发：大量 buffered 写 + 周期 sync
fio --name=dirty --rw=write --bs=1M --size=4G --ioengine=psync &
watch -n 10 sync
# 带宽挤占：大块顺序读
fio --name=hog --rw=read --bs=1M --iodepth=8 --direct=1 --ioengine=io_uring --size=8G
```

"可逆性"这步别省：干扰停、毛刺消，因果链才闭环——只有相关性的报告面试官一追就塌。

## 6. 缓解手段速览

| 手段 | 作用点 | 代价 |
|---|---|---|
| 调低 vm.dirty_ratio / dirty_background_ratio | 让 writeback 更早、更平缓地刷 | 写吞吐可能下降；fsync 语义不变 |
| ionice（CFQ/BFQ 才完整生效） | 降低干扰进程调度优先级 | NVMe + none 调度器下基本无效——本身就是考点 |
| cgroup v2 io.max | 硬限速干扰组 | 需要 cgroup 配置权限；限得太狠饿死后台任务 |

实验只要求三选一实施并量化前后 p99；三种全试更好。注意 ionice 在 NVMe/none 下的无效性是刻意保留的"陷阱选项"——发现它无效并解释原因，比直接用对工具更有面试价值。

## 7. 常见错误

- **内核 headers 缺失**：bcc 编译探针失败，`linux-headers-$(uname -r)` 必须与运行内核严格同版本。
- **读错直方图单位**：usecs 当成 msecs，结论差一千倍。
- **biosnoop 高 IOPS 下开销与丢事件**：逐笔输出在几十万 IOPS 时自身成为干扰，只在定位窗口开，不常驻。
- **fileslower 阈值设太低**：默认 10ms，设成 0.1ms 会刷屏且自身开销飙升。
- **没做可逆性验证**：干扰和毛刺只有时间相关性，没有停止-恢复证据。
- **拿 biolatency 默认口径对 iostat await**：差一个排队段，加 -Q 再对。
- **观测系统时忘了观测者**：biosnoop/blktrace 自己的输出别写进被测盘（上周纪律继续有效）。

## 8. 学习检查清单

- [ ] 能说出 eBPF 开销低于 blktrace 的原因（内核态聚合 vs 全量导出）。
- [ ] 四个工具各在哪层、各回答什么问题，能背出决策树。
- [ ] 知道 biolatency 的 -Q 与单位选项对口径的影响。
- [ ] 能列出五种毛刺来源及各自的特征信号。
- [ ] 能复述毛刺注入实验的五步（含可逆性验证）。
- [ ] 知道 ionice 在 NVMe/none 下为什么无效。

## 9. 关键要点总结

- eBPF 工具 = 常驻低开销观测；blktrace = 短窗口取证。定位流程：定层 → 看形态 → 抓人 → 取证。
- 直方图形态是信息量最大的一眼：右移是全局变慢，双峰是混入异类 I/O。
- 毛刺定位的黄金证据链：基线 → 注入 → 观测 → 停止 → 恢复，可逆性闭环因果。
- D2C 变大而找不到外部肇事者时，怀疑设备内部（GC）。
- runbook 的价值在"别人能照着走一遍"，不在结论本身。

## 关联知识

- [[S-Week 9 - eBPF 观测]]（本篇服务的周计划）
- [[S-Week 8 - 前置知识 - 块层与 blktrace]]（Q2D/D2C 分解，上周基础）
- [[块层观测专题 - iostat blktrace eBPF]]（深挖版与 runbook 模板）
- [[Week 6 - Observability + Metrics]]（推理侧同款方法论）
- [[S-Week 2 - O_DIRECT + 持久化语义]]（dirty page 与 writeback 机制）

## 参考

- bcc 工具文档与各工具 `_example.txt`：[GitHub](https://github.com/iovisor/bcc)
- Systems Performance（Brendan Gregg）ch 9 Disks、ch 15 BPF
- BPF Performance Tools（Brendan Gregg）ch 9 Disk I/O（可选深读）
- 内核文档：Documentation/admin-guide/sysctl/vm.rst（dirty_ratio 族）
