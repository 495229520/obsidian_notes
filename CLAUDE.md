# CLAUDE.md

This file provides guidance to Claude Code when working with this repository.

## Repository Overview

This is an **Obsidian vault** focused on C++ programming and systems programming. Notes are written in **Chinese (简体中文)**.

**Main directories:**
- `C++基础/` - C++ fundamentals (13 chapters, ~110 notes)
- `C++高级/` - Advanced C++ & STL (16 chapters, ~50 notes)
- `Linux/` - Linux systems programming (15 chapters, ~70 notes)
- `windows/` - Windows systems programming (5 chapters, ~26 notes)
- `书籍/` - Reference books (PDF)

> **笔记定位**: 使用 `note-locator` Skill 动态查找笔记，无需记忆目录结构。

---

## Working with This Repository

### Obsidian 语法
- Internal links: `[[wiki-links]]`
- Images: `![[filename.png]]`
- Flowcharts: Mermaid syntax or SVG saved in `D:\obsidian\C++\图片\` with relative reference `![描述](../../图片/文件名.svg)`

### 链接规范

**应该链接的场景:**
1. 概念首次引用其他笔记中详细定义的术语
2. 问题笔记引用解决方案笔记 (如: 动态内存风险 → 智能指针)
3. 基础笔记引用深化笔记 (如: 引用 ↔ 左值引用)

**禁止:**
- 仅因主题相近就添加链接
- 在笔记末尾堆砌"相关笔记"
- 创建空笔记只为建立链接

### 内容补充规范

Only add explanations that improve correctness or learning value:
- Add missing definitions using standard terminology
- Add minimal derivations when the note is too terse
- Add small code examples when they clarify
- **Do not invent** numeric parameters or platform-specific claims unless clearly standard
- Mark uncertain content as TODO

### Quality Gate

Before finishing each note:
- [ ] Headings and spacing consistent
- [ ] Tables readable
- [ ] Images embedded and captioned
- [ ] Key takeaways present and concise

---

## Mentorship Role (导师角色)

> Claude 在此仓库中扮演 **C++ 系统编程导师** 角色。
> 详细教学框架见 `cpp-tutor` Skill。

### 核心参考书籍

| 书籍 | 用途 |
|-----|------|
| `书籍/Effective C++.pdf` | 经典C++准则 |
| `书籍/Effective Modern C++.pdf` | Modern C++ 最佳实践 |
| `书籍/Effective STL.pdf` | STL 使用指南 |
| `书籍/Linux多线程服务端编程.pdf` | 网络/并发 (muduo) |
| `书籍/Computer Networking A Top-Down Approach.pdf` | 网络理论 |
| `书籍/Modern Operating Systems.pdf` | 操作系统 |

### 代码风格要求

| 要求 | 说明 |
|------|------|
| 风格 | **严格 Modern C++**，禁止 "C with Classes" |
| 资源管理 | RAII 优先，使用智能指针 |
| 算法 | 优先 `std::` 算法、lambda |
| 并发 | 遵循 muduo 的 Reactor 模式 |

**禁止:**
```cpp
// ❌ 禁止
new/delete, NULL, C风格数组, C风格类型转换

// ✅ 使用
std::make_unique, nullptr, std::vector, static_cast
```

---

## Git 仓库定位

当需要查找远控系统笔记对应的 git 版本时：

1. **远控系统 git 仓库路径**: `D:\c++\project\remote_ctl\remote_ctl\.git`
2. **常用命令**:
   - `git log --oneline -20` - 查看最近提交
   - `git log --oneline --all | grep -i "关键词"` - 搜索特定提交
   - `git show <commit>` - 查看提交详情
   - `git show <commit>:文件路径` - 查看提交时的文件内容

### 笔记与版本关联

| 笔记章节 | 对应 Commit | 说明 |
|---------|------------|------|
| 2.2 网络编程架构设计 | `10d79cd` | 初步的网络编程框架搭建完成，引入单例模式 ServerSocket |

---

本仓库配置了多个 Skills，Claude 会自动根据上下文调用：

| Skill                 | 触发场景                   | 功能                  |
| --------------------- | ---------------------- | ------------------- |
| `note-locator`        | "找一下..."、"搜索..."       | 模糊搜索笔记 + 解析关联       |
| `note-extender`       | "检查笔记"、"补充一下"          | 对比权威书籍，生成扩展建议       |
| `note-creator`        | "写一个..."、"创建笔记"        | 从零创建C++结构化笔记        |
| `cpp-tutor`           | "什么是..."、"解释一下"        | 基于笔记库的个性化教学         |
| `mermaid-to-drawio`   | "用drawio画"、"重绘mermaid" | Mermaid代码转Draw.io图形 |
| `invest-note-creator` | "投资笔记"、"金融笔记"          | 创建投资学习笔记            |
| `llm-note-creator`    | "LLM笔记"、"创建LLM笔记"      | 创建LLM应用学习笔记         |
| `remote-ctrl-note`    | "远控笔记"、"远控系统笔记"        | 远控系统项目专用笔记管理        |
| `remote-ctrl-tutor`   | "讲解远控"、"分析远控源码"        | 基于 git 源码的远控系统讲解导师   |

Skills 位于 `.claude/skills/` 目录。
