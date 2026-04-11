---
tags:
  - 项目/远控系统
---

# Debug-010：锁机线程传 NULL 参数

> **日期**：2026-02-10
> **影响范围**：Cmd 7（锁机功能）
> **严重程度**：🔴 未定义行为（可能崩溃或锁机功能异常）
> **状态**：✅ 已修复
> **引入版本**：commit `d27c8a4`（5.2 CCommand 类提取）
> **修复版本**：commit `174aee7`

---

## 现象

锁机功能在某些情况下可能崩溃，或锁机对话框无法正常显示。具体表现取决于编译器和运行时环境的处理方式。

---

## 调试过程

### 第一步：代码审查发现问题

在审查 5.2 重构后的 `LockMachine` 代码时，发现 `_beginthreadex` 传参错误：

```cpp
// Command.h - LockMachine
_beginthreadex(NULL, 0, &CCommand::threadLockDlg, NULL, 0, &threadid);
//                                                 ^^^^ 应该传 this
```

### 第二步：追踪线程函数的参数使用

```cpp
static unsigned __stdcall threadLockDlg(void* arg)
{
    CCommand* thiz = (CCommand*)arg;   // arg 是 NULL → thiz 是空指针！
    thiz->threadLockDlgMain();         // 通过空指针调用成员函数 → UB
    _endthreadex(0);
    return 0;
}
```

`threadLockDlgMain()` 内部访问成员变量 `dlg`（CLockInfoDialog 对象），通过 NULL 指针访问成员变量是**未定义行为**。

### 第三步：分析为什么有时能"正常"工作

在 Debug 模式下，`this` 为 NULL 时访问成员变量，实际访问的地址是成员变量的偏移量（如 `0x00000000 + offset`）。如果恰好该地址可访问（页面未保护），程序可能不会立即崩溃，但行为完全不可预测：

```
thiz = NULL (0x0000000000000000)
thiz->dlg 的地址 = 0x0000000000000000 + dlg 的偏移量
              ↑ 如果偏移量小，可能落在 NULL 页保护区 → 崩溃
              ↑ 如果偏移量大或页面映射特殊 → 读写垃圾数据
```

这就是为什么 Bug 有时表现为崩溃，有时表现为锁机功能异常。

---

## 根因

5.2 重构时将 `threadLockDlg` 从全局函数变为 `CCommand` 的 static 成员函数，需要通过 `void* arg` 传入对象指针以访问成员变量（`dlg`、`threadid` 等）。但 `_beginthreadex` 的第 4 个参数写成了 `NULL`，导致线程内无法获取有效的 `CCommand` 对象指针。

---

## 修复

```cpp
// [原代码] _beginthreadex(NULL, 0, &CCommand::threadLockDlg, NULL, 0, &threadid);
// [问题] 传 NULL 导致线程函数内 (CCommand*)arg 为空指针，访问成员变量是 UB
// [修复] 传 this，让线程函数能正确访问 CCommand 成员
_beginthreadex(NULL, 0, &CCommand::threadLockDlg, this, 0, &threadid);
```

---

## 调试经验

| 经验 | 说明 |
|------|------|
| **static 回调 + void* 模式必检参数** | 每次用 `_beginthreadex`/`CreateThread` 传 `void*` 参数时，检查是否传了正确的对象指针 |
| **NULL 指针调用成员函数不一定崩溃** | 如果成员函数不访问成员变量，通过 NULL 调用可能"正常"运行；一旦访问成员变量就是 UB |
| **重构后的回归检查** | 将全局函数变为 static 成员函数时，检查所有 `void* arg` 传参点 |

---

## 关联笔记

- [[5.5 Bug 修复与冗余代码清理]] — Bug 修复的主笔记
- [[5.2 代码重构：命令类与工具类的提取]] — Bug 引入的版本（5.2 易错点第 3 条已标注此问题）
- [[3.1 锁机处理]] — threadLockDlg 锁机线程的详细实现
