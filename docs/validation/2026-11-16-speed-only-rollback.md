# 回退到仅速度环验证

日期：2026-11-16。按用户要求暂存绝对位置环实验实现，并撤出绝对位置环和旧的相对位置环；该时点固件只保留速度环，后续已在其上加入差速与转向环。

## 范围

- 绝对位置环实现（`App/control/wheel_position.c/.h`、绝对命令、三种位置波形、位置参数和测试）已从工作区移出，备份见 `tmp/position-loop-backup/`，同时保存在 Git stash `absolute position loop WIP 2026-11-16`。
- 旧相对位置环的命令、状态、补偿逻辑、配置、测试和调试文档已删除。
- 保留速度环、开环限时命令、编码器、JY61P、夹爪按键和 UART8 调试。
- `speed_commands` 只保留 `left`、`right`、`both` 三种速度波形。
- `docs/validation/2026-11-15-wheel-position.md` 保留为历史记录，并标注相对位置环已撤出。

## 构建

- `stm32-debug` 预设构建通过：FLASH 71,508 B / RAM_D1 26,952 B，BIN SHA256 `57A23B28E6D35F7FF4C334BCDED87C5D6B951BF9BFCCC29354751A8E50237D2A`。
- `stm32-release` 预设构建通过：FLASH 77,156 B / RAM_D1 26,976 B，BIN SHA256 `57CAE8850C505A71584F5E634EA8FA3C21AA18A2A2233193CAB69BFA26FA0429`。
- 主机冒烟测试（`tmp/speed_smoke.c`，不进入仓库）：开环 100‰ 限时、速度目标 10 RPM 和停车均通过。

## 未验证项

本轮未烧录、未操作电机。速度环的实际跟随、停车和调参仍需上板验证；位置环实验代码未参与本次构建。
