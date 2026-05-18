---
tags:
  - 项目/远控系统
---

# Debug-005：服务端内存泄漏与客户端无法重连

> **日期**：2026-01-16
> **影响范围**：网络通信模块
> **严重程度**：🟡 内存泄漏 + 🔴 功能失效
> **状态**：✅ 已修复
> **详细笔记**：[[3.3  网络模块对接与Bug修复]]

---

## 现象

1. 服务端连续处理多次命令后内存占用持续增长
2. 客户端首次连接失败后，后续所有连接尝试都失败

## 调试过程

### Bug A：内存泄漏

1. **任务管理器观察**：服务端内存持续增长
2. **代码审查**：`DealCommand()` 中 `new char[BUFFER_SIZE]` 分配 4MB，但多个 `return` 路径未 `delete[]`

### Bug B：无法重连

1. **日志分析**：第一次 `connect` 失败后，第二次 `connect` 也失败，`WSAGetLastError()` 返回错误
2. **代码审查**：socket 在构造函数中创建，`connect` 失败后 socket 进入错误状态，但 `InitSocket` 没有重建 socket

## 根因

### Bug A
```cpp
char* buffer = new char[BUFFER_SIZE];
if (len <= 0) return -1;       // ← 未 delete[] buffer
// ...
return m_packet.sCmd;           // ← 未 delete[] buffer
```

### Bug B
```cpp
// 构造函数中创建 socket
m_sock = socket(PF_INET, SOCK_STREAM, 0);
// InitSocket 中直接 connect，不重建
int ret = connect(m_sock, ...);  // 失败后 m_sock 不可重用
```

## 修复

- Bug A：所有 `return` 路径前添加 `delete[] buffer`（后续改为 `std::vector<char>` 成员变量彻底解决）
- Bug B：`InitSocket` 开头先 `CloseSocket()` 再 `socket()` 重建

## 调试经验

| 经验 | 说明 |
|------|------|
| **RAII 优于手动管理** | `new/delete` 容易遗漏，用 `vector` 或 `unique_ptr` 自动管理 |
| **socket 状态不可逆** | `connect` 失败后 socket 不可重用，必须关闭重建 |
| **审查所有 return 路径** | 有资源分配的函数，检查每个 return 前是否释放 |
