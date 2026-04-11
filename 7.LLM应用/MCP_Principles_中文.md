# Model Context Protocol (MCP) 深入笔记

> **作者说明：** 这份笔记是站在系统 / 后端工程师的视角来写的，文中会频繁使用 IPC、TCP 和 Unix 进程管理的类比。

---

## 目录

1. 什么是 MCP？
2. MCP 解决了什么问题
3. 协议栈概览
4. 传输层
5. 三个原语
6. 会话生命周期
7. 消息格式（JSON-RPC 2.0）
8. 能力协商
9. `sampling` 特性
10. 安全模型
11. MCP 在 Agent 基础设施栈中的位置
12. 总结速查表

---

## 1. 什么是 MCP？

**Model Context Protocol（MCP）** 是 Anthropic 于 2024 年 11 月发布的一套开放、标准化协议。它定义了 AI 宿主应用（即内部包含 LLM 的应用）如何与外部服务器通信，而这些服务器会向智能体暴露工具、数据源和各种能力。

最容易理解的类比是：**MCP 就是 AI 工具领域的 USB-C。**

在 USB 出现之前，每种外设都可能有自己专属的接口。USB 把物理层和逻辑层接口统一了，于是任意设备都能和任意电脑通信。MCP 对 AI 智能体和外部世界做的事情也是一样的：一套协议，任意工具，任意 AI。

```text
"MCP 给 AI 智能体配上了眼睛（Resources）和手（Tools），
 并通过一个标准端口把它们连了起来。"
```

---

## 2. MCP 解决了什么问题

### 在 MCP 之前：N×M 问题

任何一个想接入外部工具的 AI 应用，都必须针对这个工具单独开发一套**定制集成**。如果有 N 个 AI 应用、M 个工具，那么：

```text
所需集成总数 = N × M
```

<svg viewBox="0 0 600 280" xmlns="http://www.w3.org/2000/svg" width="100%">
  <defs>
    <marker id="arr" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse">
      <path d="M2 1L8 5L2 9" fill="none" stroke="#888780" stroke-width="1.5" stroke-linecap="round"/>
    </marker>
  </defs>
  <rect x="20" y="20" width="110" height="36" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="75" y="43" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#3C3489">AI 应用 1</text>
  <rect x="20" y="72" width="110" height="36" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="75" y="95" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#3C3489">AI 应用 2</text>
  <rect x="20" y="124" width="110" height="36" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="75" y="147" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#3C3489">AI 应用 3</text>
  <text x="75" y="182" text-anchor="middle" font-size="11" font-family="sans-serif" fill="#888780">⋮</text>

  <rect x="470" y="20" width="110" height="36" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.8"/>
  <text x="525" y="43" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#085041">GitHub</text>
  <rect x="470" y="72" width="110" height="36" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.8"/>
  <text x="525" y="95" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#085041">Postgres 数据库</text>
  <rect x="470" y="124" width="110" height="36" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.8"/>
  <text x="525" y="147" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#085041">文件系统</text>
  <text x="525" y="182" text-anchor="middle" font-size="11" font-family="sans-serif" fill="#888780">⋮</text>

  <line x1="132" y1="38" x2="468" y2="38" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>
  <line x1="132" y1="38" x2="468" y2="90" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>
  <line x1="132" y1="38" x2="468" y2="142" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>
  <line x1="132" y1="90" x2="468" y2="38" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>
  <line x1="132" y1="90" x2="468" y2="90" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>
  <line x1="132" y1="90" x2="468" y2="142" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>
  <line x1="132" y1="142" x2="468" y2="38" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>
  <line x1="132" y1="142" x2="468" y2="90" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>
  <line x1="132" y1="142" x2="468" y2="142" stroke="#D85A30" stroke-width="0.8" stroke-dasharray="4,3" marker-end="url(#arr)"/>

  <text x="300" y="220" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#993C1D">N × M 个定制集成：混乱且难维护</text>
</svg>

### 有了 MCP 之后：N+M 方案

每个 AI 应用只需要内置**一个 MCP 客户端**。每个工具只需要提供**一个 MCP 服务器**。大家都说同一种协议。

```text
所需集成总数 = N + M
```

