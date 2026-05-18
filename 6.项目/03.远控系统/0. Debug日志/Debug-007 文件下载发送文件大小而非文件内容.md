---
tags:
  - 项目/远控系统
---

# Debug-007：文件下载发送文件大小而非文件内容

> **日期**：2026-02-10
> **影响范围**：Cmd 4（文件下载）
> **严重程度**：🔴 数据错误（下载文件全部是乱码）
> **状态**：✅ 已修复
> **引入版本**：commit `277a3b6`（5.3 解耦重构）
> **修复版本**：commit `174aee7`

---

## 现象

客户端下载文件后，打开文件发现内容全是乱码——所有数据块都是相同的 8 字节（文件大小值），而非实际的文件内容。

---

## 调试过程

### 第一步：复现问题

选择一个已知内容的文本文件进行下载，保存后用十六进制编辑器查看：

```
预期内容：48 65 6C 6C 6F ...  (Hello...)
实际内容：00 00 00 00 32 00 00 00  (重复的 8 字节)
```

每个 1024 字节的数据块，内容都是相同的 8 字节（`data` 变量的值 = 文件大小），而非 `buffer` 中 `fread` 读到的内容。

### 第二步：对比重构前后代码

这个 Bug 是 5.3 解耦重构时引入的。对比重构前后的 `DownloadFile`：

```cpp
// ===== 重构前（5.2 版本，正确）=====
do {
    rlen = fread(buffer, 1, 1024, pFile);
    CPacket pack(4, (BYTE*)buffer, rlen);       // ← buffer: 文件内容
    CServerSocket::getInstance()->Send(pack);
} while (rlen >= 1024);

// ===== 重构后（5.3 版本，错误）=====
do {
    rlen = fread(buffer, 1, 1024, pFile);
    lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));  // ← &data: 文件大小！
} while (rlen >= 1024);
```

### 第三步：分析错误原因

重构时将 `Send(pack)` 改为 `lstPacket.push_back()`，但在复制粘贴过程中，参数被写成了上面一行（发送文件大小的那个 `CPacket` 的参数）：

```cpp
// 上面一行（发送文件大小，参数是 &data, 8）
lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));       // 发送文件大小

// 循环体内应该是 buffer, rlen，但错误地也写成了 &data, 8
lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));       // ← 错误：复制了上面的参数
```

### 第四步：验证修复

将参数改回 `buffer, rlen`，重新下载文件，内容正确。

同时修复了另一个问题：文件打开失败时的处理逻辑：

```cpp
// 修复前：无论文件是否打开成功，结尾都发一个 data 包
if (pFile != NULL)
{
    // ... 读取和发送文件内容 ...
    fclose(pFile);
}
lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));  // ← 文件成功时也发这个包

// 修复后：只在文件打开失败时发送零长度包
if (pFile != NULL)
{
    // ... 读取和发送文件内容 ...
    fclose(pFile);
}
else
{
    lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));  // ← 只在失败时发
}
```

---

## 根因

**重构时复制粘贴错误**：将 `Send(pack)` 批量改为 `lstPacket.push_back()` 时，循环体内的 `CPacket` 参数被误写为发送文件大小的参数（`(BYTE*)&data, 8`），而非文件内容（`(BYTE*)buffer, rlen`）。

两行代码仅参数不同，复制时容易混淆：

```cpp
lstPacket.push_back(CPacket(4, (BYTE*)&data,   8));      // 文件大小（正确用途）
lstPacket.push_back(CPacket(4, (BYTE*)buffer,   rlen));   // 文件内容（正确用途）
```

---

## 修复

```cpp
do {
    rlen = fread(buffer, 1, 1024, pFile);
    // [原代码] lstPacket.push_back(CPacket(4, (BYTE*)&data, 8));
    // [问题] 读到的是buffer，但是传的是data，所以下载文件的时候会出问题
    // [修复] 改为发送buffer中实际读到的内容
    lstPacket.push_back(CPacket(4, (BYTE*)buffer, rlen));
} while (rlen >= 1024);
```

---

## 调试经验

| 经验 | 说明 |
|------|------|
| **重构后必须回归测试** | 批量替换时即使编译通过，也要逐个功能点验证 |
| **用十六进制查看异常数据** | 发现每个数据块内容相同，迅速定位到是同一个变量被反复发送 |
| **对比重构前后代码** | 逐行对比 `Send(pack)` → `lstPacket.push_back()` 的转换过程 |
| **注意相似参数的区分** | `&data` 和 `buffer` 都是 `BYTE*`，编译器无法区分用途 |

---

## 关联笔记

- [[5.3 解耦命令处理和网络模块]] — Bug 引入的版本
- [[5.5 Bug 修复与冗余代码清理]] — Bug 修复的版本
- [[Debug-006 文件下载只传输1024字节]] — 同一函数的另一个 Bug（循环条件错误）
