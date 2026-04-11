---
tags:
  - 项目/远控系统
---

# Debug-023 recv 失败时 ACK 传 NULL 导致下载状态机无法识别错误命令

> 归属提交：`71203ae`（2026-03-27）  
> 关联笔记：[[6.12 文件树展示与下载缓冲区双 Bug 修复]]

---

## 现象

文件下载途中如果网络断开（`recv` 返回 ≤0 且 `index == 0`），界面**没有任何提示、状态栏不收尾、进度条卡住**。下次再触发下载时，`length / index` 静态变量还保留着上次的值，行为不可预测。

---

## 根本原因

```cpp
// ❌ 旧代码：recv 失败时，ACK wParam 传 NULL
else
{
    CloseSocket();
    ::SendMessage(hWnd, WM_SEND_PACK_ACK, NULL, 1);  // wParam = NULL
}
```

`OnSendPackAck` 一进来就检查 `pPacket != NULL`：

```cpp
CPacket* pPacket = (CPacket*)wParam;
if (pPacket != NULL)          // ← NULL 直接跳过！
{
    CPacket head = *pPacket;
    delete pPacket;
    switch (head.sCmd)
    {
    case 4:  // 下载处理，length/index 重置逻辑在这里
        ...
    }
}
```

`wParam == NULL` → 整个 switch 跳过 → case 4 的 `length` / `index` 静态变量**永远不被重置**。

---

## 修复方案

在调用 `SendPack` 之初，把发出去的原始命令包缓存起来；`recv` 失败时，用原始命令码构造一个空载荷 `CPacket` 发给 ACK 回调：

```cpp
// ✅ 新代码：提前保存原始命令
void CClientSocket::SendPack(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
    PACKET_DATA data = *(PACKET_DATA*)wParam;
    delete (PACKET_DATA*)wParam;
    HWND hWnd = (HWND)lParam;

    // 保存原始命令，供失败路径使用
    size_t nTemp = data.strData.size();
    CPacket current((BYTE*)data.strData.c_str(), nTemp);   // ← 新增

    if (InitSocket() == true)
    {
        // ... recv 循环 ...
        else
        {
            TRACE("recv failed length %d index %d cmd %d\r\n", length, index, current.sCmd);
            CloseSocket();
            // 构造空载荷 CPacket，保留命令号，让 ACK 处理器能进入正确的 case
            ::SendMessage(hWnd, WM_SEND_PACK_ACK,
                          (WPARAM)new CPacket(current.sCmd, NULL, 0), 1);  // ← 修复
        }
    }
}
```

> 📁 `RemoteClient/CClientSocket.cpp` : `SendPack`（行 290-328）

---

## 关键设计：`CPacket(sCmd, NULL, 0)` 的语义

`CPacket(sCmd, data, len)` 构造一个命令号为 `sCmd`、数据为空的包。  
在 `OnSendPackAck` 的 case 4 里：

```
length != 0（已收到文件大小包）
index < length（下载未完成）
→ else 分支：fwrite(空数据，0字节），index 不变
→ if (index >= length)：假，不触发收尾
```

目前这条路径**仍然不能完整重置状态**（`length / index` 需要靠完成包来清零），但它至少：

1. 不再丢弃错误信号（`pPacket != NULL` 可以进入分支）
2. 加了 TRACE 方便后续继续调试

> [!warning] 根本修复仍需在 `lParam == 1`（错误）路径上显式重置 `length = 0, index = 0`，目前是 TODO。

---

## 经验总结

> **谁 new，谁负责失败路径；通过消息传递所有权时，失败路径也必须是一个合法对象，不能是 NULL。**

`wParam` 传递 `CPacket*` 是项目里的"所有权转移约定"——接收方负责 `delete`，发送方在失败时也必须保证 `wParam` 能被安全地类型检查（`!= NULL`）并进入对应的 case。传 `NULL` 打破了这个约定，让所有依赖 `sCmd` 分发的逻辑统统失效。

---

## 更新记录

| 日期 | 变更 |
|------|------|
| 2026-03-27 | 初始版本 |
