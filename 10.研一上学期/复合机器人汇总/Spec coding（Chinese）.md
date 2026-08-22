
### 为什么需要 Spec coding
- 简单的小脚本和小任务可以纯vibe解决
- 模型有上下文,大型的项目过于复杂,无法理解到所有内容
- 堆积局部最优容易一层套一层,难以维护
- 大型的项目,设计到多人开发,需要做好严格的约束

![[图片/SVG/trellis-workflow.svg|1390]]

## Agent = Model + Harness

- Harness是一种工程规范，经验沉淀，bug日志保存，约束规范

## Rules文件：

- AGENTS.md CLAUDE.md 等
- 不同的Agent工具会生成不同的rules文件，团队不好规划，而且项目越来越大之后，Agent也会随之变大，越来越臃肿。
- 静态规则，经验无法沉淀到规则

## Skill：

- 固定化的流程，比如我想做debug、写测试、review等，是一套通用的工作流程
- 不同平台的skills格式不同

## Trellis优点：

- 免去了每次解释项目，规范化项目，把任务记录和工作记忆都沉淀到 .trellis 中
- 会保存之前的工作记录和工作记忆，遇到类似的问题可以接着上次的思路继续做
- 跨平台：无论是使用claude code、cursor还是codex，都是共享的同一套 .trellis
- 有完整的项目闭环
	- Spec 长期规范（命名规范、接口格式、错误处理方式、测试要求、架构约束）
	- Task 当前的任务
	- Workflow 当前的任务阶段（大任务会开很多个workflow，不同的workflow各自处理子任务）
	- Journal 工作记忆（已修改哪些文件、遇到什么问题、尝试过哪些方案、下次从哪里继续）
	- 任务收尾的时候，_ai_ 会引导我们判断哪些经验值得长期复用，再把稳定规则写回 _spec_ 里面

总结来说就是：Spec 约束 Task 怎么做，然后Task 按 Workflow 推进 ，Journal 记录推进的过程，最后从这一次任务中沉淀出的长期经验，再升级为 Spec，从而进行长期的自进化。

## Trellis结构：

```text
your-project/
├── .trellis/                         # Trellis 跨平台共享核心
│   ├── .developer                    # 当前开发者身份，本地文件
│   ├── .version                      # Trellis 版本
│   ├── .template-hashes.json         # 模板版本记录
│   ├── workflow.md                   # Plan → Execute → Finish 工作流
│   ├── config.yaml                   # 项目配置
│   │
│   ├── .runtime/                     # 运行时状态，通常不提交
│   │   └── sessions/
│   │       └── <session-key>.json    # 当前会话对应的活跃任务
│   │
│   ├── spec/                         # 长期项目规范
│   │   ├── frontend/                 # 前端规范
│   │   ├── backend/                  # 后端规范
│   │   └── guides/                   # 通用思考与设计指南
│   │
│   ├── workspace/                    # 开发者工作记忆
│   │   ├── index.md
│   │   └── <developer-name>/
│   │       ├── index.md
│   │       └── journal-N.md          # 每次工作的日志
│   │
│   ├── tasks/                        # 任务目录
│   │   ├── <MM-DD-task-name>/        # 当前任务
│   │   │   ├── task.json             # 任务状态、负责人、分支等
│   │   │   ├── prd.md                # 需求、范围、验收标准
│   │   │   ├── design.md             # 技术设计
│   │   │   ├── implement.md          # 实施计划
│   │   │   ├── implement.jsonl       # 实现阶段需要读取的上下文
│   │   │   ├── check.jsonl           # 检查阶段需要读取的上下文
│   │   │   └── research/             # 调研资料
│   │   └── archive/                  # 已完成任务归档
│   │
│   └── scripts/                      # Trellis 自动化脚本
│       ├── task.py                   # 任务管理
│       ├── get_context.py            # 获取项目上下文
│       ├── add_session.py            # 记录会话
│       ├── create_bootstrap.py       # 初始化规范
│       └── common/                   # 公共脚本库
│
├── .claude/                          # Claude Code 适配层
│   ├── settings.json
│   ├── commands/trellis/             # Trellis 命令
│   ├── agents/                       # implement/check/research agent
│   ├── skills/                       # brainstorm/check/update-spec 等技能
│   └── hooks/                        # session-start、状态注入等 Hook
│
├── .cursor/                          # Cursor 适配层
├── .codex/                           # Codex 适配层
├── .opencode/                        # OpenCode 适配层
└── AGENTS.md                         # Codex 等工具读取的项目入口说明
```

## 一个Task的生命周期：

```
- 创建 Task
  ↓
- 绑定到当前 Session
  ↓
- planning：规划中
  ↓
- in_progress：实现和检查中
  ↓
- 完成代码提交
  ↓
- finish-work
  ↓
- Task 归档，并清除活跃任务指针
```

- 注意：一旦创建 task，Trellis 会在里面生成一个任务目录，后面的规划、设计、实现清单、调研记录都会落到这个目录里面。

## Trellis工作流程：

##### 第一步：新的会话会首先恢复项目上下文

