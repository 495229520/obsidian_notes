---
title: MFMS 上位机 Bug 修复汇总：锁、竞态与控制权
date: 2026-07-16
tags:
  - 研一上学期/复合机器人汇总
status: 已完成
---

# MFMS 上位机 Bug 修复汇总：锁、竞态与控制权

> [!abstract] 一句话总结
> 现场"有时候控制不了、退出重开才好"的病根是**适配器全局锁横跨阻塞 RPC**（锁问题），而不是信号链竞态；配套修掉了控制权状态不同步（40101）、连接无回包、结果无关联 ID 三个结构缺陷，并用自研浸泡测试在模拟与真机双环境验证闭环。

仓库：`Reconstructed-MFMS`，相关提交 `55421a1 → 4f33b3b`（2026-07-15 ~ 07-16）。
架构背景见 [[MFMS_DataCenter_Architecture]]。

---

## 症状与定位方法

**现场症状**：实机调试时客户端偶发性完全失控，急停也无响应，只能杀掉客户端重开；AGV 控制权被下位机/Roboshop 夺走后，UI 按钮卡死在「释放控制权」，点击报错误码 40101。

**定位方法**：写了一个 headless 浸泡测试工具 `soak_arm_jog`（`src/mfms_server/tools/`），走与 Qt UI **完全相同**的 7 层客户端栈，对机械臂做"上→下→左→右→转+→转-"成对自抵消的最小步长 jog 循环，记录每条指令的时延与结果并自动分类：

- `timeout_late`（慢而不丢）→ 下游阻塞/锁串行化
- `timeout`（真丢）→ 信号链竞态
- 串扰/重复 → 结果错配对
- Phase B（流水线连发）时延 vs Phase A（串行）→ 量化锁排队

**关键证据**：模拟环境 1.2 万条指令**零丢失零串扰** → 排除信号链竞态；Phase B 并发 3 条时 p50 时延精确变为 2.3 倍 → 实锤锁串行排队；下位机中途重启的整个停机窗口被**一条**阻塞 RPC 独自吞掉 → 实锤"一条指令卡死全系统"。

---

## Bug 清单与修法

### Bug 1：AGV 控制权被外部夺走后 UI 状态不同步（40101 卡死）

- **提交**：`55421a1`
- **现象**：客户端接管控制权后，下位机/Roboshop 再夺权，UI 按钮仍显示「释放控制权」；点释放报 `40101 req_forbidden`（本端已不是控制方），反复报错无法自愈。
- **根因**：接管状态机只在本端点击接管/释放时更新本地状态，没有任何机制感知"控制权被外部夺走"。
- **修法**：
	1. **主动轮询**：接管期间每 2s 用 SEER 303 查询控制权归属；接管成功时缓存占用者指纹（ip/port/type/nick_name/time），轮询比对指纹判断是否仍持有——time 字段可识别"同 IP 换主"。
	2. **错误码自愈**：释放/运动指令撞 40101/40020 时，把本地状态同步为"未接管"并提示，按钮复位。
	3. **信号链带 action**：`agvControlAuthority*` 全链路加 `action`（夺取/释放/查询）与 `ownedBySelf` 参数，轮询结果不再串进接管/释放的 pending 状态机。
	4. 设备未连接时控制权指令改走专用结果通道，修掉按钮永久禁用。

### Bug 2：适配器全局锁横跨阻塞 RPC —— "控制不了"的元凶

- **提交**：`bac7252`（①）
- **现象**：下位机一卡，所有设备的所有指令（含急停）全部无响应，最长 8 秒/条地排队。
- **根因**：`RobotProxyAdapter`/`AgvProxyAdapter` 的所有方法共用**一把全局 `QMutex`，且持锁跨越整个阻塞 RPC**；hyrms 代理的 `send()` 是 1ms 忙轮询、默认 `future_timeout_=8s`。AGV 接管序列最多 5 条串行 RPC，最坏 40 秒占锁。
- **修法**：
	1. **锁内只取 proxy**：全局锁只保护代理表与指纹缓存的存取；
	2. **每设备一把串行互斥**：RPC 在设备互斥下、全局锁外执行——同设备串行（代理成员如 `desc_pose` 非线程安全），跨设备并行，单台设备卡死不再拖累全局；
	3. **短超时**：`future_timeout_` 是 protected，用轻量派生类 `ShortRpcTimeoutProxy` 暴露设置入口，8s → 3s；
	4. Robot 连接前补 service 就绪预检（`wait_for_service(1s)`）——`connect()` 实现在预编译 .so 内，服务缺失时会无限阻塞。

### Bug 3：连接流程无失败回包（connectResult 永不返回）

- **提交**：`bac7252`（②）
- **现象**：代理连不上下位机（服务名不对/QoS 不兼容/linker 挂了）时，`connectRobot` 永远没有任何回包，UI 无限等待。
- **根因**：组连接只在"组内有在线设备"时才调度结果定时器；且 worker 线程被阻塞的连接 RPC 卡住时，2s 结果定时器根本无法触发。
- **修法**：`doConnectRobot` 增加 **20 秒死线**（`forceGroupConnectionResult`）：到期仍未出结果就按当时在线状态强制收尾、必发 `connectionResult`。配合 Bug 2 的短超时，worker 不再被无限阻塞。

### Bug 4：结果信号无请求关联 ID

- **提交**：`bac7252`（③）
- **现象**：`armControlRes`/`agvControlRes` 无法区分"哪条结果对应哪次请求"，UI 只能靠"发一等一"隐式配对，是并发下张冠李戴的结构性温床。
- **修法**：结果信号追加 `qint64 requestId` **尾参**（Qt 允许信号多出尾参连到旧 3 参槽函数，**存量 UI 零改动**）；运动类接口返回本次请求 ID。机械臂链路同步直返，用调用栈内 `activeArmRequestId_` 关联；AGV 链路异步派发，ID 塞进 `AgvMotionCommand` 结构体全程携带。

