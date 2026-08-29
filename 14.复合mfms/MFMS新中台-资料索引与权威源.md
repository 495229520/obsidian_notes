---
title: MFMS 新中台 · 资料索引与权威源
date: 2026-08-18
updated: 2026-08-23
tags:
  - 复合mfms
  - MFMS
  - 架构规划
  - 文档治理
version: v0.4
status: 当前唯一入口
---

# MFMS 新中台 · 资料索引与权威源

> [!important] 从这里开始
> 本页是 MFMS 新中台在 Obsidian 中的**唯一资料入口**。活跃架构文档统一位于 <code>14.复合mfms/</code>；Trellis Task 镜像由项目自动生成；旧目录和 legacy 仓库只保存历史证据。

> [!success] 当前基线：v0.4
> 2026-08-22 业务边界更新：**数据中台负责需求，调度系统把需求拆成订单和运单**。中台写需求、只读订单/运单；订单最小逻辑字段为工作站点名称/别名、上下料 <code>type</code>、<code>ids[]</code>、时间戳、数量和优先级。v0.3 的下位机唯一控制权与 Stream 实时事实边界继续有效。

## 1. 活跃文档

| 文档 | 负责回答 | 状态 |
| --- | --- | --- |
| [[MFMS新中台-架构构造思路]] | 当前系统边界、组件、数据与最高原则 | **v0.4 当前架构入口** |
| [[MFMS新中台-逐层设计工作台]] | 控制、Stream、需求/订单/运单、恢复、审计与实施切片 | **v0.4 唯一详细工作台** |
| [[MFMS新中台-待确定问题清单]] | P0 接口与业务问题 | **唯一未决项登记簿** |
| [[MFMS新中台-Trellis工程结构方案]] | v0.4 如何落入 ARCHMAP/spec/task | **当前项目落位方案** |
| [[MFMS新中台-系统架构图]] | 系统全景和数据链路直览 | **v0.4 架构图展示** |
| [[MFMS新中台-架构图.drawio]] | 系统上下文、控制权、数据权威 | **v0.4 可编辑图源** |
| [[MFMS新中台-P0契约评审控制台]] | Trellis 子任务、Orca 波次、人工评审与写回流程 | **P0 评审协调入口** |
| [[MFMS新中台-人工确认记录]] | 外部合同签字、版本、证据与结论 | **唯一人工确认台** |
| [[14.复合mfms/14.1 Task/Trellis Task 总览\|Trellis Task 总览]] | 项目任务状态展示 | 自动生成、只读 |

一个主题只能有一份活跃正文。若上表与旧材料冲突，以上表中的 v0.4 文档为准。

## 2. v0.4 一页摘要

| 主题 | 当前权威结论 |
| --- | --- |
| 控制权 | 下位机唯一管理/裁决；中台和 Adapter 都只是申请者/持有者 |
| 中台控制能力 | Query / Acquire / ForceAcquire / Release + 持权后的调试命令薄包装 |
| 二次确认 | 开发者知情、权限、reason 与审计，不是锁协议 |
| 实时状态 | 下位机与 Adapter 经 Redis Stream 发布；中台冻结信封，不管理 Redis 拓扑 |
| 状态可信度 | UNKNOWN / FRESH / STALE / UNSUPPORTED；缓存不能证明控制权 |
| 需求 | 中台接收、校验并单写原始需求，不拆单 |
| 订单 | 调度由需求拆分并单写；字段含站点别名、上下料 type、ids[]、时间戳、数量、优先级 |
| 运单 | 调度由需求/订单拆分并单写；中台只读，Adapter 执行并发布实时事实 |
| 数据血缘 | 推荐 <code>requirement_uid → order_uid → waybill_uid</code>；基数、DDL 和生命周期待评审 |
| Adapter | 内部 ADK 编排、控制策略和恢复移出中台范围 |
| 调度连接 | 基线不需要 gRPC 直连；需求/订单/运单经 MySQL，实时消息经 Stream |
| 部署 | 局域网小主机、C++ 单进程模块化单体 |

## 3. 权威边界

| 层级 | 权威范围 | 位置与规则 |
| --- | --- | --- |
| **Vault 当前层** | 人类确认的架构、详细设计、未决项和图源 | 本目录五个 canonical 对象 |
| **Trellis 实施层** | 允许实现的红线、模块地图、合同、质量和任务 | <code>/Users/melene/project/mfms_Framework/.trellis/</code> |
| **对端权威层** | 下位机真实控制接口、Adapter 消息、调度写入行为 | 对端代码/接口包/联调样例；中台文档不得虚构 |
| **Legacy 证据层** | 旧系统实际行为和历史拍板 | legacy repo、旧 vault 文档；只读追溯 |
| **任务展示层** | Trellis PRD/design/implement 的 Obsidian 镜像 | <code>14.1 Task/</code>；带 generated_by 的文件禁止手改 |

