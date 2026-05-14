# AGV 真车联调注意事项

切换到真实 AGV 时，核心原则是：中台只连接真车服务，不再启动任何模拟下位机。

## 1. 停止启动模拟下位机

不需要运行模拟下位机命令，例如：

```bash
ros2 run mfms_server simulated_lower_machine --kind seer ...
```

同时检查启动脚本，避免继续调用任何包含 `simulated_lower_machine` 的命令。

## 2. 将数据库中的 AGV IP 改为真车 IP

当前 `MFMS_BASE.sql` 中，`agvSrc0001` 的 IP 为 `127.0.0.1`，这是模拟环境使用的地址。联调真车前，需要改成真实 AGV 的 IP：

```sql
UPDATE device
SET ip = '真实AGV_IP'
WHERE id = 'agvSrc0001';
```

建议同时重置设备状态，避免残留的模拟状态影响联调判断：

```sql
UPDATE device_state
SET state = 'offline',
    info = JSON_OBJECT(),
    err_code = NULL
WHERE id = 'agvSrc0001';
```

## 3. 确认真车仍使用 `agvSrc0001`

代码会根据设备 ID 判断设备类型：

- `agvSrc0001` 会被识别为 Seer AGV。
- 上位机点击连接后，会进入 `connectAgv()` 流程。
- 中台会通过 `HyRMS_export` 中的 `AgvProxy::SeerCtrl` 连接真实 AGV 服务。

因此，如果真车的设备 ID 发生变化，需要同步检查数据库配置和上位机连接逻辑。

## 4. 启动真实下位机 / HyRMS 服务

真 AGV 侧需要提供以下能力：

- `com_interfaces/srv/SeerCtrlCmdInterface` 服务，用于接收控制指令。
- `SeerCtrlState` 状态发布，用于向中台同步车辆状态。

也就是说，`HyRMS_export` 代理库需要能找到真车对应的服务，而不是继续连接模拟器提供的服务。