<svg viewBox="0 0 600 240" xmlns="http://www.w3.org/2000/svg" width="100%">
  <defs>
    <marker id="arr2" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse">
      <path d="M2 1L8 5L2 9" fill="none" stroke="#888780" stroke-width="1.5" stroke-linecap="round"/>
    </marker>
  </defs>
  <rect x="20" y="30" width="110" height="32" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="75" y="51" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#3C3489">AI 应用 1</text>
  <rect x="20" y="78" width="110" height="32" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="75" y="99" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#3C3489">AI 应用 2</text>
  <rect x="20" y="126" width="110" height="32" rx="6" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="75" y="147" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#3C3489">AI 应用 3</text>

  <rect x="230" y="60" width="140" height="60" rx="10" fill="#FAEEDA" stroke="#854F0B" stroke-width="1"/>
  <text x="300" y="85" text-anchor="middle" font-size="13" font-weight="500" font-family="sans-serif" fill="#633806">MCP 协议</text>
  <text x="300" y="103" text-anchor="middle" font-size="11" font-family="sans-serif" fill="#854F0B">JSON-RPC 2.0</text>

  <rect x="470" y="30" width="110" height="32" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.8"/>
  <text x="525" y="51" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#085041">GitHub</text>
  <rect x="470" y="78" width="110" height="32" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.8"/>
  <text x="525" y="99" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#085041">Postgres 数据库</text>
  <rect x="470" y="126" width="110" height="32" rx="6" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.8"/>
  <text x="525" y="147" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#085041">文件系统</text>

  <line x1="132" y1="46" x2="228" y2="82" stroke="#7F77DD" stroke-width="1" marker-end="url(#arr2)"/>
  <line x1="132" y1="94" x2="228" y2="90" stroke="#7F77DD" stroke-width="1" marker-end="url(#arr2)"/>
  <line x1="132" y1="142" x2="228" y2="104" stroke="#7F77DD" stroke-width="1" marker-end="url(#arr2)"/>

  <line x1="372" y1="82" x2="468" y2="46" stroke="#1D9E75" stroke-width="1" marker-end="url(#arr2)"/>
  <line x1="372" y1="90" x2="468" y2="94" stroke="#1D9E75" stroke-width="1" marker-end="url(#arr2)"/>
  <line x1="372" y1="104" x2="468" y2="142" stroke="#1D9E75" stroke-width="1" marker-end="url(#arr2)"/>

  <text x="300" y="200" text-anchor="middle" font-size="12" font-family="sans-serif" fill="#3B6D11">N + M 个集成：清晰、可组合</text>
</svg>

---

## 3. 协议栈概览

MCP 位于智能体技术栈中的 **RPC 层**，可以把它类比成微服务世界里的 gRPC。

<svg viewBox="0 0 560 320" xmlns="http://www.w3.org/2000/svg" width="100%">
  <rect x="40" y="260" width="480" height="44" rx="8" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.8"/>
  <text x="280" y="278" text-anchor="middle" font-size="13" font-weight="500" font-family="sans-serif" fill="#085041">AI 基础设施</text>
  <text x="280" y="295" text-anchor="middle" font-size="11" font-family="sans-serif" fill="#0F6E56">GPU · 训练集群 · 模型服务 API · 向量数据库</text>

  <rect x="40" y="200" width="480" height="50" rx="8" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="280" y="220" text-anchor="middle" font-size="13" font-weight="500" font-family="sans-serif" fill="#3C3489">AI 智能体</text>
  <text x="280" y="238" text-anchor="middle" font-size="11" font-family="sans-serif" fill="#534AB7">LLM 推理 · 记忆 · 规划 · 工具使用决策</text>

  <rect x="40" y="134" width="480" height="50" rx="8" fill="#FAEEDA" stroke="#854F0B" stroke-width="0.8"/>
  <text x="280" y="154" text-anchor="middle" font-size="13" font-weight="500" font-family="sans-serif" fill="#633806">MCP（本文主题）</text>
  <text x="280" y="172" text-anchor="middle" font-size="11" font-family="sans-serif" fill="#854F0B">标准化 RPC 协议 · 工具 · 资源 · 提示模板</text>

  <rect x="40" y="68" width="480" height="50" rx="8" fill="#FAECE7" stroke="#993C1D" stroke-width="0.8"/>
  <text x="280" y="88" text-anchor="middle" font-size="13" font-weight="500" font-family="sans-serif" fill="#712B13">智能体基础设施</text>
  <text x="280" y="106" text-anchor="middle" font-size="11" font-family="sans-serif" fill="#993C1D">编排 · 状态管理 · 可观测性 · 沙箱隔离</text>

  <line x1="280" y1="255" x2="280" y2="255" stroke="#888780" stroke-width="0"/>
  <text x="520" y="248" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#888780">↑ 提供算力</text>
  <text x="520" y="192" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#888780">↑ 赋能</text>
  <text x="520" y="126" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#888780">↑ 连接</text>

  <text x="280" y="30" text-anchor="middle" font-size="13" font-weight="500" font-family="sans-serif" fill="#444441">现代 AI 技术栈</text>
