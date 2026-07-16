---
title: MFMS 上位机修改汇总：段错误、本地库与 AGV 接管功能
date: 2026-07-15
tags:
  - MFMS
  - 复合机器人
  - debug
  - ROS2
  - Qt
  - Docker
status: 已完成
---

# MFMS 上位机修改汇总：段错误、本地库与 AGV 接管功能

> [!abstract] 一句话总结
> 这一轮先把**运行环境跑通**（Docker 中文字体、断库不崩、本地内置 MySQL），再新增**AGV 控制权接管功能**（治现场 40020 "控制不了"），并按代码 review 修掉接管功能自身的两个 Blocker（停车误报、结果信号被击穿）+ 一个可控性缺陷（连接不置 connected）。控制权状态同步、适配器锁竞态等**更深的修复由后续一轮完成**，见 [[MFMS上位机Bug修复汇总-锁竞态与控制权]]。

仓库：`Reconstructed-MFMS`，相关提交 `52c1a83 → f697b0e → 7d7c695`（2026-07-13 ~ 07-15）。
架构背景见 [[MFMS_DataCenter_Architecture]]。

指令调用链（所有 AGV/机械臂指令同构，7 层）：

```
Qt UI(Control) → CommunicationInterface → CommunicationWorker(独立线程)
  → MfmsGateway → MfmsCommandService → AgvProxyAdapter/RobotProxyAdapter
  → SeerCtrl/FrRobot 代理(hyrms_export) → ROS2 service /<设备>_cmd → 下位机
```

---

## Bug 清单与修法

### Bug 1：Qt 界面中文全是方框（豆腐块）

- **提交**：`f697b0e`（Dockerfile）
- **现象**：noVNC 里进入界面，所有中文渲染成 □ 方框，英文数字正常。
- **根因**：基础镜像 `osrf/ros:humble-desktop-full` 不含任何 CJK 字体（`fc-list :lang=zh` 为 0），Qt 找不到能画汉字的字形。
- **修法**：Dockerfile 的 apt 列表加 `fonts-noto-cjk`，重启后 Qt 重新加载字体即恢复。

### Bug 2：登录后间歇性段错误 → 容器整体退出

- **提交**：`52c1a83`
- **现象**：登录后偶发崩溃，noVNC 显示「无法连接到伺服器」；日志有 `[ros2run]: Segmentation fault`。
- **根因**：`DatabasePool` 把 A 线程创建的 `QSqlDatabase` 连接通过队列交给 B 线程使用——Qt SQL 明确禁止跨线程共享连接，断库时在 QMYSQL 驱动内部产生未定义行为而崩溃。qt_file 是容器主进程（entrypoint `wait` 住它），一崩容器就整体退出，表面看是 VNC 连不上。
- **修法**：连接**按线程隔离缓存**（每线程首次请求时创建自己的连接、线程结束时清理），彻底消除跨线程使用；`LogHandle::getLogs()` 增加连接未打开时的跳过防护。

> [!note] 定位手法
> qt_file 崩 → 容器退出 → noVNC 报错，这条因果链是判断"是程序崩了还是 VNC 挂了"的关键：`docker compose logs` 里出现 `Segmentation fault` 就是程序崩，而非 VNC 问题。

### Bug 3：远程库离线时登录后必崩 / 设备树为空

- **提交**：`f697b0e`
- **现象**：原远程库 `100.84.157.31`（Tailscale）长期离线，连接超时后设备树数量为 0，配合 Bug 2 表现为登录即崩。
- **修法**：`compose.yaml` 新增内置 `db` 服务（mysql:8.0，原生 ARM64，命名卷持久化），**首次启动自动导入** `src/mfms_server/MFMS_BASE.sql`；`docker/mfms.env` 的 `MFMS_DB_HOST` 指向 `db`。此后不依赖远程库，断库也能降级运行（登录用硬编码 `admin/123456` 兜底）。

### Bug 4：AGV 控制指令被拒，错误码 40020（新增接管功能）⭐ 本轮核心