| **项目** | **含义** |
| --- | --- |
| developer | 当前是谁在开发，用于定位对应的 workspace/<developer>/ 和个人 Journal |
| git | 当前分支、未提交文件、最近提交，避免 AI 不知道仓库当前状态 |
| Task 的 Session Id | 根据当前会话编号，找到当前会话正在处理的任务目录 |
| Task Status | 读取任务状态，例如 planning、in_progress、completed |
| Workflow index | 读取当前工作流的阶段摘要，判断下一步是规划、实现、检查还是收尾 |
| Spec index paths | 找到项目规范的索引位置，后面按任务需要读取具体规范 |
| Workspace memory | 读取之前的 Journal，了解上次做了什么、遇到什么问题、下一步是什么 |

```
- developer是谁？
  ↓
- 仓库现在的状态？
  ↓
- 当前正在处理哪个任务？
  ↓
- 任务进行到哪一步？
  ↓
- 这一阶段下一步应该做什么？
  ↓
- 需要遵守哪些项目规范？
  ↓
- 上次工作进行到哪里？
```

##### 第二步：每一轮prompt都会把当前任务的状态，workflow的状态，下一步的动作注入prompt的上下文中，还会还需读取 PRD、设计、规范和详细 Journal

##### 第三步：判断是否需要建立 Task，小需求或者只是问一个技术问题是不会创建 task 的

##### 第四步：需求分析，Trellis在进入具体的Planning前，会进行追问，这里以做一个登录界面为例子

| **文件** | **主要回答的问题** | **示例** |
| --- | --- | --- |
| prd.md | 要做什么？做到什么算完成？ | 支持账号密码登录；失败返回统一错误格式；Token 2 小时过期 |
| design.md | 具体准备怎么做？为什么这样做？ | 增加登录接口、JWT 中间件、用户表字段，以及模块之间的调用关系 |
| implement.md | 按什么顺序执行？ | 先改数据库，再写接口，再加鉴权中间件，最后补测试 |
| research/ | 调研发现了什么？哪些结论有依据？ | 当前项目使用的 JWT 库、版本兼容性、已有用户表结构 |

##### 第五步：planning 完成后，任务就会进入 in_progress。这时 AI 开始实现，但它不会只根据我们最后一句 prompt 写代码。它会综合读取当前任务状态、task artifacts、相关 spec、调研材料和仓库里的实际代码。

##### 第六步：进入 check 阶段，会进行一次小的 code review，查看当前 diff，也会读取 check.jsonl 里声明的 spec 和 research，然后对照规范检查代码。（可以进行约束）

##### 第七步：沉淀经验，如果把所有的经验都写到spec，trellis也会越来越臃肿，反而适得其反，因此需要让 AI 引导我们判断哪些经验值得长期复用，update-spec 的重点在于筛选，避免把 spec 变成堆积材料。

##### 第八步：在git commit之后，运行 /trellis:finish-work 把任务卡标记为完成，并写工作总结

- 任务：.trellis/tasks/xxx_Task/  被放进 archive/
- 工作记录：.trellis/workspace/具体的developer/journal-1.md  文档会记录本次完成内容、问题和下一步
-  /trellis:finish-work 是要developer 自己运行的

## 日常开发中的推荐分工

```
Developer 描述需求
        ↓
AI 追问并生成 prd.md
        ↓
Developer 确认需求与验收标准
        ↓
AI 生成设计、计划并实现
        ↓
AI 运行检查和测试
        ↓
Developer 审查 diff 和关键决策
        ↓
Developer 提交代码
        ↓
AI 执行 finish-work、归档任务、更新 Journal
        ↓
Developer 审核是否把经验升级为 Spec
```

## 如何管理一个团队的Trellis？

### 首先需要知道在使用 trellis 进行团队协作时，哪些东西应该进 git，哪些不应该？

### 建议需要进入 git 的有这些：

#### _Success_

- _.trellis/spec/_：团队规范，和代码一样走 _pr review_
- _.trellis/tasks/_：任务目录（_prd_、_design_、_research_），是项目资产
- _.trellis/workspace/{name}/_：各开发者的 _journal_，_/trellis:finish-work_ 会追加 _journal_

### 不需要进入 git 共享的比如这些：

#### _Failure_

- _.trellis/.developer_：记录当前开发者名，_gitignored_
- _.trellis/.runtime/_：会话运行时状态，_gitignored_

- 这样来安排的话，spec 和 task 进 git，意味着 Spec 规范改动要像代码一样走 review。重要的 api 设计规范、测试约定、架构约束，都应该让团队看得到。
- 而且每个开发者有自己的 workspace，journal 记录各自的工作过程。它不是统一规范，但能让团队看到一些真实的开发轨迹，新人接手时也更容易理解项目是怎么演进过来的。
- .trellis/.developer 和 .trellis/.runtime/ 才是 gitignored 的部分，这些是会话级别的临时状态，不需要进入版本控制。
- 任务目录应该进 git，因为它承载需求、设计、实施计划和调研记录。但任务目录也可能冲突，所以团队里最好通过 --assignee 明确负责人，避免两个人同时改同一个任务。
- 而且新人加入更容易快速上手。交接什么的都比较方便快捷。
- 团队协作时，规范不是一成不变的。
	- 项目做了大的变更，spec 要更新。
	- 引入新的测试框架，spec 也要更新。
	- 但规范改动不能随意，重要的 spec 改动要走 pr review，需要团队讨论。
- 这样规范演进是可控的、可追溯的。不会出现某个人私自改了规范，其他人不知道的情况。
