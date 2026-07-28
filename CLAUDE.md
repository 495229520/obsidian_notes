# CLAUDE.md

本文件为 Claude Code 在此仓库工作时提供指引。最后核对：2026-07-26。

## 仓库概览

这是一个 **Obsidian vault**，主线是 C++ 与系统编程，笔记用**简体中文**写。

| 目录 | 内容 | 规模 |
|---|---|---|
| `0.ai 编程/` | Claude Code、Skills、AI 工具链笔记 | 8 篇 |
| `1.C++基础/` | C++ 基础，13 章 | 117 篇 |
| `2.C++高级/` | 高级 C++ 与 STL，17 章 | 50 篇 |
| `3.Linux/` | Linux 系统编程与网络，17 章 | 97 篇 |
| `4.windows/` | Windows 系统编程，5 章 | 28 篇 |
| `5.算法/` | `5.1 STL`、`5.2 算法`、`5.3 力扣` | 87 篇 |
| `6.项目/` | 项目笔记（含 `03.远控系统`、`06.远控服务端`） | 150 篇 |
| `7.LLM应用/` | LLM 工程；含 `5.数值分析`、`6.数值分析笔记` | 90 篇 |
| `8.高性能存储/` | 存储方向主线 | 160 篇 |
| `12.投资/` | 投资学习 | 102 篇 |
| `图片/` | 图片资产；`图片/SVG/` 存 SVG，`图片/书籍/` 存 PDF |  |
| `模版/` | 笔记模板 |  |

`7.LLM应用/` 和 `12.投资/` 各有一份 `培养方案.md`，新建周笔记前先读它确认主题和周次。

> **笔记定位**：用 `note-locator` Skill 动态查找，不要靠记忆目录结构。目录会变，**以磁盘实际结构为准**。

---

## 写作规范

### Obsidian 语法

- 内部链接 `[[wiki-links]]`，图片 `![[filename.png]]`
- 流程图用 Mermaid，或用 SVG 存到 `图片/SVG/`，笔记里 `![[文件名.svg|760]]` 引用
- SVG 命名跟随所在章节编号（如 `8_0_1_1.svg`）或用描述性中文名（如 `全路径-read未命中泳道.svg`）

### 链接规范

**该链**：概念首次引用别处定义的术语；问题笔记引向解决方案笔记（动态内存风险 → 智能指针）；基础笔记引向深化笔记（引用 ↔ 左值引用）。

**禁止**：仅因主题相近就加链接；在笔记末尾堆砌"相关笔记"；为建链接而创建空笔记；链接不存在的笔记（建之前先确认目标文件在 vault 里）。

### 内容规范

只补真正提高正确性或学习价值的内容：补缺失的标准术语定义、笔记过简时补最小推导、能说清问题时补小段代码示例。

**不要编造**数值参数或平台相关断言，除非确属标准行为。拿不准的标 TODO。

**代码必须配讲解。** 这是本 vault 最重要的一条。禁止大段代码不加说明，每个代码块要说清「为什么这样写」而不只是「代码在做什么」。超过 30 行的代码拆成逻辑块逐段讲。

---

## C++ 代码风格

严格 Modern C++，禁止 "C with Classes"。RAII 优先，用智能指针；优先 `std::` 算法和 lambda；并发遵循 muduo 的 Reactor 模式。

| 禁止 | 用 |
|---|---|
| `new` / `delete` | `std::make_unique` / `make_shared` |
| `NULL` | `nullptr` |
| C 风格数组 | `std::array` / `std::vector` |
| C 风格强制转换 | `static_cast` / `dynamic_cast` |
| 裸指针表达所有权 | 智能指针 |

### 参考书

`图片/书籍/` 下有四本 PDF，需要引用原文时读它们：

| 文件 | 用途 |
|---|---|
| `Effective C++.pdf` | 经典 C++ 准则 |
| `Effective Modern C++.pdf` | Modern C++ 最佳实践 |
| `Computer Networking A Top-Down Approach.pdf` | 网络理论 |
| `Modern Operating Systems.pdf` | 操作系统 |

《Effective STL》和《Linux 多线程服务端编程》vault 里没有 PDF，引用时凭知识写并注明书名条款，不要假装读过文件。

---

## Skills

Skills 位于 **`~/.claude/skills/`**（用户级，全局生效），不在本仓库的 `.claude/` 下。本仓库的 `.claude/` 只有 `settings.local.json` 和 worktrees。

与本 vault 相关的：

| Skill | 触发场景 | 功能 |
|---|---|---|
| `note-locator` | "找一下…"、"搜索…"、"…在哪" | 模糊搜索笔记 + 解析 wiki-link 关联 |
| `note-creator` | "写一个…"、"创建…"、"出一套题"、"写个题解" | 新建笔记，覆盖 C++/LLM/投资/力扣题解/章节题目汇总 |
| `note-extender` | "检查笔记"、"补充一下"、"查漏补缺" | 对比权威书籍审查并扩展已有笔记 |
| `obsidian-svg` | "画图"、"SVG"、"架构图"、"内存布局" | 建图 + 优化 + 验证 + vault 资产管理 |
| `obsidian-markdown` | wikilink、callout、frontmatter | Obsidian 方言语法 |
| `obsidian-bases` | `.base`、表格视图、公式 | 笔记的数据库视图 |
| `numerical-analysis` | "数值分析"、"考点"、"考不考"、"出题" | 讲考点/润色笔记/仿期末出题（内嵌 2020–2024 考频表） |
| `github-note-push` | 推笔记到 GitHub | 处理 remote、worktree、SSH key 等坑 |

完整清单和精简记录见 [[skills参考目录]]。

---

## 已知的环境事实

- **远控系统源码不在本机。** `6.项目/03.远控系统/` 和 `06.远控服务端/` 的笔记还在，但对应的 git 仓库（原路径 `D:\c++\project\remote_ctl\`）在这台 Mac 上不存在。需要看源码时先问用户要路径，不要去猜或去找。
- vault 根目录：`~/Documents/C++/obsidian_notes-main`

---

## 收尾检查

每篇笔记写完确认：

- [ ] 标题层级和空行一致
- [ ] 表格可读
- [ ] 图片已嵌入并有说明
- [ ] 代码都有配套讲解
- [ ] wiki-link 指向的笔记确实存在
- [ ] 关键要点简明
