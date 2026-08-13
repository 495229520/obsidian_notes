---
title: MFMS 旧版数据中台 · Trellis 工程结构
date: 2026-08-11
tags:
  - 研一上学期
  - 复合机器人汇总
status: 现状快照（.trellis v0.6.10，2026-08-11 盘点）
---

# MFMS 旧版数据中台 · Trellis 工程结构

> [!abstract] 本文定位
> **落地现状的盘点**，不是方案设计。2026-07-22 那篇 [[MFMS敏捷开发工作流-trellis规划与需求流程]] 写的是「打算怎么建」（四项决策 + 缺口清单），本文写的是**建完之后磁盘上真实长什么样**：v0.6.10，29 份 spec / 1412 行，4 个活动任务 + 9 个归档任务，5 次会话记录。方法论层面的取舍见 [[从 vibe coding 到 spec coding：我用 Trellis 的实践总结]]；被这套流程管着的那个系统本体见 [[MFMS数据中台技术文档-架构线程与代理层]]。
>
> **架构图**：`旧版数据中台-Trellis工程结构.drawio`（与本文同目录，三页：目录与组件 / 上下文注入回路 / 任务生命周期）。

---

## 1. 一句话结论

Trellis 不是「更聪明的提示词」，是一套**把项目约定从模型记忆里搬到磁盘上、再用 hook 定点推回上下文**的目录约定加脚本。它值钱的地方只有一条：

> 会话上下文会被压缩、会丢；`.trellis/` 下的纯文本文件不会。

旧版数据中台是 C++/Qt/ROS2 工业代码，改错一行的后果不是 500 页面而是机械臂动起来。所以这套东西在这个项目里被改造成了**带风险门禁的版本**——R0–R3 分级、R3 必须人拍板、真机动作前二次确认目标，全部写死在 `workflow.md` 里，每一轮对话强制注入。

---

## 2. 全貌：四层

对应架构图第 1 页。四层之间是**单向依赖**：平台适配层只是内核的三种宿主实现，脚本层是唯一写入口，数据层是全部事实。

| 层 | 位置 | 规模 | 职责 |
| --- | --- | --- | --- |
| ① 平台适配 | `.claude/` `.codex/` `.agents/` | 3 套 hook + 3 个子代理 + 12 个技能 | 把内核挂到具体 AI 工具上；`trellis update` 生成 |
| ② 内核 | `.trellis/workflow.md` `config.yaml` | 731 行 + 若干开关 | 工作流的唯一事实源 |
| ③ 脚本 | `.trellis/scripts/` | 5 个入口脚本 + `common/` 20 模块 | 纯 Python 3，无第三方依赖，人和 AI 共用 |
| ④ 数据 | `spec/` `tasks/` `workspace/` `.runtime/` | 29 份 spec + 13 个任务 + 4 份 workspace | 真正被注入进上下文的东西 |

三套平台适配是**同构**的：`.claude/hooks/` 和 `.codex/hooks/` 下是三个同名脚本，`.agents/skills/` 是平台中立的技能副本。换 AI 工具不用改内核，只是换一个宿主。

---

## 3. 内核层：`workflow.md` 的双重身份

731 行，同一份文件同时服务两个读者：

- **对人**：是操作手册——五条原则、任务系统命令清单、Phase 1/2/3 逐步详解；
- **对机器**：是**面包屑数据库**——里面嵌了 6 个 `[workflow-state:STATUS]` 标签块，每一轮用户输入时被 hook 抠出来注入。

### 3.1 面包屑契约

```text
[workflow-state:planning]
Load `trellis-brainstorm`; stay in planning.
R1: prd.md 够用……  R2: 补齐三件套……  R3: 等明确批准再 task.py start
[/workflow-state:planning]
```

这几行不是给人看的文档，是**每轮真会被塞进模型上下文的几十个 token**。设计上有三点值得记：

1. **脚本里没有兜底文案**。v0.5.0-rc.0 之后，`inject-workflow-state.py` 只负责解析，不内置任何默认提示。找不到标签就退化成一句显眼的通用提示——故意让人立刻发现 `workflow.md` 被改坏了，而不是悄悄用旧文案继续跑。
2. **不变量**：凡是步骤表里标了 `[required · once]` 的，必须在对应阶段的面包屑块里有一句对应的强制提示。理由写在 `workflow.md` 的注释里，且是血的教训——「面包屑是唯一的每轮通道，漏写就等于这一步被静默跳过」，Phase 1 的规划门和 Phase 3.4 的提交，历史上都是从这个缺口漏掉的。
3. **`[workflow-state:completed]` 是死块**。`task.py archive` 在同一次调用里既写 `status=completed` 又把目录移进 `archive/`，活动任务指针当场失效，解析器再也找不到这个任务——所以这个块在正常流程里**永远不会触发**。它留在文件里，是给将来某个显式 `in_progress → completed` 转换预留的。

