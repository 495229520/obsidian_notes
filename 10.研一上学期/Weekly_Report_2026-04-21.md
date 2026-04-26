# MFMS 数据中台阶段周报

> 日期: 2026-04-21
> 对比范围: 初版 `0716051` -> 当前版 `83138c0`
> 本周重点范围: `58f80da` -> `83138c0`
> 参考文档: `src/mfms_server/design/MFMS_DataCenter_Architecture.md`

## 1. 本周结论

当前版本相对最早版本，已经从“单个模拟下位机工具”演进为“可承载 Qt 前端、ROS 状态桥接、数据库事件驱动、命令服务与下位机代理”的完整数据中台雏形。

如果用一句话概括本阶段变化：

> 初版主要解决“能不能模拟设备并打通基础事件流”，当前版主要解决“能不能把设备接入、状态订阅、命令下发、路径资源管理和 UI 接口统一收敛到一个中台架构里”。

## 2. 当前版与最早版本的核心区别

| 维度 | 最早版本 `0716051` | 当前版本 `83138c0` |
|------|--------------------|--------------------|
| 仓库形态 | `src/` 下只有 `mfms_server` 包 | `src/` 下已有 `com_interfaces`、`inspection_robot`、`mfms_fr_adapter`、`mfms_server`、`qt_file` 共 5 个包 |
| `mfms_server` 实体代码 | 仅 3 个文件：`CMakeLists.txt`、`tools/README.md`、`tools/simulated_lower_machine.cpp` | 已扩展到 51 个文件，覆盖 `client_api`、`gateway`、`cmd_service`、`mfms_db`、`ros_bridge`、`tests`、`design` |
| 架构成熟度 | `CMakeLists.txt` 已预留分层目标，但实际落地代码仍以模拟器为主 | 已形成 `Qt -> CommunicationInterface -> Worker -> Gateway -> Db/Ros/Cmd -> Proxy` 的分层链路 |
| 设备接入模式 | 主要靠 `simulated_lower_machine` 提供 ROS Service/Topic + DB 事件模拟 | 机械臂与 AGV 已通过 `RobotProxyAdapter`、`AgvProxyAdapter` 对接 `hyrms_export` 下位机代理 |
| 数据中台能力 | 只有局部的设备/脚本事件模拟，没有完整中台 API | 已具备设备列表刷新、连接/断开、机械臂点动、AGV 站点导航、路径资源管理等上层接口 |
| 数据库能力 | 仅支持模拟场景下的数据写入/轮询 | 已补齐 `MFMS_BASE.sql`、`MFMS_BASE_04171715.sql`、触发器联动、AGV 路径资源表设计 |
| Qt 集成 | 无完整前端联动层 | `qt_file` 已接入 `CommunicationInterface` / `MfmsGateway` 相关控制链 |
| 文档与测试 | 仅基础工具说明 | 已新增架构文档、调用链文档、设计说明、联调报告、路径接口测试等 |

## 3. 量化变化

### 3.1 从初版到当前版

- 全仓库差异: `384 files changed, 88101 insertions(+), 221 deletions(-)`
- 其中 `src/mfms_server` 差异: `50 files changed, 21028 insertions(+), 221 deletions(-)`
- `src/mfms_server` 文件数: `3 -> 51`
- `src/` 包数量: `1 -> 5`

说明:

- 全仓库增量中包含三方库、模型、图片、UI 资源和导出头文件，不能把 8.8 万行都理解成纯业务逻辑。
- 真正和数据中台直接相关的主体增量，主要集中在 `src/mfms_server`、`src/com_interfaces`、`src/qt_file`。

### 3.2 本周范围 `58f80da -> 83138c0`

- 全仓库差异: `25 files changed, 4830 insertions(+), 394 deletions(-)`
- 其中 `src/mfms_server` 差异: `19 files changed, 4725 insertions(+), 391 deletions(-)`

本周变更基本集中在数据中台主包，说明本周工作重心已经从“外围包铺设”转向“中台能力补齐”。

## 4. 本周主要完成内容

### 4.1 补齐 AGV 代理接入链

本周新增:

- `src/mfms_server/cmd_service/include/cmd_service/AgvProxyAdapter.h`
- `src/mfms_server/cmd_service/src/AgvProxyAdapter.cpp`

这意味着 AGV 已不再只是消息层定义或模拟对象，而是和机械臂一样，进入了统一命令服务框架：

- `MfmsCommandService`
- `MfmsGatewayImpl`
- `CommunicationWorker`
- `CommunicationInterface`

一起构成了可从 Qt 直接下沉到 `hyrms_export::SeerCtrl` 的真实调用链。

### 4.2 补齐路径资源管理能力

结合 `MFMS_DataCenter_Architecture.md` 与 `CommunicationInterface_Current_CallChain.md`，当前版本已经把路径相关能力从“概念预留”推进到“中台内部可用”：

- 补齐 `getPaths()`
- 补齐 `addPath()`
- 补齐 `exeToPath()`
- 新增 `src/mfms_server/tests/test_path_interfaces.cpp`

这部分工作的关键点不是给 AGV 另起一套新协议，而是：

- 路径作为中台资源写入数据库
- 执行路径时先查库得到站点序列
- 再复用已有 AGV 多站导航链路执行

这符合数据中台的定位，也降低了对下位机协议扩展的依赖。

### 4.3 完善通信接口与线程边界

