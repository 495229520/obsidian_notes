---
tags:
  - 研一上学期/复合机器人开发-AMR底盘开发/未命名
---
#   AGV 手动运动与路径行驶测试文档
  
文档日期: 2026-05-19  
  
适用对象: 测试工程师、Qt 上位机开发者  
  
## 1. 测试目的  
  
本次修复覆盖两个问题:  
  
1. 小车运动参数输入不同的速度、距离、转动角度、转动速度后，点击前进、后退、左转、右转，实际移动距离和旋转角度应随输入变化。  
2. 多个站点添加到路径后，点击行路，系统应把完整站点列表按顺序下发给 HyRMS，不应只下发或只执行最后一个站点。  
  
## 2. 当前实现说明  
  
### 2.1 手动运动  
  
Qt 侧输入控件应使用 spinbox 的 `value()` 读取数值，不应使用 `text().toFloat()` 或 `text().toDouble()`。  
  
涉及控件:  
  
| 控件 | 含义 | 建议范围 |  
|------|------|----------|  
| `AGV_MoveVelocity` | 直线速度, m/s | 0.01 到 2.0 |  
| `AGV_MoveDistance` | 直线距离, m | 0.01 到 10.0 |  
| `AGV_TurnAngle` | 转动角度, deg | 1 到 360 |  
| `AGV_TurnVelocity` | 转动速度, deg/s | 1.0 到 90.0 |  
  
数据中台会把 UI 参数转换为 HyRMS 手动控制参数:  
  
| 操作 | x | y | w | duration |  
|------|---|---|---|----------|  
| 前进 | `+speed` | 0 | 0 | `distance / speed * 1000` |  
| 后退 | `-speed` | 0 | 0 | `distance / speed * 1000` |  
| 左转 | 0 | 0 | `+angularSpeedRad` | `angle / angularSpeed * 1000` |  
| 右转 | 0 | 0 | `-angularSpeedRad` | `angle / angularSpeed * 1000` |  
  
### 2.2 路径行驶  
  
路径行驶使用 HyRMS 的列表导航接口:  
  
```cpp  
SeerCtrl::guideGoTargetList(std::vector<std::string> station_list)  
```  
  
该接口语义:  
  
1. `station_list` 按列表顺序执行。  
2. 返回 `0` 表示 HyRMS 已接收命令。  
3. 接口是异步执行，不表示整条路径已经完成。  
4. 执行状态通过 AGV 状态、`checkGuide()`、现场视频或下位机日志观察。  
  
数据中台路径链路:  
  
```text  
Qt 点击行路  
-> CommunicationInterface::exeToPath(pathName)  
-> MfmsCommandService::executeAgvToPath(deviceId, pathName)  
-> 查询 agv_path/agv_path_station  
-> dispatchAgvStationListNavigation(deviceId, stationList)  
-> AgvProxyAdapter::navigateToStationList(deviceId, stationList)  
-> HyRMS guideGoTargetList(stationList)  
```  
  
## 3. 测试环境要求  
  
1. 使用最新 `main` 分支代码构建系统。  
2. 已替换新的 HyRMS 设备包和新的 `com_interfaces`。  
3. AGV 可连接，或使用支持站点列表导航的下位机/仿真环境。  
4. 数据库存在 `agv_path`、`agv_path_station` 表。  
5. AGV 至少有 3 个可到达站点，例如 `ST_00`、`ST_01`、`ST_02`。  
6. 测试区域安全，首次测试建议距离不超过 1 m，角度不超过 90 deg。  
  
## 4. 构建与接口冒烟测试  
  
构建:  
  
```bash  
source /opt/ros/humble/setup.bash  
colcon build --packages-select com_interfaces --cmake-clean-cache --event-handlers console_direct+  
source install/setup.bash  
colcon build --packages-select mfms_server qt_file --event-handlers console_direct+  
```  
  
路径接口测试:  
  
```bash  
source /opt/ros/humble/setup.bash  
source install/setup.bash  
export MFMS_DB_HOST=localhost  
export MFMS_DB_PORT=3306  
export MFMS_DB_USER=mfms_test  
export MFMS_DB_PASSWORD=123  
./build/mfms_server/test_path_interfaces  
```  
  
期望输出包含:  
  
```text  
[test_path_interfaces] All checks passed  
```  
  
说明: 接口测试只能证明参数链路和数据库链路正确，不能替代真实 AGV 行驶测试。  
  
## 5. 手动运动测试用例  
  
