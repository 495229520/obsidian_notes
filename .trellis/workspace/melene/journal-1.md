# Journal - melene (Part 1)

> AI development session journal
> Started: 2026-08-21

---



## Session 1: Ubuntu WSL 安装与资源配置记录

**Date**: 2026-10-23
**Task**: Ubuntu WSL 安装与资源配置记录
**Branch**: `main`

### Summary

2026-10-23 已安装 Ubuntu 24.04 LTS（发行版 Ubuntu-24.04），使用 WSL 2；虚拟磁盘 D:/WSL/Ubuntu/ext4.vhdx，默认登录用户 ubuntu。

### Main Changes

- 全局资源上限：memory=32GB、processors=12、swap=8GB，swapFile=D:/WSL/swap.vhdx；配置文件 C:/Users/Administrator/.wslconfig。
- Ubuntu-24.04 已设为默认发行版；保留 C 盘 docker-desktop 及其数据，用户已取消删除。
- Ubuntu 内 /home/ubuntu 等 Linux 目录的文件占用 D 盘虚拟磁盘；/mnt/c 写入 C 盘；LLVM/MLIR 源码与构建应放 Linux 文件系统。
- 用户报告已设置登录密码；记录中不保存密码明文。本次未修改或验证密码。

### Git Commits

(No commits - planning session)

### Testing

- [OK] 安装镜像 SHA-256 校验通过；注册信息确认 BasePath=D:/WSL/Ubuntu，DefaultUid=1000。
- [OK] Ubuntu 内验证 whoami=ubuntu、nproc=12、MemTotal=32867404kB、SwapTotal=8388608kB；根文件系统容量约 1TB，动态增长。

### Status

[OK] **Completed**

### Next Steps

- 启动时仍提示 NAT 模式无法镜像 Windows localhost 代理；如后续联网受影响，再单独处理。
