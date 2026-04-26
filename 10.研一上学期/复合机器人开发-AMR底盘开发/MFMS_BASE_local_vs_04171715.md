# 本机 `MFMS_BASE` 与 `MFMS_BASE_04171715.sql` 差异说明

## 比较对象

- 本机真实数据库
  - 主机: `127.0.0.1`
  - 端口: `3306`
  - 库名: `MFMS_BASE`
  - 查询时间: `2026-04-18`
- SQL 文件
  - 路径: `MFMS_BASE_04171715.sql`
  - 导出时间: `2026-04-17 17:15:32`

## 一句话结论

本机 `MFMS_BASE` 不是 `MFMS_BASE_04171715.sql` 的直接落地结果。

更准确地说，它像是一个“混合态”的库：

- 结构上继承了 `04171715` 这版的部分升级，尤其是 `lua_*` 状态机相关表
- 但 `device_state` 仍保留旧默认值
- 本机又额外新增了 `agv_path`、`agv_path_station` 两张表
- 设备配置、Lua 脚本、事件表内容和 `04171715.sql` 相差很大
- 一些日志表看起来几乎没动，另一些事件表则明显被清理过再继续写入

## 表集合对比

### 本机库比 SQL 文件多出的表

- `agv_path`
- `agv_path_station`

这两张表在本机都已经建好了，但当前都是 `0` 行数据。

### 共有表

两边共有以下 `13` 张表：

- `alterLogs`
- `device`
- `device_state`
- `device_state_event`
- `device_ui_event`
- `hyrms_log`
- `lua_script`
- `lua_state`
- `lua_state_event`
- `lua_ui_event`
- `trigger_table`
- `users`
- `这里记录了修改日志`

## 结构差异

### 1. 本机多了 AGV 路径相关结构

本机新增：

- `agv_path`
- `agv_path_station`

其中 `agv_path_station.path_id -> agv_path.id` 还带了外键约束，说明这不是临时表，而是已经落库的正式扩展。

### 2. `device_state` 仍然是旧默认值

- 本机 `device_state.state` 默认值: `unload`
- `MFMS_BASE_04171715.sql` 中 `device_state.state` 默认值: `offline`

这意味着本机库在设备状态初始化语义上，仍然偏旧版本。

### 3. `lua_*` 状态表已经是新版本结构

本机以下表结构与 `04171715.sql` 属于同一代：

- `lua_state`
- `lua_state_event`
- `lua_ui_event`

具体表现为：

- 状态枚举已扩展为 `wait/ready/running/pause/paused/resume/abort/aborted`
- `lua_state` 已包含 `reason` 和 `script_name`
- `lua_state_event.reason`、`lua_ui_event.reason` 已是 `text`

这说明本机库并不是纯旧库，而是至少已经吃到了 Lua 状态机的那次结构升级。

## 行数差异总览

| 表名 | 本机 `MFMS_BASE` | `MFMS_BASE_04171715.sql` | 结论 |
| --- | ---: | ---: | --- |
| `agv_path` | 0 | 不存在 | 本机新增表 |
| `agv_path_station` | 0 | 不存在 | 本机新增表 |
| `alterLogs` | 17 | 16 | 本机多 1 条登录记录 |
| `device` | 10 | 4 | 设备配置明显不同 |
| `device_state` | 10 | 4 | 本机保留了更多运行态设备 |
| `device_state_event` | 1 | 0 | 本机有新增事件 |
| `device_ui_event` | 21 | 281 | 本机只保留了很少一部分历史 |
| `hyrms_log` | 142 | 142 | 条数一致 |
| `lua_script` | 4 | 8 | 本机只保留前 4 个脚本 |
| `lua_state` | 3 | 7 | 本机只保留部分脚本状态 |
| `lua_state_event` | 1 | 0 | 本机有新增状态流转 |
| `lua_ui_event` | 7 | 569 | 本机只保留很少一部分历史 |
| `trigger_table` | 0 | 0 | 一致 |
| `users` | 0 | 0 | 一致 |
| `这里记录了修改日志` | 6 | 6 | 内容一致 |

## 关键差异说明

### 1. 设备清单完全不是同一套

`MFMS_BASE_04171715.sql` 中的设备只有 4 个：

- `agvSrc0001`
- `plcSie0001`
- `robFro0001`
- `vitDev0001`

本机 `MFMS_BASE.device` 中有 10 个设备：

- `aaafra9001`
- `aaafra9002`
- `agvser0001`
- `agvser9001`
- `ctrkos0001`
- `rbafra0001`
- `rbafra0002`
- `rbthsu0001`
- `robFro0001`
- `vitDev0001`

可以直接确认的差异：

- SQL 文件里的 `agvSrc0001`、`plcSie0001`，本机根本不存在
- 本机保留了很多更早命名风格的设备，如 `ctrkos0001`、`rbafra0001`、`rbthsu0001`
- `robFro0001` 两边都存在，但地址不同
  - SQL 文件: `192.168.58.3`
  - 本机: `192.168.58.2`

这说明本机设备配置不是从 `04171715.sql` 原样恢复出来的。

### 2. `device_state` 更像真实运行态，而不是文件里的初始化态

`MFMS_BASE_04171715.sql` 中：