### AGV-MOVE-001 前进距离随输入变化  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 设置速度 `0.2 m/s`，距离 `0.2 m`，点击前进 | 小车前进约 0.2 m |  
| 2 | 设置速度 `0.2 m/s`，距离 `0.8 m`，点击前进 | 小车前进约 0.8 m |  
| 3 | 比较两次结果 | 第二次位移明显大于第一次，不应相同 |  
  
### AGV-MOVE-002 后退距离随输入变化  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 设置速度 `0.2 m/s`，距离 `0.2 m`，点击后退 | 小车后退约 0.2 m |  
| 2 | 设置速度 `0.2 m/s`，距离 `0.6 m`，点击后退 | 小车后退约 0.6 m |  
| 3 | 比较两次结果 | 第二次位移明显大于第一次，不应相同 |  
  
### AGV-MOVE-003 同距离不同速度影响运动时长  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 设置速度 `0.1 m/s`，距离 `0.4 m`，点击前进 | 运动约 4 秒 |  
| 2 | 设置速度 `0.4 m/s`，距离 `0.4 m`，点击前进 | 运动约 1 秒 |  
| 3 | 比较两次结果 | 位移接近，第二次耗时明显更短 |  
  
### AGV-ROT-001 左转角度随输入变化  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 设置转速 `10 deg/s`，角度 `10 deg`，点击左转 | 小车左转约 10 deg |  
| 2 | 设置转速 `10 deg/s`，角度 `60 deg`，点击左转 | 小车左转约 60 deg |  
| 3 | 比较两次结果 | 第二次角度明显大于第一次，不应相同 |  
  
### AGV-ROT-002 右转角度随输入变化  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 设置转速 `15 deg/s`，角度 `15 deg`，点击右转 | 小车右转约 15 deg |  
| 2 | 设置转速 `15 deg/s`，角度 `90 deg`，点击右转 | 小车右转约 90 deg |  
| 3 | 比较两次结果 | 第二次角度明显大于第一次，不应相同 |  
  
### AGV-MOVE-NEG-001 异常参数  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 尝试输入 0 或负数速度/距离 | UI 不允许输入，或后端返回参数无效 |  
| 2 | 尝试输入 0 或负数转速/角度 | UI 不允许输入，或后端返回参数无效 |  
| 3 | 未连接 AGV 时点击运动按钮 | 提示未连接或代理不可用，程序不崩溃 |  
  
## 6. 路径行驶测试用例  
  
### AGV-PATH-001 保存多站点路径  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 新建路径，路径名输入 `QA_ROUTE_001` | 路径名显示正确 |  
| 2 | 按顺序添加 `ST_00`、`ST_01`、`ST_02` | 路径站点列表按添加顺序显示 |  
| 3 | 点击保存或更新路径 | 提示保存成功 |  
| 4 | 重新选择 `QA_ROUTE_001` | 路径仍显示 `ST_00 -> ST_01 -> ST_02` |  
  
### AGV-PATH-002 行路下发完整站点列表  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 选择 `QA_ROUTE_001` | UI 显示完整路径站点 |  
| 2 | 点击行路 | UI 提示路径导航命令已接收或已发送 |  
| 3 | 查看数据中台日志 | 日志包含完整站点列表，不只包含最后一个站点 |  
| 4 | 查看 HyRMS/下位机日志 | `station_list` 顺序为 `ST_00, ST_01, ST_02` |  
| 5 | 观察 AGV | AGV 按列表顺序执行，不应只直接去 `ST_02` |  
  
判定失败条件:  
  
1. 数据中台日志只出现最后一个站点。  
2. HyRMS 收到的 `station_list` 只有最后一个站点。  
3. AGV 路径执行完全跳过前序站点，直接执行最后站点。  
  
### AGV-PATH-003 不同路径执行不同站点序列  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 保存 `QA_ROUTE_A = ST_00 -> ST_01` | 保存成功 |  
| 2 | 保存 `QA_ROUTE_B = ST_02 -> ST_01 -> ST_00` | 保存成功 |  
| 3 | 执行 `QA_ROUTE_A` | HyRMS 收到 `ST_00, ST_01` |  
| 4 | 执行 `QA_ROUTE_B` | HyRMS 收到 `ST_02, ST_01, ST_00` |  
| 5 | 比较两次日志 | 两次 station_list 不同，系统不复用旧路径 |  
  
### AGV-PATH-004 路径编辑后执行  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 选择 `QA_ROUTE_001` | 路径列表加载 |  
| 2 | 删除中间站点 `ST_01` | UI 列表变为 `ST_00 -> ST_02` |  
| 3 | 保存或更新路径 | 提示成功 |  
| 4 | 再次点击行路 | HyRMS 收到 `ST_00, ST_02`，不再包含 `ST_01` |  
  
