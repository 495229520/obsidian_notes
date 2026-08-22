# 学习 Trellis 初始化

## Goal

在当前 Obsidian vault 中建立一个独立的学习 Trellis，并将其任务以只读方式投影到 Obsidian Task Board。

## Acceptance Criteria

- [x] vault 根目录存在 `.trellis/`
- [x] 后续学习安排以 `.trellis/tasks/` 为权威来源
- [x] Obsidian 中存在独立的学习任务总览
- [x] Trellis 生命周期变化会刷新 Obsidian 数据源

## Notes

- Obsidian 看板是只读镜像；Task 状态通过 Trellis 生命周期命令更新。
- 学习 Task 使用父任务统筹周期，子任务承载每日执行清单。
