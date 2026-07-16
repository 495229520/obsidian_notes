---
title: S-Week 3 - fio 对照与 Benchmark Matrix
date: 2026-07-08
tags:
  - infra
  - 存储
  - 阶段计划
status: active
---

# S-Week 3 - fio 对照与 Benchmark Matrix

> [!goal] 本周目标
> 把 fio 变成主力测量工具（对应推理版里 Nsight 的地位），跑出一张覆盖读写/块大小/队列深度的 benchmark matrix，并和前两周自写程序的数据互相验证。本周结束时，你要能解释"队列深度提高吞吐但恶化 p99"背后的排队论直觉。

## 学习目标

1. **fio job file 怎么写？** ioengine / rw / bs / iodepth / numjobs / direct / runtime 各控制什么。
2. **fio 输出怎么读？** IOPS、带宽、slat / clat / lat 的区别、clat percentiles 里的 p99。
3. **iodepth 与延迟的关系？** Little's law 直觉：并发数 ≈ 吞吐 x 延迟。
4. **iostat -x 怎么配合看？** r/s、rMB/s、r_await、aqu-sz、%util 分别对应什么。
5. **自写程序和 fio 的差异来自哪？** 计时方式、syscall 开销、io 引擎不同。

## 1. fio 入门与对照（Day 1-2）

### 1.1 第一个 job file

```ini
; randread_4k.fio
[global]
directory=/data/iolab
filename=fiotest
size=8g
direct=1
ioengine=io_uring
runtime=60
time_based=1
group_reporting=1

[randread-4k-qd1]
rw=randread
bs=4k
iodepth=1
```

跑通后重点读输出的三段：`clat percentiles`（延迟分布）、`IOPS/BW`、`lat (usec)` 汇总。搞清 slat（提交延迟）/ clat（完成延迟）/ lat（总延迟）。

### 1.2 与自写程序对账

用 fio `iodepth=1, ioengine=psync, rw=randread, bs=4k, direct=1` 对照上周 O_DIRECT 随机读程序：

- 两者 p50 / p99 应在同量级；差异超过 20% 就要解释（计时口径？offset 分布？文件碎片？）。
- 这一步的意义：证明你的自写工具可信，也证明你会用工业工具校准自己。

## 2. Benchmark Matrix（Day 3-5）

### 2.1 矩阵设计

| 维度 | 取值 |
|---|---|
| rw | randread / read / randwrite / write |
| bs | 4k / 64k / 1m |
| iodepth | 1 / 8 / 32 |
| direct | 1（固定） |

共 36 组，Agent 生成 job file 和结果解析脚本（fio `--output-format=json` 便于解析），每组 3 次重复。写操作全部指向测试文件，绝不碰裸设备。

### 2.2 同步观察 iostat

每组实验并行记录 `iostat -x 1`：

- `aqu-sz`（平均队列长度）是否和 iodepth 对得上；
- `r_await` 随 iodepth 怎么涨；
- `%util` 到 100% 时 IOPS 是否还在涨（blk-mq 多队列下 %util 饱和不等于设备饱和——记下这个现象，阶段 1 深挖）。

### 2.3 必须回答

- 4K 随机读从 QD1 到 QD32，IOPS 涨了几倍？p99 涨了几倍？拐点在哪？
- 顺序 1M 读的带宽是否接近盘的标称带宽？口算验证：IOPS x bs = 带宽。
- 随机写为什么可能出现先快后慢？（SSD 缓存 / GC 的第一次露面，稳态问题留到阶段 2 nvme-of-lab）

### 2.4 模板沉淀

把本周格式整理成 `benchmark-template.md` 放进 `storage-ai-infra-portfolio`：环境段、负载段、指标段、重复次数、原始数据路径——之后所有项目（包括推理线）统一用这份模板。

## 3. 理论配套

- 《Systems Performance》第 9 章 Disks（读 workload characterization 与 latency analysis 小节即可）。
- OSTEP 第 44 章 Flash-based SSDs（FTL、擦写块、写放大first pass）。

## 4. 推理保温（约 25%）

- [[Week 6 - Observability + Metrics]] 上半：vLLM + Prometheus / Grafana 起 dashboard，观察 TTFT / queue / KV cache usage。
- 对照点：Grafana 看 serving 的 p99，iostat 看盘的 await——两边都是"分位数 + 队列"思维，报告里可以互相引用。

## 5. 面试保底（约 15%）

- 算法（5-8 题）：二分查找。参考 [[5.2.6 二分查找与二分答案]]，做 [[34. 在排序数组中查找元素的第一个和最后一个位置]] 及同类题。
- 八股（1 章）：虚函数与多态。过 [[12.3.2 虚函数]]、[[6.3.1 深入理解多态]]。
- 项目问答：10 个 Q&A。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `jobs/*.fio` | 36 组 job file | 可直接复跑 |
| `scripts/run_matrix.sh` | 跑矩阵 + 收集 JSON | 一键执行 |
| `results/matrix/*.json` | fio 原始输出 | 不手动修改 |
| `docs/fio_matrix_report.md` | IOPS / 带宽 / p99 曲线 + 拐点分析 | 每条结论有图有数 |
| `benchmark-template.md` | 统一报告模板（进 portfolio 仓库） | 推理线也能用 |

## 7. 验收标准

- [ ] fio 输出三段（slat/clat/lat、IOPS/BW、percentiles）都能讲清。
- [ ] 自写程序与 fio 对账完成，差异有解释。
- [ ] 36 组矩阵跑完，QD-IOPS-p99 曲线画出，拐点标注。
- [ ] iostat 的 aqu-sz / await / %util 与 fio 参数对应关系验证过。
- [ ] `benchmark-template.md` 定稿。
- [ ] 推理保温和面试保底完成本周额度。

## 面试问题

- fio 的 iodepth 和 numjobs 有什么区别？
- slat / clat / lat 分别是什么？
- 为什么 iodepth 提高吞吐但恶化 p99？拐点由什么决定？
- iostat 的 %util 100% 一定是设备饱和吗？
- 你怎么验证一个 benchmark 工具本身是可信的？

## 关联知识

- [[S-Week 2 - O_DIRECT + 持久化语义]]
- [[S-Week 4 - mmap 与读路径对比]]
- [[S-Week 3 - 前置知识 - fio 对照与 Benchmark Matrix]]
- [[存储性能分析专题 - fio 与 benchmark matrix]]
- [[Week 5 - Serving Benchmark Harness]]（p99 / warmup / 可复现方法论同源）
- OSTEP Ch.44（SSD）