### AGV-PATH-NEG-001 路径异常输入  
  
| 步骤 | 操作 | 期望结果 |  
|------|------|----------|  
| 1 | 路径名称为空时保存 | 提示路径名称不能为空 |  
| 2 | 路径中没有站点时保存 | 提示路径站点列表不能为空 |  
| 3 | 不选择路径直接点击行路 | 提示需要选择路径 |  
| 4 | 保存同名路径 | 提示路径已存在 |  
| 5 | AGV 未连接时点击行路 | 提示 AGV 代理未连接 |  
  
## 7. Qt 开发者接入说明  
  
### 7.1 手动运动参数读取  
  
Qt 开发者应从 spinbox 读取数值:  
  
```cpp  
ui->AGV_MoveVelocity->value();  
ui->AGV_MoveDistance->value();  
ui->AGV_TurnAngle->value();  
ui->AGV_TurnVelocity->value();  
```  
  
不要使用:  
  
```cpp  
ui->AGV_MoveVelocity->text().toDouble();  
ui->AGV_MoveDistance->text().toDouble();  
```  
  
原因: `text()` 可能受 locale、小数分隔符、前后缀影响；`value()` 直接返回数值，更稳定。  
  
### 7.2 路径行驶调用方式  
  
Qt 侧行路按钮应调用路径接口，而不是自己取最后一个站点调用到点接口。  
  
推荐调用:  
  
```cpp  
communicationInterface->exeToPath(pathName);  
```  
  
或在已有封装中保持等价链路:  
  
```text  
行路按钮  
-> 当前选中路径名  
-> exeToPath(pathName)  
-> 数据中台查询路径站点  
-> HyRMS guideGoTargetList(stationList)  
```  
  
不要这样做:  
  
```text  
从路径列表取最后一项  
-> exeToStation(lastStation)  
```  
  
这会复现“只能运动到最后站点”的问题。  
  
### 7.3 UI 显示建议  
  
因为 HyRMS `guideGoTargetList()` 是异步接口，返回 `0` 只表示“命令已接收”。Qt UI 文案建议使用:  
  
```text  
路径导航命令已接收  
```  
  
不要显示:  
  
```text  
路径已全部到位  
```  
  
是否到位应通过 AGV 状态、`checkGuide()` 状态、站点状态或现场观察确认。  
  
### 7.4 测试时必须保留顺序  
  
Qt 保存路径时必须保留用户添加站点的顺序。传给数据中台的路径站点列表应与 UI 列表一致:  
  
```text  
UI: ST_00 -> ST_01 -> ST_02  
DB: station_index = 0, 1, 2  
HyRMS: guideGoTargetList(["ST_00", "ST_01", "ST_02"])  
```  
  
## 8. 验收标准  
  
全部满足时判定通过:  
  
1. 前进、后退距离随输入距离变化。  
2. 左转、右转角度随输入角度变化。  
3. 同距离不同速度时，运动时长随速度变化。  
4. 路径保存后重新加载，站点顺序不丢失。  
5. 点击行路后，数据中台向 HyRMS 下发完整 `station_list`。  
6. HyRMS 收到的 `station_list` 与 UI 路径列表顺序一致。  
7. AGV 不再只运动到最后一个站点。  
8. 异常输入有提示，程序不崩溃，AGV 不执行危险动作。  
  
## 9. 测试记录模板  
  
| 用例编号 | 测试时间 | 测试环境 | 输入参数/路径 | 初始状态 | 最终状态 | 日志/视频 | 结果 | 备注 |  
|----------|----------|----------|---------------|----------|----------|-----------|------|------|  
| AGV-MOVE-001 | | | | | | | 通过/失败 | |  
| AGV-MOVE-002 | | | | | | | 通过/失败 | |  
| AGV-MOVE-003 | | | | | | | 通过/失败 | |  
| AGV-ROT-001 | | | | | | | 通过/失败 | |  
| AGV-ROT-002 | | | | | | | 通过/失败 | |  
| AGV-PATH-001 | | | | | | | 通过/失败 | |  
| AGV-PATH-002 | | | | | | | 通过/失败 | |  
| AGV-PATH-003 | | | | | | | 通过/失败 | |  
| AGV-PATH-004 | | | | | | | 通过/失败 | |  
| AGV-PATH-NEG-001 | | | | | | | 通过/失败 | |