- **提交**：`7d7c695`
- **现象**：现场下发 AGV 运动指令全被拒，返回 `40020`。
- **根因**：`40020` 是仙工（SEER）Robokit 控制器**原生**返回码 `locked = 控制权被其他主体占用`（不是 HyRMS 的错误，HyRMS 的 AGV 错误码全是 -1500 系负数）。仙工是独占控制：下发运动前必须先抢占控制权；被 Roboshop / 调度 / 残留旧会话占着时一律 40020。相邻码佐证：40400 获取失败 / 40401 释放失败 / 40012 调度接管。
- **修法**：接入下位机新增的三个控制权接口（SEER 命令 **301 夺取 / 302 释放 / 303 查看**），贯穿全部 7 层：
	1. **控制页新增「接管权限」按钮**（工具栏红圈处）：点击弹确认框→接管；未接管时禁用 AGV 运动控件（手动 jog、到点、路径执行），停止/取消/暂停保持可用以便随时停车。
	2. **接管即停**：SEER 被夺权后**不会自动停**，接管成功后由上位机主动下发 `guideCancel`（停导航）+ `stopManualCtrl`（退手动）。
	3. 运动指令撞 40020 时提示"请先接管"并复位本地接管态。

### Bug 5（B1）：接管即停"误报已停车"

- **提交**：`7d7c695`
- **现象**：接管成功后消息报"已停车"，但车可能仍在动。
- **根因**：停车判定写成 `if (cancelRet == 0 || manualRet == 0)`。车在任一时刻只处于导航或手动之一，另一种必然"无可停"而返回非 0；`||` 意味着只要有一条平凡成功就报"已停"。且 `success` 只取 acquire 返回值，停车结果完全不影响成败——存在"接管成功但没停住却报成功"。
- **修法**：返回码无法可靠区分"无可停"与"真失败"，**不再断言已停**，措辞如实提示"已下发停车指令，请确认车辆已停止，如未停请按急停"；仅当两条停车指令都明确失败才升级强警告。`success` 只反映"接管"本身。

### Bug 6（B2）：接管状态机被无关结果击穿

- **提交**：`7d7c695`
- **现象**：导航进行中点接管，UI 可能把 1 秒一次的导航轮询结果误当作接管结果，直接置"已接管"、放开运动控件，而真正的接管可能随后失败。
- **根因**：接管/释放的 UI 状态机监听 `agvControlRes`，但这条信号上还流着导航状态轮询、pause/resume/cancel 等所有 AGV 指令结果；UI 用"下一个到达的事件"当接管结果，几乎必中竞态。
- **修法**：新增**独立信号链** `agvControlAuthorityExecuted → Result → Res`，接管/释放/查看三操作走它；导航/普通指令仍走 `agvControlRes`。UI 拆双槽：`onAgvControlAuthorityResult` 只处理接管状态机，`onAgvControlResult` 只处理"运动指令撞 40020"的提示与复位。

### Bug 7（part3）：连接 AGV 后仍控制不了（"没有力气"）

- **提交**：`7d7c695`
- **现象**：主页点「连接设备」后 AGV 仍不可控，下位机显示未使能。
- **根因**：机械臂 AUBO 连接成功后会**自动使能上电**（`MfmsCommandService.cpp` 里 `robotProxyAdapter_->enable(deviceId, true)`），AGV 的 `connectAgv` 既不使能也不更新 `device_state`。
- **修法**：参考机械臂逻辑，`connectAgv` 成功后写 `device_state='connected'`（进入可控态）、`disconnectAgv` 成功后回 `online`，对称处理（在主线程回调里操作 `db_`，线程安全）。

---

## 配套：接口全量升级

- **提交**：`7d7c695`
- 新代理 `agv.hpp` 依赖新版 `com_interfaces`（生成 `agv_control.hpp`），旧版 `src/com_interfaces` 编不过。
- 把 `src/com_interfaces` **全量升级**到下位机新交付的版本：新增 `AgvControl`/`AgvOrderState`/`SeerM4`/`VisionEngine`，`SeerCtrlCmdInterface` 增加 `control`/`nick_name` 字段；同步更新 `HyRMS_export` 代理头与预编译 `libhyrms_export.so`。
- 改动性质基本是**加字段**，唯一删除是 `AuboRobotState` 的 `int32 id`（改了引用它的 `CommunicationWorker.cpp:1702`）。
- 技术依据：交付的 `.so` 按新版全套接口编译，上位机若用旧版生成的消息结构，与 `.so` 交互时结构体布局对不上（ABI 不一致），故必须整体升级。

