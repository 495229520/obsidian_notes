---
title: MFMS 敏捷开发工作流：.trellis 规划与需求流程
date: 2026-07-22
tags:
  - 研一上学期/复合机器人汇总
status: 方案已定（四决策确认 A）；最小闭环待搭建
---

# MFMS 敏捷开发工作流：.trellis 规划与需求流程

> [!abstract] 本文定位
> **AI 协作敏捷开发方案**（系统"怎么建"）：项目庞大、重构进行中、需求会持续变化，用一组 markdown 文档 + python 脚本 + AI agent 管理开发，让 AI 以**极低 token** 理解项目架构、配置文件、上下文与业务需求。架构本体（分层/功能清单/红线/两轮拍板/drawio 解读）见姊妹篇 [[MFMS数据中台新版架构设计-分层功能与拍板记录]]。

---

## 1. 目标与设计原则

核心诉求：AI **不通读源码、不乱做事**就能持续交付。五条原则：

1. **架构文档先行（harness 式约束）**——一份 `ARCHMAP.md` 让 AI 秒懂分层与配置文件全貌；红线写死"不许层层加码"（[[MFMS数据中台新版架构设计-分层功能与拍板记录#3.5 触碰预算（反加码的硬指标）|触碰预算：新命令 ≤3 文件]]）。
2. **每层一张卡片**——应用层/业务层/门面/能力层各有自己的文档与脚本入口；卡片固定五小节：职责边界 / 关键锚点表（只写符号）/ 修改守则 / 禁区 / 验证命令。
3. **脚本定位代替读源码**——AI 用 `where.py` 从符号/URL/topic/srv 名直接定位改动点，不逐层翻文件。
4. **需求走固定问答**——新需求一律填 intake 十问清单，AI 从结构化答案理解业务意图，杜绝自作主张（§6）。
5. **最小闭环先行**——先跑通"一张需求单 → 定位 → 修改 → 验证"，再铺开全部卡片与脚本。

---

## 2. 现有 .trellis 骨架评估（2026-07-22 盘点）

结论（= 决策 ①）：**方向匹配，原地扩展，不推倒重建。**

**匹配、直接沿用的机制**：

| 现有机制 | 现状 | 与目标的关系 |
| --- | --- | --- |
| 任务四件套 `tasks/<id>/{prd.md, task.json, implement.jsonl, check.jsonl}` | 4 个已归档任务（2026-06）跑通过流程 | prd = 需求载体；jsonl manifest = spec 注入清单，正是"低 token 喂上下文"的现成骨架 |
| spec 自动注入（sub-agent 按 manifest 自动加载 spec 文件） | `spec/backend/` 已有 database-guidelines 与 endpoint/datafeed contracts 两份 | 机制沿用；内容按新架构层卡片重组 |
| workspace journal | `workspace/mfms-core/`（1 次会话） | 会话史沿用 |

**缺口（本方案补齐）**：

1. 骨架预设面向通用 fullstack Web（backend/frontend guidelines 清单），与 C++/ROS2 中台错位 → frontend 预设整组跳过；
2. 无 ARCHMAP、无层卡片、无 constraints → AI 仍需读源码才知道分层；
3. `.trellis/scripts/` 目录实际不存在（bootstrap prd 引用的 `task.py` 缺失）→ where.py / ctx.py / task.py / map_check.py 全部待建；
4. 无 intake 需求清单机制；
5. `00-bootstrap-guidelines` 任务自 2026-06-08 挂起（in_progress），其"按通用清单填 spec"路线不再走 → 按 §4 组装方案落位后关闭该任务。

---

## 3. 四项已拍板决策（均确认 A）

| # | 决策 | 内容 |
| --- | --- | --- |
| 1 | 骨架处置 | **原地扩展**现有 `.trellis`：目录、任务四件套、spec 注入机制全部沿用，不新起目录不换工具 |
| 2 | 定位机制 | **符号锚点 + rg 实时解析 + finish 校验**：层卡片只写符号名，`where.py` 用 rg 现场解析行号（文档不存行号、不腐烂），任务 finish 时校验锚点仍有效 |
| 3 | 需求管理 | **每需求一文件 + INDEX**：`intake/REQ-YYYYMMDD-NN-*.md`，`INDEX.md` 汇总状态索引 |
| 4 | 落地策略 | **先建最小闭环**，跑通后再铺开，不一次性写满所有卡片与脚本 |

---

## 4. .trellis 组装方案（基于新架构的落位）

决策 ① 的落位——在现骨架上补齐 §2 缺口：

```text
.trellis/
├── ARCHMAP.md                  # 上半: hyRMS 全景(≈15行) 下半: 中台四部分 + 迁移三态表
├── spec/
│   ├── constraints.md          # 红线 10 条 = 架构篇 §4
│   ├── contracts/
│   │   ├── bus-contracts.md    # P0 产出：四总线 msg/srv/信封/错误码/单位
│   │   └── db-event-contracts.md # P0 产出：双事件表状态机与消费语义（以现库为准）
│   ├── layers/
│   │   ├── 10-app.md           # 应用层(HttpServer/OPCUA)；层卡片固定五小节:
│   │   ├── 20-business.md      #   职责边界/关键锚点表(只写符号)/修改守则/禁区/验证命令
│   │   ├── 30-gateway.md       # 门面（薄卡）
│   │   ├── 40-capability.md    # 能力层四通道(StateManager/CoreAdapter/ProxyAdapter/MfmsDbService)
│   │   └── legacy-mfms.md      # 老中台一张卡（冻结态，链接旧架构笔记，只服务修 bug）
│   └── backend/                # 既有 DB 规范保留至老库退役
├── scripts/
│   ├── where.py                # 统一定位器；新增 --route: URL/topic/srv 名 → handler
│   ├── ctx.py  task.py  map_check.py
├── intake/
│   ├── TEMPLATE.md             # 十问不变；Q6 平面选项改: 应用面/业务面/总线通道/DB事件通道/控制器侧/仅GUI
│   ├── INDEX.md
│   └── REQ-YYYYMMDD-NN-*.md
├── tasks/                      # 沿用；P0-P6 各为一组任务
└── workspace/mfms-core/        # journal 沿用
```

---

## 5. 迁移期专用机制

- **三态标记**：ARCHMAP 每个组件标 `[旧·运行中] [新·建设中] [已切换]`，AI 一眼知道该改哪边。
- **双卡片路由**：`ctx.py` 读需求单 Q6——涉及老 GUI/老中台 bug → 加载 `legacy-mfms.md` + 旧架构笔记；新功能 → 只加载新层卡片。避免新旧上下文互相污染。
- **锚点语言无关**：where.py 的符号锚点机制对 Python/C++ 一视同仁（rg 不挑语言），技术栈定了不用改工具。
- 层卡片在各 P 阶段结束时随代码落地补锚点——**卡片跟着代码走，不提前写空卡**。

---

## 6. 需求 intake：固定问答清单

- 每条新需求 = 复制 `intake/TEMPLATE.md` 的固定**十问**填答（十问版式沿用已定稿，不改），存为 `intake/REQ-YYYYMMDD-NN-<slug>.md`，登记进 `INDEX.md`。
- **Q6（触达平面）选项按新架构修订**：应用面 / 业务面 / 总线通道 / DB事件通道 / 控制器侧 / 仅GUI。`ctx.py` 据此路由要加载的层卡片（§5 双卡片路由）。
- AI 只依据需求单 + 路由到的卡片开工，范围之外**不得自行发挥**；任务 `prd.md` 由需求单生成，答案不全就先问再做。

---

## 7. 分期建设路线（P0-P6）

每期一条可演示的竖切：

| 期 | 内容 | 可演示结果 |
| --- | --- | --- |
| P0 | **契约冻结**：四条总线的 msg/srv 清单（HyRMS 服务/消息 + 设备服务/消息）、MySQL 双事件表状态机文档化（以现库为准）、HTTP/推送 API schema、requestId 信封、错误码分段、单位铁律。产出 `.trellis/spec/contracts/bus-contracts.md` + `db-event-contracts.md` | 契约评审通过；与控制器开发者对齐接缝 |
| P1 | 能力层骨架：StateManager 缓存 + CoreAdapter + ProxyAdapter（架 Cpp-Proxy-SDK）+ MfmsDbService 平移 | CLI 打印真实设备状态快照，断线有置疑标记 |
| P2 | 多设备管理 + MfmsHttpServer（先轮询 API，schema 按订阅制预留）+ 最小安全（token） | 浏览器/Qt 看设备状态、下发一条 jog 并收到同 ID 结果 |
| P3 | 脚本竖切（DB 事件表通道）+ 推送升级（若架构篇 [[MFMS数据中台新版架构设计-分层功能与拍板记录#5.5 推送机制建议（待拍板）|§5.5 建议]]获批：WS 替换轮询） | 提交/启停脚本状态镜像正确；状态改推送零轮询 |
| P4 | 参数配置/日志处理完善 + 运单视图（经 CoreAdapter 调度查询） | 查到 M4 运单并展示；参数配置读写走通 |
| P5 | 安全硬化 + OPC UA + 审计完善 | MES 经 OPC UA 读到状态 |
| P6 | Qt GUI 切 Http/推送接入，老中台退役 | qt_file 删除 7 层中间层 |

并行约束：老 qt_file 中台**冻结只修 bug**；P0-P5 期间不做 LightCore 老代码重构（理由见 §8）。

---

## 8. 与 LightCore v2.1 的关系：冻结执行、吸收设计

LightCore 是"把 7 层收敛为 4 边界"的**旧进程内重构**方案。新架构把中台整个搬出进程，旧 7 层最终随 P6 整体退役——再花力气重构注定要拆的代码是浪费。处置：

- **吸收进 P0/新设计**：命令信封与 requestId 全链路（LightCore §4.3）、每设备 CommandLane 串行语义（→控制器侧实现约束，写进 bus-contracts）、确定性停机清单（LightCore §4.10 →ControllerLink 与控制器）、双类测试基线思想（P0 就为契约写测试）。
- **冻结不执行**：删 gateway、拆 Worker、单 executor 压测等对旧代码的手术。
- **例外**：若 P0-P2 期间旧系统出现必须修的并发 bug，按 legacy 卡片最小修复，不顺手重构。

---

## 9. 下一步

1. 推送机制拍板（建议见架构篇 [[MFMS数据中台新版架构设计-分层功能与拍板记录#5.5 推送机制建议（待拍板）|§5.5]]）→ 架构定稿；
2. 正式架构 SVG（按 vault 规范，当前先用 drawio 迭代）；
3. `ARCHMAP.md` 首版 + `spec/constraints.md` 落盘（红线 10 条已在架构篇 §4 备好）；
4. 最小闭环：`where.py` + 首张层卡片 + 首张需求单走通"定位 → 修改 → 验证"全流程；
5. 收编关闭 `00-bootstrap-guidelines` 任务。

---

## 10. 关联

- [[MFMS数据中台新版架构设计-分层功能与拍板记录]] —— **姊妹篇（长什么样）**：分层/功能清单/红线/两轮拍板/drawio 解读/图源
- [[MFMS数据中台技术文档-架构线程与代理层]] —— 旧架构现状（`legacy-mfms.md` 卡片的事实来源）
- `src/mfms_server/design/REFACTOR_PROPOSAL_LightCore_v2.md` —— §8 的处置对象
- `.trellis/tasks/00-bootstrap-guidelines/prd.md` —— 现骨架 bootstrap 任务（待收编关闭）
