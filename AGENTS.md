# Repository Guidelines

## Project Structure & Module Organization
- Markdown notes are grouped by topic: `C++基础` covers fundamentals; `C++高级` holds advanced language patterns; `Linux` and `windows` focus on system and network programming; `ai 编程` stores agent and markdown tips.
- Assets live in `图片/`; quick captures stay in `剪切板/`; keep markdown under the topic folders with their numeric prefixes.
- Preserve the numbered naming scheme (e.g., `6. 指针/6.9 总结.md`) so Obsidian sorts sections correctly.

## Build, Test, and Development Commands
- No build pipeline is required; edit markdown directly.
- Survey sections quickly with `Get-ChildItem -Depth 2` from repo root.
- Search notes fast with `rg "关键字" C++基础` (adjust folder as needed).
- For new runnable snippets, sanity-check with `g++ -std=c++17 example.cpp && ./a.exe` on Windows shells.

## Coding Style & Naming Conventions
- Use Markdown ATX headings, `-` bullet lists, and fenced code blocks with languages (` ```cpp ` for C++).
- Keep filenames numbered and titled in Chinese to match the existing outline; avoid renaming unless reorganizing the syllabus.
- Store images in `图片/` and reference them with relative paths; avoid embedding large binaries in note folders.
- Keep code samples minimal, 4-space indented inside fences, and include expected output when helpful.

## SVG Workflow
- When a note contains raw `<svg>...</svg>` markup, export each SVG into `图片/SVG/` and replace the inline block with an Obsidian embed.
- Every exported SVG must include a solid background rectangle with `fill="rgb(245, 244, 237)"`.
- Prefer the local Skill at `.codex/skills/obsidian-svg-bg/SKILL.md` and its script for this workflow.

## Testing Guidelines
- No automated test suite; proofread for accuracy, broken links, and consistent cross-references.
- If adding runnable code, compile locally (command above) and note expected behavior or output in the text.

## Commit & Pull Request Guidelines
- Use short, imperative commits summarizing the topic touched (e.g., "Add epoll note", "Update pointer safety tips"); group related edits together.
- PRs should describe scope, list folders/files touched, link related issues or tasks, and include before/after screenshots when changing diagrams or images.
- Review focus: technical accuracy, coherence with adjacent sections, and adherence to numbering/placement.

## Security & Configuration Tips
- Do not commit personal Obsidian settings in `.obsidian/` unless intentionally shared; keep workspace-specific files local.
- Never include real credentials or hostnames in examples—use placeholders and redact machine-specific paths.

## Project Memory (项目记忆)

> 以下信息由Claude分析生成，用于未来的笔记分析和链接工作。

### 统计概览

| 指标 | 数值 |
|------|------|
| 总笔记文件数 | ~290 |
| C++基础笔记数 | ~110 |
| C++高级笔记数 | ~50 |
| Linux笔记数 | ~70 |
| Windows笔记数 | ~26 |
| AI编程笔记数 | ~3 |
| Wiki-links总数 | 700+ |
| 平均每文件wiki-links数 | ~3.5 |

### 详细目录结构

```
C++基础/ (13章, ~110笔记)
├── 1. 计算机基础/           [3 notes] - 编译链接、栈帧、虚拟地址空间
├── 2. 基本数据类型/         [4 notes] - 类型、格式化、命名空间、枚举
├── 3. 运算符/               [4 notes] - 关系、三元、位运算、判断
├── 4. 数组/                 [2 notes] - 基本数组、vector
├── 5. 循环/                 [2 notes] - for、while
├── 6. 指针/                 [13 notes] - 指针、动态内存、引用、智能指针
│   └── 智能指针/            [3 notes] - unique_ptr, shared_ptr, weak_ptr
├── 7. 结构体 & 联合体/      [2 notes]
├── 8. 字符串/               [2 notes] - string、字符处理
├── 9. 函数/                 [28 notes] - 三大子系统
│   ├── 函数模版/            [11 notes] - 9.1.1-9.1.11
│   ├── 基础函数/            [7 notes] - 9.2.1-9.2.7
│   └── 运算符重载/          [10 notes] - 9.3.1-9.3.10
├── 10. C&C++/               [3 notes] - C/C++联调、const区别
├── 11. 编译器/              [8 notes] - ODR、inline、预处理、宏
├── 12. OOP/                 [28 notes] - 三大特性
│   ├── 12.1 封装/           [14 notes] - 类、构造析构、静态成员、友元
│   ├── 12.2 继承/           [4 notes] - 继承、构造析构、多重继承
│   └── 12.3 多态/           [7 notes] - 虚函数、抽象类、虚基类
└── 13. 左值与右值/          [4 notes] - 左值、右值引用、移动语义

C++高级/ (16章, ~50笔记)
├── 1-5. 基础深化            - 压栈、inline、混合编程、const、new/delete
├── 6. OOP/                  [9 notes] - 深度OOP、面试题
├── 7-9. 构造析构深化        - 拷贝、类成员指针
├── 10. 模版/                [2 notes] - 函数模版、类模版
├── 11. 运算符重载/          [1 note]
├── 12. 迭代器/              [3 notes] - 基础、vector实现、失效问题
├── 13. 类型转换方式/        [1 note]
├── 14. STL/                 [13 notes] - vector, queue, list, set, map, string, 算法
├── 15. volatile/            [1 note]
└── 16. C++11/               [11 notes] - auto, thread, atomic, lambda, 移动语义, chrono

Linux/ (15章, ~70笔记)
├── 1. Linux系统命令/        [2 notes] - ls、echo、cd、cp、rm、chmod、vim、gcc
├── 2. Linux基本操作/        [2 notes] - 前期准备、环境变量
├── 3. Linux基础函数/        [5 notes] - 字符串、数据转换、格式化IO、权限控制
├── 4. Linux IO函数/         [3 notes] - 打开读写关闭、重定向同步、文件锁
├── 5. 系统进程控制函数/     [5 notes] - 进程控制、结束进程、非局部跳转、等待函数
├── 6. 文件与目录函数/       [9 notes] - 基本文件、高级操作、读写、目录操作
├── 7. 网络编程基础/         [7 notes] - IP、端口、协议、TCP基础、套接字、listen/accept
├── 8. TCP编程/              [8 notes] - TCP原理、服务端、客户端、迭代服务器、回声、IO缓冲
├── 9. UDP编程/              [3 notes] - UDP原理、服务端、客户端
├── 10. 套接字/              [3 notes] - 套接字选项、TIME_WAIT、Nagle算法
├── 11. 进程/                [8 notes] - 进程简介、IPC、管道、共享内存、信号量、消息队列、僵尸进程
├── 12. 线程/                [6 notes] - 线程简介、创建运行、互斥量、信号量、销毁、并发服务器
├── 13. IO复用/              [4 notes] - IO复用简介、select、poll、epoll
├── 14. Makefile/            [4 notes] - Makefile基础进阶、CMake基础进阶
└── 15. 安装V2ray.md         [1 note]

windows/ (5章, ~26笔记)
├── 1. 具体案例/             [1 note] - 窗口案例
├── 2. 网络编程/             [5 notes] - 网络编程基础、常见函数、TCP、UDP、隐藏进程
├── 3. 多线程/               [11 notes] - 线程简介、同步、互斥量、事件对象、信号量、临界区、死锁
├── 4. 进程/                 [6 notes] - 进程简介、剪切板、油槽、匿名/命名管道、VM_COPYDATA
└── 5. 文件/                 [3 notes] - 文件简介、配置文件和注册表、动态链接库

ai 编程/ (2章, ~3笔记)
├── 1. claude/               [2 notes] - claude code语法、skills
└── 2. GPT/                  [1 note] - codex语法
```

### 核心中枢笔记 (高链接度)

| 笔记 | 链接数 | 角色 |
|------|--------|------|
| 6.9 指针总结 | 13+ | 指针知识汇总中枢 |
| 6.3 动态内存分配的风险 | 8+ | 内存管理问题中枢 |
| 12.3.1 静态多态与动态多态 | 7+ | OOP多态分类中枢 |
| 11.2 编译器基本概念 | 7+ | 编译概念中枢 |
| 9.3.10 实现加密hint | 8 | 运算符重载综合应用 |
| 12.3.7 从内存角度理解 | 2+ | 对象模型深度分析 |

### 知识依赖图谱

```
层级1: 基础 (必修)
┌─────────────────────────────────────┐
│ 1.1 程序编译与链接原理               │ ← 最基础
│ 1.2 函数栈帧建立与销毁               │
│ 1.3 进程的虚拟地址空间               │
└─────────────────┬───────────────────┘
                  ↓
层级2: 指针与内存 (关键)
┌─────────────────────────────────────┐
│ 6.1 基本指针 → 6.2 动态内存         │
│ 6.4 引用 → 6.6 堆栈区别             │
│ 智能指针 (依赖6.2-6.3)              │
└─────────────────┬───────────────────┘
                  ↓
层级3: 函数与模板 (中间层)
┌─────────────────────────────────────┐
│ 9.1.* 函数基础 → 9.2.* 函数模板     │
│ 9.3.* 运算符重载                    │
│ 11.* 编译器概念 (ODR, inline)       │
└─────────────────┬───────────────────┘
                  ↓
层级4: OOP (高级)
┌─────────────────────────────────────┐
│ 12.1 封装 → 12.2 继承 → 12.3 多态   │
│ 12.3.2 虚函数 ← 核心                │
│ 12.3.7 从内存角度理解 ← 深度        │
└─────────────────┬───────────────────┘
                  ↓
层级5: 现代C++ (进阶)
┌─────────────────────────────────────┐
│ 13.* 左值与右值 (配合12.3)          │
│ 16.* C++11特性 (依赖13.2)           │
│ 14.* STL + 12.迭代器 (实战)         │
└─────────────────────────────────────┘
```

### 跨章节关联映射

| 源概念 | 目标概念 | 关联类型 |
|--------|----------|----------|
| 1.2 函数栈帧 | 9.1.6 栈溢出攻击 | 应用实例 |
| 6.2 动态内存 | 智能指针/* | 问题→解决方案 |
| 6.4 引用 | 13.3 左值引用 | 基础→深化 |
| 9.2.2 函数模板 | 11.1 ODR | 概念依赖 |
| 11.2 编译器概念 | 10.* C/C++联调 | 名称修饰关联 |
| 12.2.4 多重继承 | 12.3.6 虚基类 | 问题→解决方案 |
| 12.3.2 虚函数 | 12.3.7 内存角度 | 概念→实现 |
| 13.2 右值引用 | 16.8 移动语义 | 基础→高级 |
| C++高级/16.2 thread | Linux/12.线程 | C++标准库→系统API |
| C++高级/16.3 atomic | Linux/12.3 互斥量 | 原子操作→同步原语 |
| Linux/11.进程 | windows/4.进程 | 跨平台对比 |
| Linux/12.线程 | windows/3.多线程 | 跨平台对比 |
| Linux/8.TCP编程 | windows/2.网络编程 | 跨平台对比 |
| Linux/13.IO复用 | windows/2.网络编程 | select/epoll vs IOCP |
| Linux/11.5 信号量 | windows/3.7 信号量 | 跨平台对比 |

### 推荐学习路径

**入门路径 (新手)**
```
1.计算机基础 → 2-3.类型运算符 → 4-5.数组循环 → 6.指针(6.1-6.4) → 8.字符串
```

**中级路径 (函数与模板)**
```
9.函数(9.1全, 9.2全) → 11.编译器 → 10.C/C++互操作(可选)
```

**高级路径 (OOP)**
```
12.1封装 → 12.2继承 → 12.3多态 → 13.左值右值
```

**精通路径 (现代C++)**
```
完成基础+高级 → C++高级/6.OOP深化 → 14.STL → 16.C++11
```

**Linux系统编程路径**
```
Linux/1-6.系统基础 → 7.网络基础 → 8.TCP编程 → 9.UDP编程
→ 10.套接字 → 11.进程 → 12.线程 → 13.IO复用 → 14.Makefile
```

**Windows系统编程路径**
```
windows/1.案例入门 → 2.网络编程 → 3.多线程 → 4.进程通信 → 5.文件操作
```

**跨平台对比路径 (推荐有Linux基础后)**
```
Linux/11.进程 ↔ windows/4.进程
Linux/12.线程 ↔ windows/3.多线程
Linux/8.TCP ↔ windows/2.网络编程
```

### 链接最佳实践

**应该链接的场景:**
1. 概念首次引用其他笔记中详细定义的术语
2. 问题笔记引用解决方案笔记 (如: 动态内存风险 → 智能指针)
3. 基础笔记引用深化笔记 (如: 6.4引用 ↔ 13.3左值引用)
4. 汇编分析笔记引用概念笔记 (如: 9.2.7模板汇编 → 9.2.2函数模板)

**不应该链接的场景:**
1. 仅因为主题相近就添加链接
2. 在笔记末尾堆砌"相关笔记"
3. 创建空笔记只为建立链接
4. 链接到完全不相关的笔记

### 高优先级待链接关系 (建议)

以下是分析发现的潜在链接机会:

| 源笔记 | 建议链接到 | 理由 |
|--------|-----------|------|
| 12.1.8 对象放置构造 | 6.2 动态内存分配 | placement new依赖内存分配理解 |
| 12.1.13 malloc与new | 11.2 编译器基本概念 | 涉及名称修饰 |
| 智能指针/* | 12.1.7 析构函数 | RAII模式关联 |
| 16.8 移动语义 | 12.1.6 构造函数 | 移动构造函数 |
| 6.6 堆与栈的区别 | 1.3 虚拟地址空间 | 内存布局关联 |

## Remote Control Note Skill (remote-ctrl-note)

### Skill 描述
远程控制和管理Obsidian知识库中的笔记，实现远程操作、搜索、编辑和文件管理功能。

### 触发条件
当用户请求涉及以下操作时自动激活：
- 远程操作笔记（创建、删除、重命名）
- 远程搜索内容
- 远程编辑和修改文件
- 远程获取目录和文件信息
- 远程管理文件结构

### 触发词
- 远程、remote
- 远程操作、远程控制
- 远程搜索、远程查找
- 远程编辑、远程修改
- 远程创建、远程删除
- 远程查看、远程读取

### 功能列表
1. **远程文件操作**：创建、删除、重命名、移动笔记文件
2. **远程内容搜索**：在笔记库中搜索特定内容
3. **远程编辑**：修改现有笔记内容
4. **远程信息获取**：查看目录结构、文件属性
5. **远程批量操作**：批量处理多个文件

### 使用示例
- "远程创建笔记"
- "远程搜索虚函数相关内容"
- "远程删除这个文件"
- "远程查看目录结构"
- "远程编辑这个笔记"
