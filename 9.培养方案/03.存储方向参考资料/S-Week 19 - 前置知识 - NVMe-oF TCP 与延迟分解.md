---
title: S-Week 19 - 前置知识 - NVMe-oF TCP 与延迟分解
date: 2026-07-12
tags:
  - infra
  - 存储
  - 参考资料
aliases:
  - 存储 Week 19 前置知识
  - NVMe-oF 前置知识
status: active
---

# S-Week 19 - 前置知识 - NVMe-oF TCP 与延迟分解

## 索引

- [[#0. 先建立直觉：把盘插到网络上，上层无感]]
- [[#1. NVMe-oF 架构：host、target 与 NQN]]
- [[#2. NVMe/TCP 数据面：PDU 与往返次数]]
- [[#3. nvmet 搭建：configfs 的目录树就是配置]]
- [[#4. 两套拓扑：netns 调功能，双机测性能]]
- [[#5. 延迟分解：三段减法怎么做才闭环]]
- [[#6. 带宽段位与队头阻塞]]
- [[#7. 配菜启动：JuiceFS 泛读指引]]
- [[#8. 常见错误]]
- [[#9. 学习检查清单]]
- [[#10. 关键要点总结]]
- [[#关联知识]]
- [[#参考]]

## 阅读说明

这篇是 [[S-Week 19 - NVMe-oF TCP 与延迟分解]] 的总前置知识：搭 target 前通读 1-3 节，选拓扑前看 4 节，做分解实验前精读 5 节。transport 取舍的深挖版见 [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]]。

---

> 上周你在本机把 NVMe 摸透了；本周把"机箱"这个边界拆掉。NVMe-oF 做的事一句话：**把上周学的 qpair 从 PCIe 搬到网络连接上，让远端的盘出现在本机的 `nvme list` 里**——上层文件系统、fio、你的 mini-kv-engine 完全无感。代价是每笔 I/O 多了一段网络和两台机器的软件栈，本周的核心技能就是把这段多出来的延迟**拆开、称重、归因**。

---

## 0. 先建立直觉：把盘插到网络上，上层无感

搭好 target 之后，host 端跑一句 `nvme connect`，然后：

```bash
nvme list
# /dev/nvme1n1  ...  Linux  ...   ← 一块"新盘"出现了，物理上在另一台机器
fio --filename=/dev/nvme1n1 ...   # 上周的所有工具原样能跑
```

这就是 NVMe-oF 的产品形态：**盘的位置成了部署细节**。为什么行业需要它——两个词：

- **盘池化**：盘集中在存储节点，容量按需划给计算节点；不再有"这台机器盘满了、那台空着"。
- **存算分离**：计算和存储独立扩缩、独立故障域。AI 训练集群（也包括 3FS 这类系统）的存储层全是这个思路。

> [!important] 第一性原理
> NVMe-oF 不发明新命令——**同一套 NVMe 命令集，换一条腿跑**。理解它的钥匙是映射关系：qpair ↔ 网络连接、SQE/CQE ↔ 网络消息（capsule）、doorbell ↔ 没有了（网络消息的到达本身就是通知）。抓住"什么变了、什么没变"，一小时就能建立全图。

## 1. NVMe-oF 架构：host、target 与 NQN

| 概念 | 是什么 | 对应本地 NVMe 的什么 |
|---|---|---|
| host | 发起端（用盘的机器） | 主机驱动 |
| target | 提供端（有盘的机器） | 控制器 |
| subsystem | target 上导出的一个"虚拟控制器" | 一块 NVMe 盘的控制器 |
| namespace | subsystem 里的一个块设备 | 本地盘的 namespace |
| port | subsystem 的网络监听点（transport + 地址 + 端口） | PCIe 插槽 |
| NQN | NVMe Qualified Name，host 和 subsystem 的全局唯一名字 | — |

连接建立：host 对 target 的 discovery 服务发起 `nvme discover`（列出有哪些 subsystem）→ `nvme connect -n <目标NQN>` → 协商建立 admin qpair 和若干 IO qpair——**每个 qpair 映射成一条独立的 TCP 连接**。上周的"per-CPU qpair"在这里变成"per-CPU TCP 连接"，多队列的并行结构原样保留。

## 2. NVMe/TCP 数据面：PDU 与往返次数

TCP 是字节流，NVMe 命令是离散消息——中间需要一层定界封装：**PDU**（Protocol Data Unit）。常用的几种：

| PDU | 方向 | 作用 |
|---|---|---|
| ICReq / ICResp | 双向 | 连接初始化，协商参数（数据摘要、PDU 大小等） |
| CapsuleCmd | host → target | 命令胶囊（SQE 的网络化身，小写入可内联数据） |
| CapsuleResp | target → host | 完成胶囊（CQE 的网络化身） |
| C2HData | target → host | 读数据（Controller to Host） |
| R2T / H2CData | target → host / host → target | 写路径：target 说"可以传了"（R2T），host 再发数据 |

往返次数口算（QD1、忽略 TCP ACK）：

- **4K 读**：CapsuleCmd 去 → C2HData + CapsuleResp 回 ≈ **1 个 RTT**。
- **4K 写**：小写可以把数据内联进命令胶囊 ≈ 1 个 RTT；大写要走 R2T 流程（命令去 → R2T 回 → 数据去 → 完成回）≈ **2 个 RTT**——这就是"大块写在 NVMe/TCP 上延迟不成比例变差"的协议级原因。

字节流上怎么定界：PDU 头里带类型和长度——**这就是 S1 网络八股里"粘包/拆包"问题的工业标准答案**，本周面试保底的 [[8.15 输入输出缓冲区与协议解析器]] 与它互证。

## 3. nvmet 搭建：configfs 的目录树就是配置

内核 target（nvmet）没有配置文件——**configfs 的目录结构本身就是配置**，mkdir 就是"创建对象"，echo 进文件就是"设属性"：

```bash
sudo modprobe nvmet nvmet-tcp
cd /sys/kernel/config/nvmet
# 1. 建 subsystem
mkdir subsystems/nqn.2026-07.io.lab:kv
echo 1 > subsystems/nqn.2026-07.io.lab:kv/attr_allow_any_host
# 2. 建 namespace，绑后端设备（真盘或文件均可）
mkdir subsystems/nqn.2026-07.io.lab:kv/namespaces/1
echo /dev/nvme0n1 > subsystems/nqn.2026-07.io.lab:kv/namespaces/1/device_path
echo 1 > subsystems/nqn.2026-07.io.lab:kv/namespaces/1/enable
# 3. 建 port（tcp、地址、4420）
mkdir ports/1
echo tcp    > ports/1/addr_trtype
echo ipv4   > ports/1/addr_adrfam
echo <IP>   > ports/1/addr_traddr
echo 4420   > ports/1/addr_trsvcid
# 4. 把 subsystem 挂到 port（软链接 = 发布）
ln -s /sys/kernel/config/nvmet/subsystems/nqn.2026-07.io.lab:kv \
      ports/1/subsystems/
```

host 端：

```bash
sudo modprobe nvme-tcp
sudo nvme discover -t tcp -a <IP> -s 4420
sudo nvme connect  -t tcp -a <IP> -s 4420 -n nqn.2026-07.io.lab:kv
nvme list          # 新盘出现
sudo nvme disconnect -n nqn.2026-07.io.lab:kv   # 清理
```

全部动作（含清理）进 `nvmet_setup.sh`；`nvmetcli` 工具可以把整棵 configfs 树存成 JSON 复原，适合双机场景。后端绑**文件**（loop 出来的）适合功能调试，绑真盘才能做性能对照。

## 4. 两套拓扑：netns 调功能，双机测性能

| 拓扑 | 搭法 | 用途 | 标签 |
|---|---|---|---|
| 同机双 netns | veth 对连两个 namespace，target/host 各占一个 | 配置调通、PDU 抓包学习 | **功能级**（回环延迟近零，数字无意义） |
| 两台云主机 | 真实网络，内网互通 + 放行 4420 | 延迟分解、QD 扫描 | **性能级但受虚拟网络影响**（声明） |

netns 拓扑十分钟能搭好且随开随关，先用它把脚本调对，再上双机——**调试用便宜环境，测量用真实环境**，这是全项目通用的省钱纪律。

## 5. 延迟分解：三段减法怎么做才闭环

模型：

$$
L_{total} \approx L_{network} + L_{software} + L_{device}
$$

三项的测法与来源：

| 项 | 测法 | 备注 |
|---|---|---|
| 网络 | `ping -c 100`（取中位数）或 iperf3 的 RTT；4K 数据的传输时延在万兆以上可忽略，千兆要加上 | 先测线速和 RTT，这是实验的"天气预报" |
| 设备 | S-Week 18 本地基线（**同 bs/QD/ioengine/direct**） | 分母口径必须一致 |
| 软件 | 余项 = 总延迟 − 网络 − 设备 | 含两端协议栈、nvmet 处理、PDU 封装 |

闭环校验：三段加起来和实测总延迟对不上（误差 > 20%）就找原因——虚拟网络抖动（多跑几轮看方差）、iostat 采样窗口错位、两侧时钟不同步（分解只用单侧计时，不跨机比时间戳）。**两侧同窗口观测**是定位偏差的利器：host 侧 fio 延迟 vs target 侧 biolatency——target 侧设备延迟没变、host 侧总延迟涨了，多出来的就在路上和栈里。

QD 扫描的预期形态：QD1 时远程比本地差一整个 RTT + 软件项（差距显著）；QD 升高后流水线掩盖单笔延迟，**吞吐差距收窄**（带宽或设备成为共同瓶颈）——"延迟差、吞吐平"是网络存储的标准形态，能解释它就是本周的合格线。

## 6. 带宽段位与队头阻塞

- 大块顺序读把 QD 堆上去，看吞吐贴多少线速（iperf3 先测出线速做分母）：贴得住 → 瓶颈在网络带宽；贴不住 → 瓶颈在盘或 CPU（看两侧 iostat/top）。
- 队头阻塞：多命令共享一条 TCP 连接（qpair），字节流严格有序——前面一个 1 MB 的 C2HData 没传完，后面 4K 读的完成就得排队。缓解手段是**多 qpair**（多连接），这是 NVMe/TCP 与 RDMA（消息语义、多 QP）的关键差异伏笔，S-Week 21 收口。

## 7. 配菜启动：JuiceFS 泛读指引

本周只花 1-2 小时，回答三个问题（笔记记在草稿，S-Week 21 汇入对比笔记）：

1. JuiceFS 的元数据放哪、数据放哪？（独立元数据引擎 Redis/TiKV/MySQL + 对象存储放数据）
2. 一个文件怎么变成对象？（file → 64 MiB chunk → slice → 默认 4 MiB block 落对象存储）
3. 一次写的路径经过谁？（客户端 → 元数据引擎事务 + 对象存储上传）

带着"它和 NVMe-oF 是两个层次的东西"这个视角读：NVMe-oF 导出**块设备**，JuiceFS 提供**文件系统语义**——盘池化 vs 文件共享，面试里别混。

## 8. 常见错误

- **connect 失败先怀疑代码**：九成是 `nvmet-tcp`/`nvme-tcp` 模块没加载、4420 被防火墙/安全组拦、或 NQN 写错——按这个顺序排查。
- **拿 netns 回环的延迟数字写结论**：回环没有真实网络，那是功能级环境——标签纪律。
- **分解的分母口径不一致**：本地基线是 buffered、远程测的是 O_DIRECT，减法从根上错。
- **跨机比时间戳**：两台机器时钟不同步，单侧计时才可信。
- **忘了先测 RTT 和线速**：没有网络基线，"软件开销"这个余项里混着网络抖动，归因失效。
- **只测 QD1**：错过"延迟差、吞吐平"的关键形态，结论只有半张。
- **实验完不清理**：残留的 connect 让下一轮 nvme list 出鬼盘；脚本必须含 disconnect + configfs 清理。

## 9. 学习检查清单

- [ ] 能画 host/target/subsystem/namespace/port 的关系图并说出 NQN 的作用。
- [ ] 能口算 4K 读 / 小写 / 大写在 NVMe/TCP 上的往返次数，并解释 R2T。
- [ ] 能用"PDU 定界"回答粘包问题——项目与八股互证。
- [ ] configfs 四步（subsystem → namespace → port → 软链）能脱稿写出。
- [ ] 三段延迟的测法、口径要求、闭环校验方法都能说清。
- [ ] 能解释"QD 高了差距收窄"和队头阻塞的机制。
- [ ] JuiceFS 三问有草稿答案。

## 10. 关键要点总结

- NVMe-oF = 同一套 NVMe 命令换条腿跑：qpair 映射成网络连接，上层无感——盘池化与存算分离的地基。
- NVMe/TCP 用 PDU 在字节流上定界；读 1 RTT、大写 2 RTT（R2T），这是延迟形态的协议根源。
- configfs 目录树即配置：mkdir 建对象、echo 设属性、软链发布。
- 延迟分解 = 总延迟 − ping 的网络 − 上周的设备基线，闭环靠口径一致 + 两侧同窗口观测。
- netns 调功能、双机测性能，每个数字带标签——三类实验纪律的本周形态。

## 关联知识

- [[S-Week 19 - NVMe-oF TCP 与延迟分解]]（本篇服务的周计划）
- [[S-Week 18 - 前置知识 - NVMe 命令模型与本地基线]]（qpair 概念与设备分母）
- [[NVMe-oF 专题 - TCP 与 RDMA transport 取舍]]（transport 对照深挖）
- [[8.9 TCP的粘包和拆包]]、[[8.15 输入输出缓冲区与协议解析器]]（PDU 定界的八股互证）
- [[分布式存储阅读专题 - JuiceFS 与 3FS]]（配菜的沉淀去处）
- [[块层观测专题 - iostat blktrace eBPF]]（两侧同窗口观测的工具箱）

## 参考

- NVMe over Fabrics Specification（架构与 capsule 模型概览章）
- NVMe/TCP Transport Specification（PDU 类型定义，查表用）
- 内核文档：Documentation/target（nvmet configfs 接口）；nvmetcli 文档
- `man nvme-connect`、`man nvme-discover`
- JuiceFS 官方架构文档（How JuiceFS Works）