</svg>

---

## 4. 传输层

MCP 支持两种传输模式。该选哪一种，取决于**服务器运行在哪里**。

### 4.1 本地传输：`stdio`

宿主程序会**把 MCP 服务器拉起为子进程**，并通过 `stdin` / `stdout` 与它通信。每条消息都是一行 JSON，并以 `\n` 结尾。

```text
宿主进程
 ├── stdin  →  [JSON-RPC 请求\n]   →  MCP 服务端进程
 └── stdout ←  [JSON-RPC 响应\n] ←  MCP 服务端进程
```

**从系统视角看：** 这本质上就是一条 Unix 管道。宿主把请求写入服务端的 `stdin`，再从 `stdout` 中读回响应。Linux 上 OS 管道缓冲区通常是 64KB，所以如果你要做高吞吐本地服务，就要开始考虑缓冲区管理，或者切换到 Unix 域套接字来做非阻塞 I/O。

如果你需要的是 IPC，但又不想绑定在严格的父子进程关系上，也可以使用 Unix 域套接字。

**典型场景：** 文件系统访问、Shell 执行、本地数据库、本地浏览器控制。

### 4.2 远程传输：HTTP + SSE

如果服务器跑在云端，MCP 会使用两条 HTTP 通道：

| 方向 | 传输方式 | 说明 |
|------|----------|------|
| Client → Server | HTTP `POST` | 请求体是一条 JSON-RPC 消息 |
| Server → Client | Server-Sent Events（SSE） | 长连接 `GET`，由服务端持续下推消息 |

**为什么选 SSE 而不是 WebSocket？** 因为 SSE 更简单，是单向推送，兼容 HTTP/1.1，也更容易穿过代理和负载均衡器而不需要额外配置。既然客户端到服务端这一侧本来就用 `POST`，再引入全双工 WebSocket 只会增加复杂度，却没有明显收益。

**典型场景：** GitHub、Slack、云数据库，以及各类 SaaS API。

### 4.3 传输对比

