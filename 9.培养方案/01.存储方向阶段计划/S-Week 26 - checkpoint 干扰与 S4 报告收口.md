---
title: S-Week 26 - checkpoint 干扰与 S4 报告收口
date: 2026-07-12
tags:
  - infra
  - 存储
  - 阶段计划
status: active
---

# S-Week 26 - checkpoint 干扰与 S4 报告收口

> [!goal] 本周目标
> S4 的最后一块拼图与收口：可选的 checkpoint 干扰实验（四种写姿势 × 前台 p99，S-Week 9 方法的 AI 场景版），然后 `ai_data_path_report.md` 定稿——GDS、KV offload、checkpoint 三章合体，总纲报告面试题 4/8 全部能脱稿。本周结束，S4 从"两个实验"变成"一份能讲 GPU 数据路径全貌的报告"。
>
> 精简版决策点：若春/夏实习挤压时间，checkpoint 实验整体砍掉（口述级即可，素材在 [[checkpoint I O 专题 - burst write 隔离]]），本周只做报告收口——总纲允许的精简路径。

## 学习目标

1. **checkpoint 的负载形态是什么？** 每参数约 16 字节的训练态、周期性 burst 顺序写、恢复时全集群读风暴——训练侧"存储上关键路径"的那一半。
2. **四种写姿势各验证什么？** buffered（复现 writeback 风暴）、buffered+限速、O_DIRECT（从根上不堆脏页）、O_DIRECT+限速——四组合成一张缓解收益表。
3. **异步 checkpoint 省的是什么？** GPU 停顿从"写盘时间"缩到"D2H 快照时间"；崩溃语义与 everysec 同构（丢最近一份未完成的）。
4. **S4 报告的骨架怎么搭？** 三章（GDS / KV offload / checkpoint）+ 一条主线（"GPU 在等什么数据、数据卡在哪一层"）+ 三类实验边界声明（沿用 S3 的三分法纪律）。
5. **报告面试题 4/8 的答案在哪一章？** 显存瓶颈与 SSD-backed KV cache 成因（KV 章开头）、收支公式与临界点（对账表）、GDS 降 CPU 的机制与条件（GDS 章归因）。

## 1. checkpoint 干扰实验（Day 1-3，可选）

- 前台：io_uring 4K 随机读 QD4 持续记录 p99（S-Week 9 程序原样复用）。
- 注入：模拟 checkpoint 的数十 GB 顺序写，四种姿势各一轮（buffered / buffered + cgroup io.max / O_DIRECT / O_DIRECT + 限速），每轮记录前台 p99 时间线 + `Dirty` 水位 + biosnoop 肇事样本。
- 加一组"异步快照"演示：先写进内存 buffer（模拟 D2H 快照）再后台刷盘，观察前台受扰时段的移动。
- 产出 `docs/ckpt_interference.md`：四姿势对照表 + 缓解收益结论 + 原子落盘（临时文件 + fsync + rename + 目录 fsync）的实现要点。

## 2. ai_data_path_report.md 定稿（Day 4-5）

- 三章合体 + 开篇一页讲主线：从 "GPU 时薪 vs 存储毫秒" 的经济学切入，训练侧（checkpoint）与推理侧（KV cache）各一半论据。
- 边界声明一节：租用机型与驱动版本、单卡、7-8B 模型、compat mode 断言记录、checkpoint 实验的模拟性质（非真实训练框架）。
- `reproduce.sh` 收口：GDS 对照、KV 三档、checkpoint 注入全部一键化（GPU 依赖的实验标注"需 GDS 机型"）。
- 报告面试题 4/8 自测：对着 [[存储面试问题清单 - AI 数据路径]] 的 Q1-Q6 逐题脱稿，卡壳处回改报告——报告写到"能答面试题"才算定稿。

## 3. gap 快查与 S4 边界（Day 5）

- 对照总纲 S4 定义核对：GDS 对照 ✓ / KV offload benchmark ✓ / checkpoint（做了或声明砍）/ ai_data_path_report ✓。
- 没做的可选项（Mooncake 实测、GDS over NVMe-oF、真实训练框架 checkpoint）列成"入职后/面试聊"清单——主动圈边界。

## 4. 推理线合流（约 25%）

- 复面 [[Week 7 - KV Cache + Prefix Cache + Paged KV]] 与 [[Week 8 - Prefill Decode + Open Source Repro]] 要点——报告引用它们的地方逐处核对口径。

## 5. 面试保底（约 15%）

> 阶段 3 秋招冲刺编排第 4 讲。

- 算法：90 分钟限时模拟一场（组卷沿用 [[AI Infra 岗算法笔试保底清单]]）；CodeTop 扩展保温。
- 题库轮换（1 板块）：[[存储面试问题清单 - AI 数据路径]]——本周素材直接回填 Q1-Q9 的"S4 实测"证据位。
- 项目问答：S4 全部素材并入 `interview_qa.md`。

## 6. 本周产出文件

| 文件 | 内容 | 验收方式 |
|---|---|---|
| `docs/ckpt_interference.md`（可选） | 四姿势对照 + 缓解收益表 | 每个结论有 p99 时间线 |
| `ai_data_path_report.md`（定稿） | 三章 + 主线 + 边界 | 报告面试题 4/8 脱稿通过 |
| `reproduce.sh`（S4 版） | 全实验一键复现 | 标注 GPU 依赖项 |
| 题库 99.40 回填 | Q1-Q9 补实测证据 | 每题证据可指到报告页 |

## 7. 验收标准

- [ ] checkpoint 实验完成（或砍掉并声明，口述级素材就绪）。
- [ ] ai_data_path_report.md 定稿，主线一页能讲清"GPU 在等什么数据"。
- [ ] 报告面试题 4/8 全部脱稿通过。
- [ ] 三类实验边界声明齐全，可选项圈成"入职后"清单。
- [ ] [[存储面试问题清单 - AI 数据路径]] 的实测证据位回填完成。
- [ ] 合流与面试保底完成本周额度。

## 面试问题

- checkpoint 保存怎么不打死前台？四层缓解各自的代价？
- 你的干扰实验和真实训练框架的 checkpoint 差在哪？（模拟边界）
- 用一页纸讲"存储被推到 AI 关键路径"——训练侧和推理侧各举一个你的实测。
- GDS 为什么能降低 CPU 参与？需要什么条件？（报告题 8）
- S4 里你主动没做的是什么？为什么？

## 关联知识

- [[S-Week 25 - KV offload 三档对照与收支对账]]
- [[S-Week 27 - 作品集收口 storage-ai-infra-portfolio]]
- [[S-Week 26 - 前置知识 - checkpoint 干扰与 S4 报告收口]]
- [[checkpoint I O 专题 - burst write 隔离]]（实验骨架与四层缓解）
- [[S-Week 9 - eBPF 观测]]（前台 p99 方法与程序复用）
- [[存储面试问题清单 - AI 数据路径]]（证据回填对象）
- [[存储引擎专题 - WAL 与 crash consistency]]（原子落盘同源模式）