`workflow.md` 被列进了 `config.yaml` 的 `update.skip`：**项目手改，升级不覆盖**。

### 3.2 `config.yaml` 里真正影响行为的几个开关

| 开关 | 本项目取值 | 意味着什么 |
| --- | --- | --- |
| `session_auto_commit` | `false` | 脚本只写盘，**绝不替你 commit**。journal、任务归档都落磁盘，提交由人决定 |
| `max_journal_lines` | `2000` | 会话流水超限自动开 `journal-2.md` |
| `codex.dispatch_mode` | `auto` | 派发子代理，而非 inline 主会话直改 |
| `channel.worker_guard` | idle `5m` / 最多 `6` | 常驻 worker 的 OOM 护栏 |
| `update.skip` | 4 个文件 | `config.yaml` `workflow.md` `packages_context.py` 与 brainstorm 技能，升级时保留手改 |
| `context_injection` | 默认注释态（32K / 64K / 128K） | 子代理注入的截断上限，见 §6.3 |

`session_auto_commit: false` 这条在工业项目里不是洁癖：仓库里同时躺着 SDK 二进制、厂商导出目录和现场配置，任何自动提交都可能把不该进版本库的东西带进去。

---

## 4. 脚本层：唯一写入口

### 4.1 `task.py`

任务生命周期的全部动作都从这里走，AI 和人用同一套命令：

```bash
task.py create "<title>" [--slug <name>] [--parent <dir>]  # 建目录 + task.json + prd.md，并自动设活动指针
task.py start <name>          # 指针幂等重写，status: planning → in_progress
task.py current --source      # 看当前盯的是哪个任务、指针从哪来
task.py archive <name>        # status=completed 并移入 archive/{年-月}/
task.py add-context <name> <action> <file> <reason>   # 往 implement/check.jsonl 写一条
task.py validate <name>       # 校验任务制品完整性
task.py add-subtask <parent> <child>                  # 父子树
task.py create-pr [name] [--dry-run]
```

`add-context` 的四个参数里，`reason` 是必填的——这是设计上的强制：**你必须说清「为什么这个任务要读这份 spec」**，写不出理由的条目就不该进清单。

### 4.2 活动任务指针为什么是「每会话」而不是「每项目」

指针文件长这样：`.runtime/sessions/<平台>_<会话id>.json`。当前盘到 3 个 `codex_*` 会话文件。

一个全局的「当前任务」在多窗口场景下会立刻打架：左边窗口在查 Qt 日志格式，右边窗口在写互锁方案，两边共享一个指针必然互相踩。按会话隔离之后，三个窗口各盯各的任务。代价是：**拿不到会话身份就没有活动任务**，`task.py start` 会直接失败并提示身份问题——宁可失败，也不静默退回全局状态。

这个目录进 `.gitignore`，不入版本库。

### 4.3 `get_context.py`

```bash
get_context.py                            # 会话运行时全量
get_context.py --mode packages            # spec 层清单
get_context.py --mode phase --step 1.1    # 单步详细指引
```

`--mode packages` 在本项目的输出是：

```text
Single-repo project (no packages configured)
Spec layers: adapters, backend, frontend, interfaces
Shared guides: guides
```

**这是关键的一次性省 token**：AI 不用 `ls` 一层层翻目录猜规范放哪，一条命令拿到全部层名，再按需读对应的 `index.md`。

---

## 5. 数据层

### 5.1 `spec/` —— 29 份，1412 行

按包与层组织，每层一个 `index.md` 作入口：

| 层 | 份数 | 覆盖 | 大头 |
| --- | --- | --- | --- |
| `backend/` | 8 | `src/mfms_server/` | `logging-guidelines.md` 195 行、`mfms-endpoint-and-datafeed-contracts.md` 132 行 |
| `frontend/` | 8 | `src/qt_file/` | `qt-client-layering.md` |
| `adapters/` | 4 | `mfms_fr_adapter`、SDK 边界 | `sdk-and-runtime-boundary.md`、`real-device-safety.md` |
| `interfaces/` | 3 | `src/com_interfaces/` msg/srv | `ros-interface-contracts.md` |
| `guides/` | 6 | 跨包思考 | `industrial-network-and-ssh.md` 218 行、`mfms-architecture.md` 140 行 |