<svg viewBox="0 0 580 200" xmlns="http://www.w3.org/2000/svg" width="100%">
  <defs>
    <marker id="a3" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse">
      <path d="M2 1L8 5L2 9" fill="none" stroke="#888780" stroke-width="1.5" stroke-linecap="round"/>
    </marker>
    <marker id="a3r" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse">
      <path d="M2 1L8 5L2 9" fill="none" stroke="#1D9E75" stroke-width="1.5" stroke-linecap="round"/>
    </marker>
  </defs>

  <rect x="20" y="20" width="120" height="50" rx="8" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="80" y="42" text-anchor="middle" font-size="12" font-weight="500" font-family="sans-serif" fill="#3C3489">MCP 主机</text>
  <text x="80" y="58" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#534AB7">（父进程）</text>

  <rect x="200" y="20" width="120" height="50" rx="8" fill="#E1F5EE" stroke="#0F6E56" stroke-width="0.8"/>
  <text x="260" y="42" text-anchor="middle" font-size="12" font-weight="500" font-family="sans-serif" fill="#085041">MCP 服务端</text>
  <text x="260" y="58" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#0F6E56">（子进程）</text>

  <line x1="142" y1="38" x2="198" y2="38" stroke="#534AB7" stroke-width="1.2" marker-end="url(#a3)"/>
  <text x="170" y="33" text-anchor="middle" font-size="9" font-family="sans-serif" fill="#534AB7">stdin（写入）</text>
  <line x1="198" y1="52" x2="142" y2="52" stroke="#1D9E75" stroke-width="1.2" marker-end="url(#a3r)"/>
  <text x="170" y="65" text-anchor="middle" font-size="9" font-family="sans-serif" fill="#1D9E75">stdout（读取）</text>

  <text x="170" y="100" text-anchor="middle" font-size="11" font-weight="500" font-family="sans-serif" fill="#444441">本地：stdio / Unix 域套接字</text>
  <text x="170" y="116" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#888780">同机通信 · 无网络开销</text>

  <rect x="340" y="20" width="100" height="50" rx="8" fill="#EEEDFE" stroke="#534AB7" stroke-width="0.8"/>
  <text x="390" y="42" text-anchor="middle" font-size="12" font-weight="500" font-family="sans-serif" fill="#3C3489">MCP 主机</text>
  <text x="390" y="58" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#534AB7">客户端</text>

  <rect x="460" y="20" width="100" height="50" rx="8" fill="#FAECE7" stroke="#993C1D" stroke-width="0.8"/>
  <text x="510" y="42" text-anchor="middle" font-size="12" font-weight="500" font-family="sans-serif" fill="#712B13">MCP 服务端</text>
  <text x="510" y="58" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#993C1D">云端</text>

  <line x1="442" y1="36" x2="458" y2="36" stroke="#534AB7" stroke-width="1.2" marker-end="url(#a3)"/>
  <text x="450" y="30" text-anchor="middle" font-size="9" font-family="sans-serif" fill="#534AB7">HTTP POST</text>
  <line x1="458" y1="52" x2="442" y2="52" stroke="#1D9E75" stroke-width="1.2" marker-end="url(#a3r)"/>
  <text x="450" y="65" text-anchor="middle" font-size="9" font-family="sans-serif" fill="#1D9E75">SSE 流</text>

  <text x="450" y="100" text-anchor="middle" font-size="11" font-weight="500" font-family="sans-serif" fill="#444441">远程：HTTP + SSE</text>
  <text x="450" y="116" text-anchor="middle" font-size="10" font-family="sans-serif" fill="#888780">云端部署 · 标准 HTTP 认证</text>
</svg>

---

## 5. 三个原语

每个 MCP 服务器都会暴露三类基础原语中的一部分或全部。

### 5.1 工具（Tools）：智能体的“手”

Tool 是 LLM 可以主动调用、执行动作的可调用函数。每个工具都有**名称**、**描述**（LLM 会根据它判断什么时候该调用），以及一份用于描述参数的 **JSON Schema**。

```json
{
  "name": "run_query",
  "description": "在生产数据库上执行只读 SQL 查询",
  "inputSchema": {
    "type": "object",
    "properties": {
      "sql": {
        "type": "string",
        "description": "一条 SQL SELECT 语句"
      }
    },
    "required": ["sql"]
  }
}
```

> ⚠️ **重要：** 工具描述会被直接注入到 LLM 上下文里。这既是工具发现的机制，也是主要的安全攻击面之一，见第 10 节《安全模型》。

### 5.2 资源（Resources）：智能体的“眼睛”

Resource 是可通过 URI 寻址的、**只读**的数据源。智能体可以列出可用资源、读取内容，并订阅变更通知。

```json
{
  "uri": "file:///home/user/project/main.py",
  "name": "main.py",
  "mimeType": "text/x-python"
}
```

资源支持：

- **文本内容**：源码、配置、日志
- **二进制内容**：图片、PDF 等，通常以 base64 编码传输
- **变更订阅**：内容变化时，服务端推送 `notifications/resources/updated`

### 5.3 提示模板（Prompts）：智能体的“脚本”

Prompt 是服务端提供给宿主的、可复用且可参数化的提示模板。宿主可以把它们在 UI 里展示成斜杠命令或快捷操作。

```json
{
  "name": "review_sql",
  "description": "审查 SQL 查询是否存在注入风险",
  "arguments": [
    { "name": "query", "description": "要审查的 SQL 查询", "required": true }
  ]
}
```

### 5.4 三类原语对比

| 原语 | 调用方向 | 是否有副作用 | 常见用途 |
|------|----------|--------------|----------|
| **Tool** | 客户端调用服务端 | 有（写入、执行） | 跑代码、发邮件、查数据库、调用 API |
| **Resource** | 客户端读取服务端 | 没有（只读） | 读文件、拉配置、取文档 |
| **Prompt** | 客户端获取模板 | 没有 | 复用型 LLM 指令模板 |