---

## 容器内响应速度实测

模拟下位机（`simulated_lower_machine -k seer`）+ rclpy 定时客户端，健康态单次 RPC 往返延迟（25 次采样）：

| 操作 | RPC 数 | 平均 | 最大 | 说明 |
| --- | --- | --- | --- | --- |
| 连接 connectAgv | 1 | 15.3ms | 21.8ms | +写库 connected 数毫秒 |
| 查看控制权 queryControl | 1 | 19.1ms | 24.1ms | checkControl(303) |
| 释放控制权 releaseControl | 1 | 19.5ms | 23.4ms | (302) |
| **接管即停** acquireControl | 4 | **79.0ms** | 82.8ms | check+acquire+cancel+stopManual 串行 |

App 端到端 ≈ RPC 延迟 + 几毫秒（7 层 Qt 排队信号是进程内亚毫秒）。健康态下均在 100ms 内。

> [!warning] 前提是健康态
> 下位机**卡死不应答**时，每次 RPC 撑满 8 秒超时（`SIMPLE_DEV_FUTURE_TIMEOUT`），接管最坏 ≈ `wait_for_service(1s) + 4×8s ≈ 33 秒`，期间 worker 线程 + adapter 锁被占。这个可用性隐患（review 的 M2）本轮未修，由后续一轮的**锁重构**解决，见 [[MFMS上位机Bug修复汇总-锁竞态与控制权]] 的 Bug 2。

---

## 分析但未在本轮修复的问题（已交后续）

1. **连接生命周期卡死**：下位机关闭时上位机连接卡死、重启后无法重连、必须重开程序。定位为四层状态（UI 标志 / Worker 状态 / 代理缓存 / DB device_state）在对端死亡时都不复位 + 机器人命令在 worker 线程同步阻塞。→ 后续锁重构 + 控制权状态同步已解决。
2. **控制权被外部夺走后 UI 不同步（40101）**：本轮只做了撞 40020 时复位，完整的轮询感知（指纹比对 / `ownedBySelf`）由后续完成。

---

## 验证方式（全部容器内，未碰真车）

- 接口 codegen、编译（3 包干净）、登录不崩、接管按钮渲染。
- **端到端回路**：点接管 → 确认框 → `acquireAgvControl` → 7 层 → 返回结果 → UI 弹框（容器无真车时返回"接管失败/未连接 AGV"，证明全链路通）。
- 段错误修复：停 db 后登录 90 秒不崩，走降级路径。
- 中文渲染：xdotool 驱动 + xwd 截图确认。

> [!tip] 容器内 UI 自动化手法
> `xdotool` 点击 + `xwd | convert` 截图。登录窗字段坐标约 838,507 / 838,583，登录按钮 889,726（桌面 1600×1000）。`QStringLiteral` 在二进制里是 UTF-16，验证符号用 `strings -el`。QMessageBox 在软件渲染 + VNC 下内容区会画成黑色（首绘 glitch），靠标题栏 + 键盘 `Alt+Y`/`Enter` 操作。

---

## 环境与红线备忘

- **三个数据库别搞混**：本机 Docker 内置 `db`（开发）/ 测试模拟库 `192.168.83.176`（hyrms/HyRMS0001）/ 生产库下位机 `192.168.83.74`（**测试绝不改**）。
- **绝不在下位机执行 `src/mfms_server/MFMS_BASE.sql`**（开头 DROP TABLE 会清生产数据）。
- 下位机 `DbConfig.h` 有故意保留的本地库指向改动，`git pull` 前先 `git stash`、拉后 `git stash pop`；构建**不要加 `--symlink-install`**。
- 本地未提交待定夺：`compose.yaml` 的 `127.0.0.1:3306:3306` 端口映射、优化后的 `bal.bash`。
- 下位机 git 远端 URL 曾内嵌明文 PAT（已多次泄露/失效），建议换 SSH deploy key。