每个 `index.md` 固定三节：**开发前检查 / 质量检查 / 文件索引**。前两节是复选框清单，直接被子代理当验收标准用。例如 `backend/index.md` 的质量检查第一条：

```markdown
- [ ] 数据仍按 `CommunicationInterface -> Worker -> Gateway -> DB/ROS/CMD` 传播。
```

这一条锁死的就是 [[MFMS数据中台技术文档-架构线程与代理层]] 里那条主链，防止任何人顺手绕过 Gateway 直连数据库。

**沉淀下来的内容是有分量的，不是套话。**举三条：

- `database-guidelines.md`：prepared SELECT 取 MySQL `JSON` 字段必须写 `CAST(col AS CHAR(N)) AS col`。原因写在规范里——QMYSQL 的 prepared-statement 路径按 JSON 字段声明的最大长度预分配结果缓冲区，单字段能产生约 4 GB 映射和约 1.1 秒 prepare 延迟。这是从一次真实的 1 秒延迟排查里抠出来的，写进 spec 之后再没人踩第二次。
- `logging-guidelines.md`：`flush()` 只许用于 fatal、有序关闭和测试门禁，业务线程 / 数据库轮询线程 / ROS 回调**禁止调用**；crash handler 必须在 writer 初始化前安装、在 `shutdown()` 完成后才卸载。
- `industrial-change-safety.md`：「默认只运行构建、lint、mock、simulator 和只读检查。真机动作、真实数据库写入或部署**不因『测试』二字自动获得授权**。」

还有一条贯穿全局的边界，写在多份 spec 里：**LightCore v2.1 是候选路线图，不是当前实现授权**——按那份提案删 Gateway、改线程模型、统一命令总线，必须单独立 R3 任务。新版设计本身见 [[MFMS数据中台新版架构设计-分层功能与拍板记录]]。

### 5.2 `tasks/` —— 活动 4 + 归档 9

单任务目录最多七件套：

```text
tasks/{MM-DD-name}/
├── task.json            # status / scope / priority / relatedFiles / notes / meta
├── prd.md               # 要什么、约束、验收标准                     ← 必有
├── design.md            # 边界、契约、数据流、兼容、回滚形态          ← 复杂任务必有
├── implement.md         # 有序清单、验证命令、评审门、回滚点          ← 复杂任务必有
├── research/            # 调研产物落盘
├── implement.jsonl      # 给实现子代理的 spec 注入清单
└── check.jsonl          # 给检查子代理的 spec 注入清单
```

`task.json` 里除了固定字段还有一个自由的 `meta`，实际用来记结论性的判定。比如 `08-10-composite-robot-motion-interlock`（机械臂与底盘运动互锁）里：

```json
"meta": {
  "safety_blocker": "true",
  "implementation_authorized": "false",
  "release_gate": "blocked_pending_interlock",
  "new_interface_safety_coverage": "unproven",
  "m4_data_center_boundary": "isolated"
}
```

**这是把「不许动」这件事变成机器可读的状态**，而不是埋在某段聊天记录里。

已归档的 9 个任务：

| 归档月 | 任务 | 性质 |
| --- | --- | --- |
| 2026-06 | MoveJ 关节到点 GetForwardKin 埋点定位 1s 延迟 | 性能定位 → 沉淀成 DB 的 `CAST` 规范 |
| 2026-06 | Qt 控制页机械臂列表弹窗（ID 前 10 位 + DB 状态） | 前端 |
| 2026-06 | robot-device-status-dropdown | 前端 |
| 2026-06 | 首页设备下拉框 DB 删除后自动刷新 | 前端 |
| 2026-07 | Bootstrap Guidelines | 规范初始化 |
| 2026-07 | 记录工控机 SSH 与设备网络拓扑 | 运维记忆 → 见 [[MFMS工控机网络修复记录-多网卡路由与SSH直连]] |
| 2026-08 | 下位机离线弹窗 | 前端 |
| 2026-08 | MFMS 离线诊断日志系统 | 大改 → 沉淀成 195 行日志规范 |
| 2026-08 | 升级数据中台下位机接口到 2026-08-10 版本 | 接口 |

当前 4 个活动任务：机械臂运动结果离线修复（planning）、Qt 关节日志格式（in_progress）、数据中台架构指南（in_progress）、复合机器人运动互锁（planning，R3 卡在批准门）。

### 5.3 `workspace/` —— 跨会话记忆

```text
workspace/
├── index.md                      # 全体开发者总表
└── mfms-core/
    ├── index.md                  # 5 次会话，最近 2026-08-08
    ├── journal-1.md              # 会话流水，超 2000 行自动开 journal-2.md
    └── network-access.md         # 运维记忆：工控机 SSH、网卡地址、设备网关系
```