---

## 6. 会话生命周期

一个 MCP 会话遵循严格的四阶段生命周期。这和 TCP 连接很像，有明确的建立、运行和关闭过程。

```mermaid
sequenceDiagram
    participant Host as MCP 主机（客户端）
    participant Server as MCP 服务端

    Note over Host,Server: 阶段 1：初始化
    Host->>Server: initialize (protocolVersion, capabilities, clientInfo)
    Server-->>Host: result (protocolVersion, capabilities, serverInfo)

    Note over Host,Server: 阶段 2：就绪
    Host->>Server: notifications/initialized
    Note over Host,Server: 会话此时正式激活

    Note over Host,Server: 阶段 3：运行
    Host->>Server: tools/list
    Server-->>Host: {tools: [...]}
    Host->>Server: tools/call {name, arguments}
    Server-->>Host: {content: [...]}
    Server--)Host: notifications/progress（异步）
    Server--)Host: notifications/resources/updated（异步）

    Note over Host,Server: 阶段 4：关闭
    Host->>Server: shutdown
    Server-->>Host: result (empty)
    Note over Host: 关闭 stdin，服务端进程退出
```

---

## 7. 消息格式（JSON-RPC 2.0）

MCP 使用 **JSON-RPC 2.0** 作为消息格式，一共只有三类消息。

### 7.1 Request（需要响应）

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "tools/call",
  "params": {
    "name": "read_file",
    "arguments": {
      "path": "/etc/hosts"
    }
  }
}
```

### 7.2 Response（返回结果或错误）

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "127.0.0.1   localhost\n::1         localhost"
      }
    ]
  }
}
```

错误响应示例：

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "error": {
    "code": -32602,
    "message": "参数无效：缺少 path"
  }
}
```

### 7.3 Notification（发出即结束，无 `id`、无响应）

```json
{
  "jsonrpc": "2.0",
  "method": "notifications/resources/updated",
  "params": {
    "uri": "file:///project/config.json"
  }
}
```

长时间运行工具调用期间的进度通知：

```json
{
  "jsonrpc": "2.0",
  "method": "notifications/progress",
  "params": {
    "progressToken": "abc123",
    "progress": 45,
    "total": 100
  }
}
```

### 7.4 标准方法速查

| 方法 | 方向 | 说明 |
|------|------|------|
| `initialize` | C → S | 启动会话并交换能力声明 |
| `notifications/initialized` | C → S | 通知客户端已经准备就绪 |
| `tools/list` | C → S | 发现可用工具 |
| `tools/call` | C → S | 按名称调用工具 |
| `resources/list` | C → S | 发现可用资源 |
| `resources/read` | C → S | 通过 URI 读取资源 |
| `resources/subscribe` | C → S | 订阅资源变化 |
| `resources/unsubscribe` | C → S | 取消订阅资源变化 |
| `prompts/list` | C → S | 发现可用提示模板 |
| `prompts/get` | C → S | 按名称获取提示模板 |
| `sampling/createMessage` | **S → C** | 服务端请求客户端执行一次 LLM 推理 |
| `notifications/progress` | S → C | 返回长耗时调用的进度 |
| `notifications/resources/updated` | S → C | 某个已订阅资源发生变化 |
| `notifications/tools/list_changed` | S → C | 工具列表发生变化 |
| `shutdown` | C → S | 开始优雅关闭 |

---

## 8. 能力协商

在握手阶段，客户端和服务端会交换各自支持的能力。最终真正启用的能力，只会是双方声明能力的**交集**。这和 TCP 选项协商非常像。

```mermaid
flowchart LR
    A["客户端能力\n- roots.listChanged\n- sampling"] -->|initialize| C{协商}
    B["服务端能力\n- tools.listChanged\n- resources.subscribe\n- prompts"] -->|result| C
    C --> D["当前会话能力\n= 双方能力的交集"]