本周修改了以下核心文件：

- `src/mfms_server/client_api/include/mfms_server/CommunicationInterface.h`
- `src/mfms_server/client_api/include/mfms_server/CommunicationInterfaceImpl.h`
- `src/mfms_server/client_api/src/CommunicationInterface.cpp`
- `src/mfms_server/client_api/src/CommunicationInterfaceImpl.cpp`
- `src/mfms_server/client_api/src/CommunicationWorker.cpp`
- `src/mfms_server/client_api/src/CommunicationWorker.h`

从架构上看，这些修改的意义在于：

- 继续维持 Qt 主线程与通信线程隔离
- 把新增 AGV/路径接口纳入统一的 `QueuedConnection` 调度
- 明确“当前选中设备”上下文，避免路径执行静默回退到错误设备

这使当前系统更接近真正可维护的中台，而不是 UI 直接拼接后端调用。

### 4.4 完善 Gateway 门面与命令服务编排

本周同时修改了：

- `src/mfms_server/gateway/include/mfms_server/MfmsGateway.h`
- `src/mfms_server/gateway/include/mfms_server/MfmsGatewayImpl.h`
- `src/mfms_server/gateway/src/MfmsGatewayImpl.cpp`
- `src/mfms_server/cmd_service/include/cmd_service/MfmsCommandService.h`
- `src/mfms_server/cmd_service/src/MfmsCommandService.cpp`

说明当前版本已经不再只是“能调用某个设备方法”，而是在做更清晰的职责收敛：

- `Gateway` 负责统一入口与跨模块协调
- `CommandService` 负责命令编排和代理适配
- `Worker` 负责线程内状态和调用转发

这正是 `MFMS_DataCenter_Architecture.md` 中描述的门面模式和分层职责落地。

### 4.5 数据库与文档同步推进

本周同步更新了：

- `MFMS_BASE_04171715.sql`
- `src/mfms_server/design/MFMS_DataCenter_Architecture.md`
- `src/mfms_server/design/CommunicationInterface_Current_CallChain.md`
- `src/mfms_server/design/DataPlatform_Structure_and_TopLevel_CallChain.md`

这说明当前阶段不是只补代码，而是在同步固化三类资产：

- 数据模型
- 调用链认知
- 架构说明

这一点对后续多人协作、前后端联调和测试回归都很重要。

## 5. 从架构角度看，当前版比初版提升了什么

结合参考文档，当前版已经具备以下初版不具备的能力：

### 5.1 从“单点模拟”升级到“完整链路”

初版的核心价值在于模拟下位机、验证 ROS/DB 事件流。

当前版则已经形成完整闭环：

- Qt 发起控制请求
- `CommunicationInterfaceImpl` 将请求排入通信线程
- `CommunicationWorker` 维护当前设备上下文并调用 `Gateway`
- `MfmsGatewayImpl` 协调 `DbService`、`RosBridge`、`CommandService`
- `CommandService` 经由 `RobotProxyAdapter` / `AgvProxyAdapter` 对接 `hyrms_export`
- `RosBridge` 负责实时状态回流
- 数据库负责资源与事件状态持久化

### 5.2 从“设备控制”升级到“资源管理”

初版更多关注设备是否能连、命令是否能发。

当前版开始引入“路径”这一中台资源，意味着系统开始从“设备控制层”向“业务资源管理层”提升，这也是数据中台和普通设备驱动层的本质区别。

### 5.3 从“代码存在”升级到“可说明、可测试、可维护”

当前版已经具备：

- 设计文档
- 当前调用链文档
- 集成测试与路径接口测试
- 前端入口与中台接口的映射关系

这说明项目正在从“能跑”转向“能维护、能协作、能持续扩展”。

## 6. 当前仍然存在的阶段性问题

虽然当前版相较初版已经提升明显，但从中台成型角度看，仍有几个问题需要后续继续推进：

- 路径保存前的站点合法性校验仍需进一步完善，避免脏路径配置进入数据库
- AGV 手动运动接口仍受下位机协议能力约束，部分接口仍属于占位或受限状态
- 数据库路径资源目前按 `device_id` 独占，后续是否需要按 `group_id` 共享，还需要结合业务场景确认
- 当前架构文档和代码已经基本同步，但随着接口继续增加，文档需要持续维护，否则很快失真

## 7. 下周建议

- 继续补齐路径管理的异常处理与站点校验
- 把 `qt_file` 中与 AGV 路径相关的交互进一步收口到统一接口
- 针对 AGV 站点查询、路径执行、失败回滚补充更完整的回归测试
- 评估是否需要把路径资源从“设备级”提升为“组级/产线级”资源模型

## 8. 阶段总结

从提交历史看，仓库经历了 4 个清晰阶段：

1. `0716051`: 初版，只有模拟下位机工具，验证基础交互模型
2. `cfad9f8`: 将各模块整合进 `mfms_server`，形成中台骨架
3. `58f80da`: 接入下位机接口前的稳定版，完成主要模块铺设
4. `83138c0`: 当前版，补齐 AGV 代理链、路径资源能力与相关文档测试

因此，当前版和最早版本的本质区别不是“多了几个接口”，而是项目已经从一个偏原型性质的验证仓库，演进成了一个具备数据中台分层架构、设备代理接入能力、数据库资源管理能力和 Qt 联动入口的工业控制平台基础版本。
