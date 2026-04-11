---
tags:
  - 项目/远控系统
  - debug
git: "71203ae"
---

# Debug-021 下载完成依赖下一次ACK导致最后一包后界面不收尾

> **现象**：下载文件时，最后一块数据已经写进本地文件，但等待光标和“下载完成”提示不一定立刻出现。  
> **根因**：`RemoteClientDlg::OnSendPackAck(case 4)` 把完成条件判断放在了“下一次回调入口”，而不是“本次数据块写完以后”，导致最后一块写完后流程还要再赌一次额外 ACK。

---

## Bug 基本信息

| 项目 | 内容 |
|------|------|
| **编号** | Debug-021 |
| **严重程度** | 🔴 功能失效 |
| **分类** | 边界条件 + 收尾时机 |
| **commit** | `71203ae` |
| **出现场景** | `cmd=4` 文件下载，最后一个数据块写完后 |

---

## 现象描述

1. 控制端发起下载，并成功收到首包里的文件总长度。
2. 后续数据块也持续写入本地文件。
3. 最后一块数据实际上已经写完，但状态对话框、等待光标和完成提示没有立即收口。
4. 下载流程是否结束，错误地依赖“后面还会不会再进一次 `OnSendPackAck(case 4)`”。

---

## 调试过程

### 第一步：先确认不是写文件失败

观察 `fwrite()` 和 `index` 累加日志，可以发现最后一块数据确实已经成功写入：

```cpp
fwrite(head.strData.c_str(), 1, head.strData.size(), pFile);
index += head.strData.size();
TRACE("index = %d\r\n", index);
```

问题不在“最后一块没写进去”，而在“写完以后没人立刻宣布结束”。

### 第二步：对照文件总长度看完成条件

首包已经给出了完整 `length`：

```cpp
length = *(long long*)head.strData.c_str();
```

而最后一块写完后，`index` 往往已经满足：

```text
index >= length
```

说明业务上其实已经完成，但代码没有在这一轮把它收掉。

### 第三步：定位到判断时机错位

旧代码结构是：

```cpp
if (length == 0)
{
    length = *(long long*)head.strData.c_str();
}
else if (length > 0 && (index >= length))
{
    fclose((FILE*)lParam);
    length = 0;
    index = 0;
    CClientController::getInstance()->DownloadEnd();
}
else
{
    FILE* pFile = (FILE*)lParam;
    fwrite(head.strData.c_str(), 1, head.strData.size(), pFile);
    index += head.strData.size();
}
```

它的逻辑问题在于：

- `index >= length` 只在**进入本轮回调之前**检查
- 当前块写完以后，哪怕已经凑满总长度
- 这轮函数也不会再检查一次

于是“完成”被推迟到了下一轮 ACK。

---

## 根因分析

这个 bug 本质上是一个非常典型的“完成条件检查时机错位”：

```text
正确问题：本次写入后，累计字节数是否已经达到总长度？
错误写法：下一次回调开始时，上一轮是不是已经达到总长度？
```

这两者看起来只差一行 `if` 的位置，但行为差很多：

### 旧逻辑

```text
收到最后一块
  -> fwrite
  -> index += 本块大小
  -> 直接 return
  -> 等下一次 ACK 再判断是否完成
```

### 新逻辑

```text
收到最后一块
  -> fwrite
  -> index += 本块大小
  -> 立刻 if (index >= length)
  -> fclose + Reset + DownloadEnd
```

对于分块下载来说，显然后者才是对协议更忠实的写法。

---

## 修复方案

```cpp
FILE* pFile = (FILE*)lParam;
fwrite(head.strData.c_str(), 1, head.strData.size(), pFile);
index += head.strData.size();
TRACE("index = %d\r\n", index);
if (index >= length)
{
    fclose((FILE*)lParam);
    length = 0;
    index = 0;
    CClientController::getInstance()->DownloadEnd();
}
```

### 关键点

- `fwrite()` 之后立刻更新 `index`
- 更新后立刻比较 `index >= length`
- 一旦满足，就在当前回调里完成全部收尾

这样修完之后，下载流程的完成条件就只依赖两件事：

1. 首包给出的总长度 `length`
2. 客户端真实写入的累计长度 `index`

不再依赖额外的空包，也不再依赖“后面刚好还有下一次回调”。

---

## 修复效果

```text
修复前：
  最后一块已写完
  -> 这轮不收尾
  -> 还要等下一次 ACK
  -> UI 收口时机漂移

修复后：
  最后一块已写完
  -> 这一轮立即判断 index >= length
  -> 立刻关闭文件并结束等待状态
```

---

## 调试经验

| 经验 | 说明 |
|------|------|
| **完成条件要看“当前块处理后的状态”** | 分块协议最容易犯的错，就是把收尾判断放到下一轮事件里 |
| **头包既然给了总长度，就应该优先信长度** | 能用协议字段闭环，就不要把业务完成押在额外空包或连接关闭上 |

---

## 关联笔记

- [[6.12 文件树与下载链路收口：异步 ACK 回填与按长度收尾]] - 本次提交的主笔记
- [[Bug目录/6.12-Bug-01 多包响应最后一步没接上：目录树不展开与下载收尾滞后]] - 本章本地 bug 说明
- [[Debug-020 单击目录一次后重复解析同一FILEINFO导致日志刷屏]] - 同一提交里另一处多包收尾问题：缓冲区尾包搬运错误

---

## 更新记录

| 日期 | 变更 |
|------|------|
| 2026-03-27 | 初始版本：基于提交 `71203ae` 记录下载最后一块写完后仍依赖下一次 ACK 才收尾的问题与修复 |
