---
tags:
  - 项目/远控系统
  - debug
git: "8d2e3c3"
---

# Debug-019 PostThreadMessage 失败路径未释放 PACKET_DATA

**严重程度**：🟡 内存泄漏（发包失败时）
**分类**：资源管理 / 内联 new 写法
**发现时间**：2026-03-25

---

## 现象

正常发包时不可见。当 `PostThreadMessage` 返回 `false`（目标线程队列满、线程已退出等情况下），堆上分配的 `PACKET_DATA` 对象泄漏。

---

## 根因

```cpp
// ❌ 旧版本 CClientSocket::SendPacket
bool ret = PostThreadMessage(
    m_nThreadID,
    WM_SEND_PACK,
    (WPARAM)new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam),  // 内联 new
    (LPARAM)hWnd
);
// PostThreadMessage 失败时，PACKET_DATA 永远泄漏
// 因为没有任何变量引用这个指针，根本无法 delete
```

`new PACKET_DATA(...)` 嵌在参数里，没有赋值给变量。函数成功时，所有权转移给网络线程（由 `SendPack` 负责 `delete`）。函数失败时，没有接收方，也没有变量名，无法清理。

---

## 修复

```cpp
// ✅ 新版本
PACKET_DATA* pData = new PACKET_DATA(strOut.c_str(), strOut.size(), nMode, wParam);
bool ret = PostThreadMessage(m_nThreadID, WM_SEND_PACK, (WPARAM)pData, (LPARAM)hWnd);
if (ret == false)
{
    delete pData;   // 失败时自己清理
}
```

---

## 教训

**不要把 `new` 内联在函数参数里。**

```cpp
// ❌ 危险写法：失败时无法清理
func((WPARAM)new Foo(...));

// ✅ 安全写法：先保存，再处理失败路径
Foo* pFoo = new Foo(...);
bool ok = func((WPARAM)pFoo);
if (!ok) delete pFoo;
```

只要 `new` 出来的指针可能经过某个"可能失败的操作"再转移所有权，就必须先保存到变量。

---

## 关联

- [[6.10 远程显示修复：ACK 陷阱复现与帧率控制]]
- [[Bug目录/6.10-Bug-02 PostThreadMessage失败路径内存泄漏]]
- [[Debug-017 WM_SEND_PACK_ACK回调未释放CPacket导致内存泄漏]] — 同类问题：堆对象所有权未明确