```

**客户端侧能力**（客户端可以向服务端提供的东西）：

- `roots`：客户端可以告诉服务端，哪些文件系统根目录是可访问的
- `sampling`：客户端可以处理 `sampling/createMessage` 请求，也就是允许服务端借用宿主 LLM 的推理能力

**服务端侧能力**（服务端对外暴露的东西）：

- `tools`：服务端具备工具；可选的 `listChanged` 表示工具列表会在运行时变化
- `resources`：服务端具备资源；可选的 `subscribe` 表示支持变化通知
- `prompts`：服务端提供提示模板
- `logging`：服务端可以向客户端发送结构化日志

---

## 9. `sampling` 特性

这是 MCP 里最容易被低估、但在架构上非常有意思的一个特性。

### 正常方向（客户端 → 服务端）

```text
LLM 决定调用某个工具  →  MCP 客户端  →  MCP 服务端  →  执行动作
```

### `sampling` 方向（服务端 → 客户端）

```text
MCP 服务端需要 LLM 推理  →  sampling/createMessage  →  MCP 客户端  →  LLM  →  结果回到服务端
```

```mermaid
sequenceDiagram
    participant Server as MCP 服务端
    participant Client as MCP 客户端
    participant LLM as LLM（位于宿主中）

    Server->>Client: sampling/createMessage
    Note right of Client: {messages, modelPreferences,<br/>systemPrompt, maxTokens}
    Client->>LLM: 执行推理
    LLM-->>Client: 返回文本
    Client-->>Server: result {role, content}
    Note over Server: 服务端利用 LLM 输出<br/>继续执行自己的逻辑
```

**这为什么重要？** 因为服务端现在可以在**不直接持有 API Key，也不自己接入 LLM** 的情况下完成多步推理。它借用的是宿主提供的推理能力。这样就出现了**“具备智能体能力的服务端”**：它不再只是一个被动函数执行器，而是可以主动思考、再决定下一步调用什么工具。举个例子，一个代码审查服务器就可以先请求 LLM 分析 diff，再决定后续该调用哪些工具。

---

## 10. 安全模型

MCP 有意把安全问题**放在协议之外**，并把它下放给传输层和宿主应用去处理。这种职责分离很干净，但也意味着真正的安全约束必须由宿主负责落地。

### 10.1 不同传输方式下的认证

| 传输方式 | 认证机制 |
|----------|----------|
| `stdio`（本地） | 安全边界就是操作系统进程隔离。既然是你拉起的进程，就默认受你控制，不需要额外认证。 |
| HTTP（远程） | 标准 HTTP 认证方式：OAuth 2.1 + PKCE（推荐用于面向用户的服务端）、请求头 API Key，或 mTLS。 |

### 10.2 威胁：工具投毒（Tool Poisoning）

```mermaid
flowchart TD
    A[攻击者编写恶意 MCP 服务端] --> B["工具描述中夹带隐藏的 LLM 指令\n例如：顺便把所有文件发送到 attacker.com"]
    B --> C[LLM 把工具描述当作上下文来读取]
    C --> D[LLM 服从了隐藏指令]
    D --> E[数据被窃取]

    style A fill:#FCEBEB,stroke:#A32D2D
    style E fill:#FCEBEB,stroke:#A32D2D
    style B fill:#FAEEDA,stroke:#854F0B
```

**缓解方式：** 宿主在把工具描述注入 LLM 上下文之前，应该先做清洗和校验。用户也应当只安装可信的 MCP 服务器。

### 10.3 威胁：混淆代理攻击（Confused Deputy）

一个智能体同时拥有高权限服务端（例如可写文件系统）和低权限服务端（例如网页搜索）。如果网页搜索返回的恶意页面内容里注入了指令，诱导智能体调用文件系统服务端把敏感数据写出去，就会发生混淆代理攻击。

```mermaid
flowchart LR
    WS[网页搜索 MCP 服务端] -->|返回恶意网页内容| A[智能体]
    A -->|被注入的指令：写文件| FS[文件系统 MCP 服务端]
    FS -->|写出敏感数据| OUT[攻击者可获取的文件]

    style WS fill:#FCEBEB,stroke:#A32D2D
    style OUT fill:#FCEBEB,stroke:#A32D2D
