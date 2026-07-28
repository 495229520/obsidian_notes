---
tags:
  - AI编程
  - claude
---

# Claude Skills 参考目录

> 当前生效的个人 Skills 清单。最后更新：2026-07-26（从 31 项精简到 15 项）。

Skills 实际位于 `~/.claude/skills/`（用户级，全局生效），**不在** vault 的 `.claude/` 下。vault 里的 `.claude/` 只有 `settings.local.json` 和 worktrees。

---

## 一、笔记流水线（核心）

三个 Skill 是一条链：**locator 找 → creator 写 → extender 补**。

| Skill           | 触发场景                       | 功能                                      |
| --------------- | -------------------------- | --------------------------------------- |
| `note-locator`  | "找一下…"、"搜索…"、"…在哪"         | 模糊搜索笔记 + 解析 wiki-link 关联网。带 `locate.py` |
| `note-creator`  | "写一个…"、"创建…"、"出一套题"、"写个题解" | 新建笔记。覆盖 C++/系统编程、LLM 应用、投资、力扣题解、章节题目汇总  |
| `note-extender` | "检查笔记"、"补充一下"、"查漏补缺"       | 对比权威书籍审查已有笔记，生成扩展内容。带 `analyze.py`      |

### note-creator 的结构

2026-07-26 由五个 Skill 合并而来（原 `note-creator` + `llm-note-creator` + `invest-note-creator` + `leetcode-solver` + `exam-creator`）。

- `SKILL.md`（77 行）只留三条硬约束：代码必须配讲解、C++ 严格 Modern C++、wiki-link 只链已存在的笔记。
- `references/vault-map.md`（109 行）存数据：目录映射、各领域权威书单、五类笔记骨架。按需读。

> [!note] 目录表以磁盘为准
> 合并时校正了三处过期路径：`LLM应用/` 实际是 `7.LLM应用/`，`投资/` 实际是 `12.投资/`，力扣题解骨架改成了 vault 里现存笔记的实际八段结构。旧 Skill 里规定的 SVG 前缀（`lc_`、`5_1_`）在 166 个 SVG 文件里一个都不存在。

---

## 二、Obsidian 格式与资产

| Skill | 触发场景 | 功能 |
|---|---|---|
| `obsidian-svg` | "画图"、"SVG"、"架构图"、"内存布局" | 建图 + 优化 + 验证 + vault 资产管理。带 4 个 Python 脚本和 4 张参考图 |
| `obsidian-markdown` | wikilink、callout、frontmatter、embed | Obsidian 方言语法 |
| `obsidian-bases` | `.base` 文件、表格视图、公式 | 把笔记做成数据库视图 |

`obsidian-svg` 是唯一保留的画图 Skill，已吸收原 `svg-precision-skill` 和 `architecture-diagram` 的能力。

SVG 统一放 `图片/SVG/`，命名跟随所在章节编号（如 `8_0_1_1.svg`）或用描述性中文名（如 `全路径-read未命中泳道.svg`）。

---

## 三、学科专项

| Skill | 触发场景 | 功能 |
|---|---|---|
| `numerical-analysis` | "数值分析"、"考点"、"考不考"、"必看"、"出题" | 三种模式：讲考点、润色上课笔记、仿期末出题 |

唯一保留的导师型 Skill。留它的理由是它内嵌了 2020–2024 五套期末真题的考频表，任何"这个考不考"的判断以该表为准，这份数据模型推不出来。

对应笔记在 `7.LLM应用/5.数值分析/` 和 `7.LLM应用/6.数值分析笔记/`。

---

## 四、设计与写作

| Skill           | 触发场景                      | 功能                                |
| --------------- | ------------------------- | --------------------------------- |
| `claude-design` | "设计…"、"做个 deck"、"原型"、"海报" | HTML 设计产物：落地页、幻灯片、可点原型、动画、线框图     |
| `no-ai-slop`    | 写任何正文之前                   | 反 AI 味写作规则：禁破折号、禁强调副词、禁空洞断言、禁三段同构 |

---

## 五、发布

| Skill | 触发场景 | 功能 |
|---|---|---|
| `github-note-push` | 推笔记仓库到 GitHub | 处理 remote 名字错、worktree 混用、`.DS_Store` 混入、SSH key 不匹配等坑 |

---

## 六、Paseo 多代理编排