`index.md` 里有 `<!-- @@@auto:session-history -->` 标记的自动区块，由 `add_session.py` 回填，人手写的部分不受影响。

---

## 6. 上下文注入回路

对应架构图第 2 页。三个 hook，三个时机，三种粒度。

| 时机 | 脚本 | 读什么 | 产出 |
| --- | --- | --- | --- |
| `SessionStart`（startup/clear/compact） | `session-start.py` | `workflow.md`、`.developer`、`workspace/<dev>/index.md`、活动任务 | 会话开场上下文，每会话 1 次 |
| `UserPromptSubmit`（每轮） | `inject-workflow-state.py` | 会话指针 → `task.json.status` → 对应的 `[workflow-state:*]` 块 | `<workflow-state>` 面包屑，每轮几十 token |
| `PreToolUse: Task/Agent`（派发瞬间） | `inject-subagent-context.py` | `implement.jsonl` / `check.jsonl` 点名的文件正文 + 任务制品 | 子代理首轮 prompt |

三者的 token 成本差着数量级，用途也完全不同：开场那次是「你在哪个项目、上次干到哪」，每轮那次是「你现在处于哪个阶段、这阶段不许干什么」，派发那次是「你这个子代理需要知道的全部规矩」。

### 6.1 jsonl 清单是什么

每行一个 JSON 对象，`{file, reason}`：

```jsonl
{"file": ".trellis/spec/adapters/real-device-safety.md", "reason": "Require exact device IDs, safety I/O documentation, emergency-stop readiness and explicit approval before any real-device action."}
{"file": ".trellis/tasks/08-10-.../research/confirmed-hyrms-m4-boundary.md", "reason": "Treat same-HyRMS Aubo plus SeerCtrl orchestration and M4 data-center isolation as the confirmed implementation boundary."}
```

`task.py create` 会先塞一行 `_example` 种子，规划阶段（步骤 1.3）由 AI 换成真条目。

**清单里只放 spec 和 research，不放代码路径**——这条硬规则写在种子行里。理由有两层：代码由子代理自己按需检索，清单的职责是「按什么规矩改」而不是「改哪几行」；混进代码路径会让注入体积失控，而且代码路径腐烂得极快，清单会迅速变成一堆死链。

`implement.jsonl` 和 `check.jsonl` 通常指向同一批 spec，但 `reason` 不同：前者写「设计时要保住什么」，后者写「验收时要核对什么」。以互锁任务为例，同一份 `real-device-safety.md`，实现侧的理由是「动真机前需要确切设备 ID、安全 I/O 文档、急停就绪和明确批准」，检查侧的理由是「核实真机验收仍处于阻断状态」。

### 6.2 三个子代理的权限边界

| 子代理 | 读序 | 硬边界 |
| --- | --- | --- |
| `trellis-implement` | `implement.jsonl` → `prd` → `design` → `implement` → 按需 `spec/` | **禁止 `git commit` / `push` / `merge`**，提交权只在主会话 |
| `trellis-check` | `check.jsonl` → 制品 → `git diff` | 机械小问题当场自修，其余带 `file:line` 报出，区分「已修」与「仍开着」 |
| `trellis-research` | 自由检索 | **每条结论必须落盘到 `tasks/<t>/research/`**，该目录之外一律不许改 |

还有一条**自防递归**规则：派发只发生在主会话。已经身为 `trellis-implement` 的代理，不许再派 implement 或 check；已经身为 `trellis-check` 的，同理。所有派发 prompt 的第一行固定是 `Active task: <task path>`——子代理不继承父会话记录，靠这一行冷启动定位磁盘制品。

`trellis-research` 那条「只能写 research/」是这三条边界里回报最高的：调研成果留在磁盘上，下一次注入直接复用，不用重查。

### 6.3 截断策略

`config.yaml` 的 `context_injection` 三个上限（默认注释态，即使用内置默认值）：

| 上限 | 默认 | 超限行为 |
| --- | --- | --- |
| `max_file_bytes` | 32 KB | 单个 jsonl 条目文件截断并留提示 |
| `max_artifact_bytes` | 64 KB | 单个任务制品（prd/design/implement）截断 |
| `max_total_bytes` | 128 KB | **触顶后剩余文件降级为索引行**（路径 + 理由 + 大小） |

降级成索引行而不是直接丢弃，是个细节但很重要：子代理仍然知道「还有这么一份 spec 存在、它管什么」，需要时可以自己去读，而不是完全不知情。

---

## 7. R0–R3 与三阶段