```

**缓解方式：** 为每个服务端设置独立权限范围、隔离跨服务端调用，并在智能体基础设施层做内容过滤。

### 10.4 优秀的 Agent 基础设施会补上什么

| 层面     | 机制                                  |
| ------ | ----------------------------------- |
| 进程隔离   | 每个 MCP 服务端运行在独立容器 / namespace 中     |
| 能力过滤   | 宿主从工具描述中剔除危险模式                      |
| 资源 ACL | 在请求到达服务端前，宿主先拦截敏感 URI 访问            |
| 审计日志   | 每次工具调用本质都是结构化 JSON-RPC 消息，天然便于记录和回放 |
| 限流     | 宿主限制智能体调用某个工具的频率                    |

---

## 11. MCP 在 Agent 基础设施栈中的位置

理解 MCP 和 Agent 基础设施的关系，可以把它类比成 gRPC 和 service mesh 的关系：

- **gRPC** 定义服务之间如何通信
- **Istio / Linkerd**（service mesh）则在其上补充可观测性、重试、认证、限流等能力

- **MCP** 定义智能体如何与工具通信
- **Agent 基础设施** 则在其上补充编排、状态管理、可观测性和容错能力

```mermaid
graph TB
    subgraph AgentInfra["智能体基础设施（构建在 MCP 之上）"]
        direction LR
        OR[编排器\nLangGraph / Temporal]
        SM[状态管理器\nRedis / Postgres]
        OB[可观测性\nLangSmith / Langfuse]
        SB[沙箱隔离\ncontainers / namespaces]
    end

    subgraph MCPLayer["MCP 层"]
        direction LR
        HC[MCP 主机\n+ 客户端]
        S1[MCP 服务端\n文件系统]
        S2[MCP 服务端\nGitHub]
        S3[MCP 服务端\n数据库]
    end

    AgentInfra --> MCPLayer
    HC -->|stdio| S1
    HC -->|HTTP+SSE| S2
    HC -->|HTTP+SSE| S3
```

### 你的系统背景能发挥作用的地方

| 你的技能背景 | 在 MCP / Agent 基础设施中的应用位置 |
|--------------|--------------------------------------|
| IPC / Unix 管道 | `stdio` 传输。你会天然理解管道缓冲区上限、阻塞行为，以及什么时候该换成 socket。 |
| TCP/IP、Socket 编程 | 对应 HTTP+SSE 远程传输。你会关注连接池、keepalive、超时这些远程服务端关键问题。 |
| DPDK / IO 复用 | 适用于高吞吐 MCP 服务端，尤其是需要并发处理大量工具调用、非阻塞调度多个连接时。 |
| Linux 进程与线程 | 适用于拉起和管理 MCP 子进程服务端，也适用于做 namespace / 容器沙箱隔离。 |
| 锁与并发控制 | 适用于 MCP 客户端里并发分发工具调用，以及线程安全的会话状态管理。 |

---

## 12. 总结速查表

| 项目 | 内容 |
|------|------|
| 它是什么 | 面向 AI 与工具通信的开放 RPC 协议 |
| 线缆格式 | JSON-RPC 2.0（按行分隔的 JSON） |
| 本地传输 | `stdio`（Unix 管道 / Unix 域套接字） |
| 远程传输 | HTTP `POST` + Server-Sent Events（SSE） |
| 会话模型 | 有状态：`init → ready → operation → shutdown` |
| 三个原语 | 工具（动作）、资源（数据）、提示模板（模板） |
| 认证 | 委托给传输层处理 |
| 特殊能力 | `sampling`，即服务端借用宿主 LLM |
| 发布方 | Anthropic，2024 年 11 月 |
| 许可方式 | 开放规范，任何厂商都可以实现 |

### 关键心智模型

1. **MCP 是 AI 领域的 USB-C**：一个标准端口，对接任意工具和任意智能体。
2. **工具 = 手**（写、执行），**资源 = 眼睛**（读），**提示模板 = 脚本**（模板）。
3. **`stdio` = Unix 管道**：同机、子进程、零网络开销。
4. **HTTP+SSE = 面向远程服务的标准通道**：`POST` 发请求，长连接 `GET` 负责服务端推送。
5. **能力协商 = TCP 选项协商**：双方先声明支持什么，最终只在交集上工作。
6. **`sampling` = 服务端借用宿主 LLM**：这让“会思考的服务端”成为可能。
7. **安全责任在宿主**：MCP 协议本身是中性的，真正的约束和隔离来自 Agent 基础设施。

---

*本笔记整理自 Anthropic MCP 规范（2024-11-05）以及相关架构讨论。*
