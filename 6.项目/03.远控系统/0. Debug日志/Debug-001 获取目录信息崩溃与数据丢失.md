---
tags:
  - 项目/远控系统
---

# Debug-001：获取目录信息崩溃与数据丢失

> **日期**：2026-02-10
> **影响范围**：Cmd 2（获取目录列表）
> **严重程度**：🔴 崩溃 + 数据丢失
> **状态**：✅ 已修复

---

## 现象描述

1. 客户端成功连接被控端，测试连接正常（`ack:1981`）
2. 点击"查看文件信息"可以正常看到 C 盘和 D 盘（`ack:1`）
3. **点击 C 盘时**：
   - 客户端收到 `ack:-1`，`Count = 0`，无法显示任何目录内容
   - 被控端进程直接崩溃中断

---

## 调试过程

### 第一步：分析日志定位方向

从客户端输出日志中提取关键信息：

```
ack:1981   ← 第1次连接：测试连接，成功
ack:1      ← 第2次连接：获取磁盘(Cmd 1)，成功
ack:-1     ← 第3次连接：获取目录(Cmd 2)，失败！
Count = 0  ← 没收到任何文件信息
```

`ack:-1` 说明 `DealCommand()` 返回了 -1，结合"被控端进程中断"，判断**服务端崩溃**导致连接断开。

### 第二步：确认客户端发送的路径

在客户端 `LoadFileInfo` 中 `CString strPath = GetPath(hTreeSelected);` 处打断点：

```
strPath = "C:\\"    ← 路径正确，排除客户端路径问题
```

### 第三步：定位服务端崩溃点

由于服务端运行在虚拟机上只有 exe，无法直接打断点。解决方法：

> **在本机用 VS 调试服务端，客户端双击 exe 运行，IP 填 127.0.0.1**

在服务端 `MakeDirectoryInfo` 打断点，触发崩溃后 VS 报告：

```
0x00007FFCFFE454A6 (ntdll.dll) 处引发的异常:
0xC0000005: 写入位置 0xFFFFFFFFEFC4DE40 时发生访问冲突
```

崩溃位置：`_findnext(hfind, &fdata)` 调用处。

### 第四步：确认根因

```cpp
// Command.h - MakeDirectoryInfo
int hfind = 0;                              // ← int 是 32 位
if ((hfind = _findfirst("*", &fdata)) == -1) // _findfirst 返回 intptr_t (64位)
```

x64 下 `_findfirst` 返回 `intptr_t`（64 位），存入 `int`（32 位）导致**句柄截断**。截断后的无效句柄传给 `_findnext` 引发访问冲突。

地址 `0xFFFFFFFFEFC4DE40` 高 32 位全是 `F`，是典型的 32 位有符号数被符号扩展到 64 位的特征。

---

## Bug 1：`_findfirst` 句柄截断（服务端崩溃）

### 问题代码

```cpp
// Command.h - MakeDirectoryInfo
int hfind = 0;
if ((hfind = _findfirst("*", &fdata)) == -1)
```

### 修复

```cpp
// [原代码] int hfind = 0;
// [问题] 64位系统 _findfirst 返回 intptr_t(64位)，用 int(32位) 存储会截断句柄值
// [新代码] 使用 intptr_t 确保在 64 位系统正确存储句柄
intptr_t hfind = 0;
// [新代码结束]
```

---

## Bug 2：客户端缓冲区数据丢失（只显示部分目录）

修复 Bug 1 后，服务端不再崩溃，成功遍历 27 个文件并发送。但客户端只显示了一部分目录内容。

### 问题分析

客户端 `CClientSocket::DealCommand()` 的逻辑：

```cpp
static size_t index = 0;
while(TRUE)
{
    int ilen = recv(m_sock, buffer + index, ...);  // ← 每次都先 recv
    if (ilen == 0) return -1;                       // 对端关闭就返回 -1
    // ... 解析一个包后 return
}
```

**问题**：服务端一次性发送 27 个包后 `CloseClient()` 关闭连接。客户端第一次 `recv` 可能一次收到所有数据，但只解析一个包就返回。第二次进入 `DealCommand()` 时又先调用 `recv`，此时服务端已关闭连接，`recv` 返回 0，直接 `return -1`。**缓冲区里还有 26 个包的数据被丢弃了。**

### 修复

在 `recv` 之前先检查缓冲区中是否已有完整的包：

```cpp
while(TRUE)
{
    // [新代码] 先检查缓冲区中是否已有完整的包
    if (index > 0)
    {
        size_t len = index;
        m_packet = CPacket((BYTE*)buffer, len);
        if (len > 0)
        {
            memmove(buffer, buffer + len, index - len);
            index -= len;
            return m_packet.sCmd;
        }
    }
    // [新代码结束]

    if (index >= BUFFER_SIZE)
        return -1;
    int ilen = recv(m_sock, buffer + index, (int)(BUFFER_SIZE - index), 0);
    // ... 后续不变
}
```

---

## 调试经验总结

| 经验 | 说明 |
|------|------|
| **从日志定位方向** | `ack:-1` + 进程中断 → 服务端崩溃，而非客户端问题 |
| **跨进程调试** | 服务端在虚拟机无法调试时，在本机用 127.0.0.1 同时运行两端 |
| **64 位兼容性** | x64 下所有句柄/指针类型必须用 `intptr_t` 或对应的 64 位类型 |
| **TCP 粘包意识** | 服务端发完即关闭连接，客户端必须先消费缓冲区再 recv |
| **分层排查** | 先排除客户端（路径正确）→ 再定位服务端（崩溃点）→ 再查客户端接收 |

---

## 关联笔记

- [[2.5 获取指定文件目录下的文件和文件夹]] — `_findfirst`/`_findnext` 的使用
- [[2.3 设计网络传输包协议]] — CPacket 协议与粘包处理
- [[3.3  网络模块对接与Bug修复]] — 之前的网络 Bug 修复经验