对应架构图第 3 页。**分级看后果，不看改了几行**——这是工业项目和 Web 项目最大的差别，一行 `#define` 改错可能让机械臂以错误单位运动。

### 7.1 风险分级

| 级别 | 范围 | 门禁 |
| --- | --- | --- |
| **R0** | 问答、解释、只读检查 | 不建任务，直接答 |
| **R1** | 单模块、不动契约的局部修复 | 自动建轻量任务，`prd.md` 即可；无未决决策可直接进实现 |
| **R2** | 跨文件 / 跨层软件改动 | 自动建任务，`prd` + `design` + `implement` 三件套齐全，两份 jsonl 必须是真实上下文；开工前先把最终方案讲给用户 |
| **R3** | 真机运动 · 数据库结构与数据 · 公开 C++/Qt/ROS 接口 · 线程与生命周期 · 部署配置 · 凭据 · SDK 二进制 | **planning 做完后停住**，拿到对*最新方案*的明确批准前不许 `task.py start`；每次外部写入前再核一遍确切目标 |

R3 的批准是**有作用域**的：只覆盖说明过的目标和范围，目标、环境或动作一变就要重新批准。这条规则的实际效果可以在 `08-10` 那个互锁任务里看到——`implementation_authorized: false`，任务停在 planning，`research/` 下已经写了三份调研（接口审计、集成影响矩阵、边界确认），但一行实现代码都没写。

### 7.2 三阶段

| 阶段 | 步骤 | 关键点 |
| --- | --- | --- |
| **Phase 1 Plan**（`planning`） | 1.0 建任务 `[必做·一次]` → 1.1 需求探索 `[必做·可重复]` → 1.2 调研 `[可选]` → 1.3 配 jsonl `[必做·一次]` → 1.4 激活 `[必做·一次]` → 1.5 完成标准 | 1.4 是**评审门**，R3 在此等人拍板 |
| **Phase 2 Execute**（`in_progress`） | 2.1 实现 → 2.2 质量检查 → 2.3 回滚 `[按需]` | 2.2 含原 3.1 的全量收尾检查；**没跑过检查不许说「做完了」** |
| **Phase 3 Finish** | 3.2 调试复盘 `[按需]` → 3.3 更新 spec `[必做·一次]` → 3.4 提交 `[必做·一次]` → 3.5 收尾 | 3.3 是复利来源；步骤 3.1 已并入 2.2 和 3.4，编号故意留空以免打断外部引用 |

**阶段可以倒流**，这被明确写成正常路径而非失败路径：执行中发现 `prd` 有缺陷 → 退回 Phase 1 改 → 重进 Phase 2。

状态机只有三档：`planning ──start──▶ in_progress ──archive──▶ completed`。注意 Phase 3 全程状态仍是 `in_progress`——只有 `archive` 才翻牌，这也是 §3.1 那个死块的成因。

---

## 8. 这套东西实际留下了什么

抛开流程本身，一年跑下来磁盘上多出的东西是：

- **1412 行 spec**，其中至少三处是从真实事故里抠出来的硬规则（JSON 字段 `CAST`、`flush()` 调用边界、真机验证不因「测试」二字获得授权）；
- **9 个归档任务的完整决策链**，每个都能回溯到「当时为什么这么改」；
- **一份运维记忆**（`network-access.md`）——工控机 SSH、网卡路由、设备网关系，这类知识既不在代码里也不在 git history 里，不落盘就只存在于某个人的脑子里；
- **一个机器可读的发布阻断标记**（`release_gate: blocked_pending_interlock`）。

反过来说，这套东西**没有**解决的：它管不住模型在具体某一行代码上的判断力，只能保证「该看的规矩都推到眼前了」。规矩本身写得对不对、覆盖全不全，仍然是人的责任。

---

## 9. 已知毛刺

盘点时记下的、暂未处理的：

1. `[workflow-state:completed]` 死块（§3.1）——不影响功能，但读 `workflow.md` 的人容易误以为它在生效。
2. `frontend/` 那 8 份 spec 有一部分是初始化时按通用 Web 模板生成的（`component-guidelines` / `hook-guidelines` / `state-management` / `type-safety`，各 20–24 行），与 C++/Qt 场景的贴合度明显低于 `qt-client-layering.md`。真正在用的是后者。
3. `.trellis/scripts/hooks/linear_sync.py` 在本项目没有接外部任务系统，属于闲置件。
4. `08-03-arm-motion-result-offline-fix` 自 8 月 3 日建起一直停在 planning（等验证条件），流程里没有「长期挂起任务如何处置」的规定——既没有超时提醒，也没有主动关闭的口子。
