# 2026-10-14 C/D 上电测试固件烧录

用户重新插好烧录器后要求继续烧录。本次使用 `develop` 分支、基线 `1c0aff8d5d2d2c618f1313fc3b32a09f616d9b09` 上尚未提交的 C/D 接线修改与临时上电测试，未再修改固件代码，未提交或推送。

接线为 **D 左轮、C 右轮**：左轮 PB14 / TIM12_CH1、PD2/PD1 方向、PC6/PC7 / TIM8 编码器；右轮 PB15 / TIM12_CH2、PD3/PD4 方向、PD12/PD13 / TIM4 编码器。代码及离线验证见[C/D 接线同步记录](2026-10-13-cd-wheel-mapping.md)。

## 烧录和回读

- `stm32-debug` 增量构建通过；固件源码、BIN/ELF/HEX 与 2026-10-13 已验证快照一致。
- 实时识别无线 DAPLink：`FAED:4873`、序列号 `3FEE83BB`、串口 `COM5`；SWD 100 kHz 连接恢复。核心板 IDCODE 为 `0x20036450`，UID 为 `0x0042001e 0x33335115 0x39333631`，Flash 为 128 KiB。
- 写入前串口确认旧 A/B 固件双轮零输出，随后暂停 CPU 并完整备份 Flash；备份与最近烧录的 A/B 上电测试版一致。
- 烧录 C/D Debug BIN 到 `0x08000000`，OpenOCD 校验后完整回读 128 KiB。前 61616 字节与新 BIN 完全一致，剩余区域均为 `0xFF`；确认一致后才复位运行。

| 产物 | 字节数 | SHA256 |
| --- | ---: | --- |
| C/D 测试 Debug BIN | 61616 | `2249e4d31a10e1b47ac0f0a56168fe286d4a148b7da9487b9b2f057925fc0fd7` |
| 烧录前完整备份 | 131072 | `d3edeec5a6acca746abace5383569898b91c73b6f4cf5e6af2e4a0172ab9432e` |
| 烧录后完整回读 | 131072 | `52f46acc769f596738fbacde9202633972920fda55d28400e3343bb1aa1431d3` |

## 启动与停车检查

1. 复位启动后，TIM12 计数器和双通道启用，`PSC=239`、`ARR=999`、`CCR1=CCR2=300`。PB14/PB15 为 TIM12 复用，PD2/PD3 高、PD1/PD4 低，符合当前正命令配置。未使用的 A/B PWM 和舵机引脚保持低电平。
2. TIM8 与 TIM4 编码器计数器均已启动，`ARR=65535`。启动约 490 ms 和 750 ms 的串口快照均为 `duty_left=300 duty_right=300`，剩余时间分别为 59520 ms、59260 ms。
3. 随后发送 `wheel stop`；约 870 ms 的快照已为双轮零输出、剩余时间为 0，后续复测仍保持停止。最终通过 SWD 确认 TIM12 双路比较值、PWM 引脚输入回读和四个方向脚均为 0。
4. 日常 `tools/check_board.py` 实机检查通过：周期最小值、最大值、当前间隔均为 10 ms，调度错误、串口接收/发送错误及溢出计数均为 0。CFSR/HFSR 均为 0，CPU 保持运行。

烧录前旧固件有累计 2 次串口接收错误，首次换行返回丢弃行提示，重同步后的查询成功；新固件启动及最终复测未出现接收错误。本次连接恢复与用户重新插接相吻合，未进一步定位具体接触点。

## 当前状态与验证边界

**板上现在为 C/D 临时上电测试版，当前双轮已停止。** 每次重新上电或复位仍会自动提交双轮 30%、60 秒请求；可用 `wheel stop` 提前停止。该固件用于开环驱动排查，固定占空比不保证恒定转速；用户排查完成后应删除临时启动请求、恢复默认零输出并重新烧录。

本次编码器增量和累计计数均为 0，未取得轮子实际转动或供电状况的现场证据，不能据此认定驱动板、电机、编码器接线或物理方向通过。没有额外发送逐轮运动命令，没有等待 C/D 固件 60 秒自动停止，也没有注入致命故障；对应行为仍按后续验收安排。Release 未烧录。

本次证据位于 `build/stage2-cd-mapping/flash-20261014-104539/`：`context.json`、`firmware-check.json`、`serial-precheck.json`、`identity-check.json`、`backup-check.json`、`flash-check.json`、`startup-stop-check.json`、`final-board-check.json`、`stopped-registers.json`，以及日志、完整 Flash 备份/回读与 `cd-wheel-test.bin/.elf/.hex`。这些产物保留在被忽略的 `build/`，没有新增长期测试程序。