### Bug 5：模拟下位机与新版代理不兼容（QoS + 服务命名）

- **提交**：`11b042c`
- **现象**：浸泡测试连模拟设备时 `connectRobot` 卡死无回包（此现象顺带暴露了 Bug 3）。
- **根因**：两处——① 模拟器状态话题用 `SensorDataQoS`（BEST_EFFORT）发布，而 hyrms_export 代理的状态订阅是 RELIABLE，**QoS 不兼容导致代理收不到状态**；② Fr 指令服务还用旧前缀命名 `/robFro_cmd`，新版代理按完整设备 ID 调 `/robFro0002_cmd`，指令无人应答。
- **修法**：状态发布改 RELIABLE（对 BEST_EFFORT/RELIABLE 订阅都兼容）；服务名统一按完整设备 ID。

### Bug 6：Aubo 笛卡尔单位误换算（自引入，真机纠错）⭐ 教训最深

- **提交**：`29cc7c6`（引入）→ `4f33b3b`（修正）
- **过程**：真机测试前读到 aubo_sdk 头文件注释（米/弧度、示例位姿 `{-0.155, -0.727, 0.439, ...}`），据此给工具加了 mm→m 换算。真机一跑，下位机报**"路点不可达 -1"**——0.5mm 被换算成半微米，目标点与当前点数值重合，奥博控制器拒绝零长度规划。
- **真相**：下位机 HyRMS 的 Aubo 封装层**已把 aubo_sdk 的米/弧度归一为 mm/deg**（与法奥一致），实测 `/robAub0001_state` 的 `tl_cur_pos = [449.2, -161.0, 237.3, 179.9, -0.2, 89.3]`，量级一目了然。
- **修法**：撤销换算，Fr/Aubo 一律 mm/deg 直传。

> [!warning] 教训
> **单位问题一律以 `ros2 topic echo` 实测量级为准，绝不信 SDK 头文件注释**——中间封装层可能做过归一。另注意：linker 内部日志打印仍是米/弧度（`x:0.449 z:0.237`），对照时别再误判。

---

## 配套工具：soak_arm_jog

- **提交**：`7dc205c`（工具）+ `624fedd`（真机化）
- 默认**只连 group_id=0 模拟设备**；连真机必须显式 `--allow-group <组号>`，且自动套护栏：强制关 Phase B 流水线（jog 的 getDescPose+moveL 读-改-写非原子，真机并发会物理漂移）、指令间隔 ≥300ms、步长硬上限 2mm/2°。
- `--skip-restore` = 零写库模式（不保存起始路点，靠成对指令自抵消回原位）——生产库红线友好。
- 组连接会拉起组内**全部**设备（含 AGV 会话建立与 AUBO 连接即自动使能），真机使用前必须清场。

```bash
# 模拟环境
ros2 run mfms_server simulated_lower_machine --kind fr --device-id robFro0002 --group-id 0
ros2 run mfms_server soak_arm_jog --device-id robFro0002 --duration 300

# 真机(清场+有人守急停)
ros2 run mfms_server soak_arm_jog --device-id robAub0001 --allow-group 1 \
    --duration 60 --step 0.5 --rot-step 0.5 --settle-ms 500 --skip-restore
```

---

## 验证结果

| 环境 | 规模 | 结果 |
| --- | --- | --- |
| 容器模拟 5min | 4065 条 | 零失败/零超时/零串扰；A 相 p50=30ms，B 相 70ms(锁排队证据) |
| 容器模拟 10min | 7664 条 | 同上；前后半程时延 31.0→31.4ms，无累积退化 |
| 修复后回归 60s | 1122 条 | 全绿，连接/恢复原位正常 |
| **真机 robAub0001** | 84 条(14 周期) | 全绿；linker 侧确认 0.5mm(z 0.237↔0.238m)/0.5°(rz 1.559↔1.568rad) 物理运动；原位残差 0.087 ≪ 步长；agvSm4 连接失败被隔离不拖累全组 |

---

## 下位机环境问题（非上位机代码，运维项）

排查过程中在下位机 `192.168.83.74` 顺带确认的病灶：

1. **Fast DDS 共享内存残留锁**：`/dev/shm` 里 88 个 fastrtps 文件跨天残留，`open_and_lock_file failed`——同机 SHM 传输"发现正常但数据不通"，与"重启才好"高度相关。治本：profile 禁 SHM 强制 UDPv4。
2. **重名节点**：3 份 myviz 残留实例挂在 DDS 图上（`/myviz/subscriber` ×3）。
3. **nouveau GPU 崩溃**：两次内核级 GPU 错误直接杀掉 gnome-shell/X 会话（连带杀死上位机进程）。建议换 NVIDIA 官方驱动。
4. **Aubo SDK 空闲掉线**：空闲数分钟后连接断开，linker 靠自动重连恢复（-1611 重连风暴属正常自愈，但值得关注）。
5. 下位机 git 远端 URL 内嵌明文 PAT，建议换 SSH key。

---

## 遗留事项

- [ ] 真机故障注入验证：现场杀 linker，确认急停不再排队 8s、连接 20s 内弹失败提示
- [ ] `/dev/shm` 清理 + 禁 SHM profile + systemd 托管 hyrms/qt_file（日志落盘）
- [ ] UI 侧利用 requestId 做精确结果配对（信号已带，UI 尚未消费）
- [ ] 本地 `compose.yaml`(3306 映射)、`bal.bash` 两个未提交改动待定夺