### 3.1 冲突处理

1. 已确认事实优先于推荐方案；
2. v0.4 优先于 v0.3、v0.2 和 2026-07 历史拍板；
3. 下位机真实接口优先于中台建议函数名/结果码；
4. 调度/DBA 联合评审优先于推荐 DDL；
5. 未确认时保留来源与状态，不擅自合并成一个“真值”；
6. 任何被重新打开的结论先回到问题清单，再修改 Trellis。

## 4. 当前输入集

| 材料 | 位置 | 用途 |
| --- | --- | --- |
| v0.4 业务变更 | 2026-08-22 用户确认“中台负责需求、调度拆订单/运单”及订单最小字段；持久化在 architecture task 的 <code>research/v0.4-requirement-boundary.md</code> | 业务持久化最高优先输入 |
| v0.3 保留规划 | 2026-08-18《MFMS 数据中台架构方案 v0.3：下位机统一管理控制权》 | 控制权与 Stream 保留基线 |
| 架构入口 | [[MFMS新中台-架构构造思路]] | 结论与边界 |
| 详细设计 | [[MFMS新中台-逐层设计工作台]] | 字段、流程、恢复与切片 |
| 问题登记 | [[MFMS新中台-待确定问题清单]] | 联合评审队列 |
| Trellis 方案 | [[MFMS新中台-Trellis工程结构方案]] | repo 落位 |
| 图源 | [[MFMS新中台-架构图.drawio]] | 可编辑视觉模型 |
| 新中台 repo | <code>/Users/melene/project/mfms_Framework</code> | 唯一新实现仓库 |
| Legacy repo | <code>/Users/melene/project/mfms</code> | 冻结事实源，不复制实现 |
| SDK 权威包 | <code>/Users/melene/Downloads/HyRMS_export_0810</code> | 当前 Cpp-Proxy-SDK/ADK 接口证据 |

## 5. SDK 版本基线

- 权威目录：<code>/Users/melene/Downloads/HyRMS_export_0810</code>；
- <code>libhyrms_export.so</code> 大小：5,891,528 bytes；
- SHA-256：<code>a1b9906894118596a54bc6529e245f5299584194cadac1703afa40aa753d310c</code>；
- legacy 仓库历史命名目录中的 vendored .so 已验证为同一哈希；
- <code>/Users/melene/project/new_interface</code> 是已被取代的候选，不作为当前输入。

审计记录：<code>/Users/melene/project/mfms/.trellis/tasks/archive/2026-08/08-10-update-lower-machine-interface-20260718/</code>。

## 6. 历史材料

以下材料保留追溯，不再承担当前入口：

- <code>10.研一上学期/复合机器人汇总/MFMS数据中台新版架构设计-分层功能与拍板记录.md</code>；
- <code>10.研一上学期/循光复合机器人重构/MFMS数据中台新版架构设计-分层功能与拍板记录.md</code>；
- <code>10.研一上学期/复合机器人汇总/MFMS敏捷开发工作流-trellis规划与需求流程.md</code>；
- <code>10.研一上学期/复合机器人汇总/MFMS数据中台重构规划-逐层设计工作台.md</code> 跳转页；
- 旧 PNG/旧 drawio 和 legacy 架构技术文档；
- v0.3 的中台 <code>order_request</code> + 调度 <code>execution/binding</code> 持久化模型；
- v0.2 的 Redis KV 业务状态、调度 gRPC 与中台协调夺权方案。

历史笔记中的“已定”只代表当时上下文，不得覆盖 v0.4。

## 7. 文档落位规则

1. 新中台架构、需求、会议结论、接口规划和图源统一落到本目录。
2. Trellis 任务源写在 repo 的 <code>.trellis/tasks/</code>；<code>14.1 Task/</code> 只存导出镜像。
3. 新结论先标【已确认/推荐/待确认】，再决定能否进入 spec。
4. 外部二进制和源码不复制进 vault，只登记路径、版本和校验信息。
5. 新活跃文档必须加入本索引；未登记材料不得被当作权威源。
6. 改动 canonical 文档后同步 ARCHMAP/spec/task，并运行链接、XML 与 exporter drift 检查。