这五个由 Paseo 自动安装和管理（目录里有 `.paseo-managed-files.json`），手动删掉会被重装。都是 `user-invocable`，可以直接 `/paseo-advisor <问题>` 这样调。

| Skill | 触发场景 | 功能 |
|---|---|---|
| `paseo` | 需要建 agent 或管 worktree 时 | 底座参考：agent / worktree 的 API |
| `paseo-advisor` | "第二意见"、"advisor" | 拉一个外部 agent 给判断，只分析不动手 |
| `paseo-committee` | 卡住、绕圈、钻牛角尖 | 两个不同 provider 的高推理 agent 做根因分析和方案 |
| `paseo-handoff` | "交接给…" | 带完整上下文把任务交给另一个 agent |
| `paseo-loop` | "loop"、"盯着"、"跑到…为止" | worker/verifier 循环跑到满足退出条件 |

后四个都会先读 `~/.paseo/orchestration-preferences.json` 挑 provider。

---

## 七、Claude Code 自带（无需维护）

跟着版本走，不在 `~/.claude/skills/`：

`dataviz`（图表配色版式）、`artifact-design` / `artifact-capabilities`（发布 Artifact 页面）、`claude-api`（Claude API 与 SDK 参考）、`simplify`（改动的化简清理）、`security-review`、`review`、`run`、`init`、`update-config`（改 settings.json 和 hooks）、`keybindings-help`、`fewer-permission-prompts`、`loop`、`schedule`。

---

## 八、2026-07-26 删除记录

删掉 20 项，约 5500 行指令。记在这里是为了避免以后又装回来。

| 类别 | 删除的 Skill | 理由 |
|---|---|---|
| 已损坏 | `claude-design`（软链） | 指向不存在的 `~/code/claude-design-skill` |
| 已损坏 | `remote-ctrl-tutor`、`remote-ctrl-note`、`debug-logger` | 全部写死 `D:\obsidian\...` 和 `D:\c++\project\remote_ctl\`，Mac 上源码仓库不存在 |
| 已损坏 | `tutor` | 依赖的 `StudyVault/` 从来没建过 |
| 被工具淘汰 | `mermaid-to-drawio` | drawio MCP 的 `open_drawio_mermaid` 直接吃 mermaid，原生支持 28 种图表 |
| 被工具淘汰 | `defuddle` | 依赖的二进制根本没装 |
| 纯后训练能力 | `concept-explainer`、`socratic-teaching-scaffolds`、`cpp-tutor` | 讲解、类比、苏格拉底提问都是模型已饱和的能力 |
| 被超集覆盖 | `svg-precision-skill`、`architecture-diagram` | 归入 `obsidian-svg` |
| 被超集覆盖 | `web-design-engineer` | 与 `claude-design` 职责完全重合，后者调用量 36 次对 2 次 |
| 被超集覆盖 | `obsidian-cli`、`json-canvas` | Read/Glob 够用；零调用 |
| 合并 | `llm-note-creator`、`invest-note-creator`、`leetcode-solver`、`leetcode-note`、`exam-creator` | 并入 `note-creator` |

顺带修了一个真 bug：`note-locator`（8 处）和 `note-extender`（1 处）的脚本调用写的是相对路径 `.claude/skills/.../x.py`，而 vault 的 `.claude/` 下没有 skills 目录，脚本一直跑不起来。已改成 `python3 ~/.claude/skills/...` 绝对路径。

备份在 `~/.claude/skills-backup-20260726/`。

---

## 九、维护方法

### 判断某个 Skill 该不该留

先看真实调用次数，别凭感觉：

```bash
grep -oh '"skill":"[a-z0-9-]*"' -r ~/.claude/projects | sort | uniq -c | sort -rn
```

2026-07-26 实测：30 个个人 Skill 里有一半从未被调用过一次。

### 写 Skill 的原则

只写模型**推不出来**的东西：

- 保留：vault 路径与目录映射、命名规范、模板骨架、个人硬性偏好、脚本入口。
- 删掉：「你是一位严谨的…」角色定义、「第一步…第五步」流程编排、讲解方法论、质量检查清单、正反例长篇对比。

新建 Skill 前先问一句：模型不看这份文件能不能做对。不能，才写。

数据表下沉到 `references/`，`SKILL.md` 只留动作和约束，并注明「以磁盘实际结构为准」。

---

## 关联

- [[1.1 claude code语法]]
- [[1.2 skills]] 用 Skill_Seekers 生成 skills 的方法