- 只有 `4` 条 `device_state`
- 全部是 `online`
- `info` 基本是空 JSON

本机中：

- 有 `10` 条 `device_state`
- 状态混杂为 `connected`、`offline`、`load`
- 大部分 `info` 长度明显大于空 JSON，说明已经写入真实运行态信息

这说明本机库保存的是某个实际联调/运行现场，而不是 `04171715.sql` 那种简化初始化状态。

### 3. `lua_script` 被裁剪成了前 4 条

`MFMS_BASE_04171715.sql` 中共有 8 条脚本：

- `1 lua脚本使用指南`
- `2 !!!危险!!!`
- `3 virt_test`
- `4 while_test`
- `5 robot_test`
- `6 test`
- `7 plc_test`
- `8 force_sample`

本机只剩前 4 条：

- `1 lua脚本使用指南`
- `2 !!!危险!!!`
- `3 virt_test`
- `4 while_test`

也就是说：

- `robot_test`
- `test`
- `plc_test`
- `force_sample`

这 4 条在本机已不存在。

### 4. `lua_state` 也只保留了部分脚本状态

`MFMS_BASE_04171715.sql` 中 `lua_state` 有 7 条，覆盖脚本 `2` 到 `8`。

本机只有 3 条：

- `2 / wait / NULL`
- `3 / aborted / virt_test`
- `4 / aborted / while_test`

这进一步说明本机 Lua 相关数据不是完整继承自 `04171715.sql`，而是发生过裁剪或重建。

### 5. 事件表不是“继续增长”，而更像“清理后重新写入”

这是本次对比里最值得注意的一点。

#### `device_ui_event`

- SQL 文件: `281` 条，ID 范围 `35` 到 `315`
- 本机: `21` 条，ID 范围 `35` 到 `109`
- 本机时间范围: `2026-01-27 20:17:40` 到 `2026-04-17 23:02:35`
- SQL 文件时间范围: `2026-01-20 21:41:39` 到 `2026-04-17 17:11:28`

本机最后几条还是 `2026-04-17` 的新事件，例如：

- `agvser9001 connected -> offline`
- `agvser9001 offline -> online`

但它只剩 21 条记录，远少于 SQL 文件的 281 条。

这说明更像是：

- 旧历史被删除了大部分
- 删除后又继续产生了新事件

#### `lua_ui_event`

- SQL 文件: `569` 条，ID 范围 `281` 到 `849`
- 本机: `7` 条，ID 范围 `281` 到 `287`
- 本机时间范围: `2026-01-27 20:17:39` 到 `2026-04-17 19:52:34`
- SQL 文件时间范围: `2026-01-20 21:41:38` 到 `2026-04-17 17:13:18`

本机保留的只是很小的一段残余历史，但时间上又晚于 SQL 文件。

这同样更像“被清理过，再继续写入”，而不是“在 `04171715.sql` 基础上自然累积”。

#### `device_state_event` 和 `lua_state_event`

- SQL 文件两张表都是 `0` 条
- 本机分别有 `1` 条和 `1` 条

本机实际记录为：

- `device_state_event`: `agvser0001 offline -> load`，时间 `2026-04-17 22:58:48`
- `lua_state_event`: `script_id=2 running -> ready`，时间 `2026-04-17 19:52:31`

说明本机在 SQL 文件导出之后，确实还有新的状态事件写入。

### 6. `alterLogs` 比 SQL 文件多 1 条

`MFMS_BASE_04171715.sql` 最后一条登录日志是：

- `2026-01-19 20:10:03`

本机 `alterLogs` 多出一条：

- `2026-01-24 15:34:59`

这个差异很小，但能说明本机并不完全等于文件快照。

### 7. `这里记录了修改日志` 看起来是完全一致的

两边都是 `6` 条，并且都包含这条关键记录：

- `2026-01-16 00:58:52`
- `修改lua_state，新增reason字段，反映lua报错情况`

这说明本机和 `04171715.sql` 至少在“Lua 状态字段升级”这件事上是一致的。

### 8. `hyrms_log` 大概率与 SQL 文件一致

可以确认的事实：

- 两边行数都是 `142`
- 本机尾部 `id=141..145` 的记录与 `MFMS_BASE_04171715.sql` 尾部记录一致

基于这两点，可以推断：

- `hyrms_log` 很可能没有像 `device_ui_event` / `lua_ui_event` 那样被大幅清理

这里是“高概率推断”，不是逐行校验后的绝对结论。

## 结论判断

如果只用一句话概括：

本机 `MFMS_BASE` 更像是“旧设备配置 + 新 Lua 状态结构 + 新增 AGV 路径表 + 被部分清理过的事件历史”的混合数据库，而不是 `MFMS_BASE_04171715.sql` 的原样恢复库。

更具体一点，本机库很可能经历过以下过程中的一部分：

- 先基于较早版本的库或初始化数据建库
- 后续同步了 `lua_state` 相关结构升级
- 又新增了 `agv_path` / `agv_path_station`
- 设备表被重新配置过
- `device_ui_event` / `lua_ui_event` 等事件表被清理过一部分
- 清理后系统继续运行，所以又产生了新的事件记录

上面这段是依据当前数据形态做出的推断，但和现有证据是吻合的。
