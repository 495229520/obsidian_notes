---
tags:
  - 研一上学期/复合机器人开发-AMR底盘开发/计划
---
﻿# 周报：私有数据库访问环境部署

## 工作背景

本周完成了云服务器上的私有数据库访问环境建设。项目存在多名开发者协作需求，开发者能够远程访问同一套 MySQL 数据库。同时该项目为私密项目，数据库不能暴露到公网。
## 完成工作

### 1. 私有网络接入方案确认

- 确认使用 Tailscale 作为开发者访问数据库的私有网络入口。
- 云服务器已成功加入 Tailscale 网络。
- 服务器 Tailscale IPv4 地址为 `<TAILSCALE_PRIVATE_IP>`。
- 明确数据库访问链路为：开发者设备连接 Tailscale 后，通过 `<TAILSCALE_PRIVATE_IP>:3306` 访问 MySQL。
- 未修改现有 SSH、Sui、V2ray 网络配置。

### 2. MySQL 安装与配置

- 安装 MySQL Server 8.0.45。
- 配置 MySQL 开机自启。
- 新增专用 MySQL 配置文件 `/etc/my.cnf.d/90-mfms-tailscale.cnf`。
- 将 MySQL 主服务绑定到 Tailscale 私有 IP：

```ini
bind-address=<TAILSCALE_PRIVATE_IP>
port=3306
```

- 禁用 MySQL X Plugin，避免默认 `33060` 端口监听公网。
- 保留现有公网服务端口，不对 SSH、Sui、V2ray 做改动。

### 3. 数据库初始化

- 创建项目数据库 `MFMS_BASE`。
- 从 `/root/sql/MFMS_BASE.sql` 导入数据库结构和初始化内容。
- 导入完成后验证数据库存在。
- 当前 `MFMS_BASE` 数据库中共有 14 张表。

### 4. 数据库账号与权限

- 设置 MySQL `root@localhost` 密码，避免 root 空密码风险。
- 创建开发者专用账号 `<DB_DEV_USER>`。
- 限制开发账号来源为 Tailscale 地址段 `100.%`。
- 授权范围限制在 `MFMS_BASE` 数据库内，没有授予全局权限。
- 数据库凭据保存到 `<SERVER_CREDENTIAL_FILE>`，文件权限为 `600`，仅 root 可读。

### 5. 安全验证

- 验证 MySQL 服务状态为 `active`。
- 验证 MySQL 已设置开机自启。
- 验证 MySQL 只监听 `<TAILSCALE_PRIVATE_IP>:3306`。
- 验证没有监听 `0.0.0.0:3306`。
- 验证 MySQL X Plugin 的 `33060` 公网监听已关闭。
- 使用开发账号 `<DB_DEV_USER>` 通过 `<TAILSCALE_PRIVATE_IP>:3306` 成功连接数据库。
- 验证 `<DB_DEV_USER>` 权限仅限于 `MFMS_BASE` 数据库。

## 当前连接信息

开发者连接数据库前需要先连接 Tailscale，然后使用以下信息：

```text
Host: <TAILSCALE_PRIVATE_IP>
Port: 3306
Database: MFMS_BASE
Username: <DB_DEV_USER>
```

密码存放在服务器 `<SERVER_CREDENTIAL_FILE>`，不写入周报正文。

## 风险控制

- 未开启或修改 `firewalld`，避免误操作导致 SSH 断连。
- 数据库安全边界主要依赖 MySQL 仅绑定 Tailscale IP。
- 未开放 MySQL 公网监听端口。
- 未改动 Sui、V2ray、SSH 的运行端口和配置。
- 未将 root 账号提供给开发者使用，开发访问统一使用专用账号。

## 后续计划

- 在 Tailscale Admin Console 中配置 ACL，只允许开发者访问 `<TAILSCALE_PRIVATE_IP>:3306`。
- 后续计划为每位开发者创建独立 MySQL 账号，便于审计和权限回收。
- 计划建立数据库变更流程，所有结构变更保存为 migration SQL。
- 计划配置定期数据库备份，例如每日备份 `MFMS_BASE`。
