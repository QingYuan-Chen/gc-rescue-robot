# UART5 无线调试串口

UART5 通过无线 DAPLink 提供命令响应、运行状态和参数读写入口。基础串口在 2026-10-12 已完成 Debug 上板验收。当前为 C 左轮、D 右轮，两路电机符号均为 -1，编码器符号为左 +1、右 -1。2026-10-14 已完成[编码器方向与增量专项验收](validation/2026-10-14-encoder-direction-increment.md)，修正版 Debug 已烧录并完整回读一致；用户逐路确认了实际左右轮。以下含义适用于当前源码与板上固件。

测试版保留为历史提交 `c7a7f1a`。[移除临时上电测试](validation/2026-10-14-wheel-startup-cleanup.md)的提交 `b16fa3f` 已按授权合入 `main`。当前仍默认零输出，通过下述限时命令开展专项测试，没有新增固件测试任务；用户已授权将本次编码器符号修正及记录提交、推送到 `develop`。

## 接线与终端

| 拓展板 UART5 | STM32 引脚 | 连接到无线 DAPLink 的核心板端 |
| --- | --- | --- |
| 1 | PB13 / TX | RX |
| 2 | PB12 / RX | TX |
| 3 | GND | GND |

引脚来自 `hardware/Netlist_Schematic1_4_2026-09-18.tel`；使用 3.3 V TTL 信号。电脑端收发器连接 USB，2026-10-12 实测识别为 `COM5`，VID:PID 为 `FAED:4873`，设备序列号为 `3FEE83BB`。COM 编号可能变化，应按设备身份选择串口。

终端设置：**115200 波特率、8 数据位、无校验、1 停止位，无硬件流控**。开启发送换行；支持 CR、LF、CRLF。命令使用小写 ASCII，终端可开启本地回显，固件不逐字回显。

## 当前命令

| 命令 | 作用与示例回复 |
| --- | --- |
| `help` | 列出命令 |
| `ping` | 检查双向链路，回复 `OK pong` |
| `status` | 依次返回 `OK status` 基础状态、`OK wheel` 车轮状态两行 |
| `get report_ms` | 读取上报周期，例如 `OK report_ms=0` |
| `set report_ms 1000` | 每 1000 ms 上报一次 `status` |
| `set report_ms 0` | 关闭自动上报 |
| `wheel status` | 单独查询双轮输出、周期增量、累计计数与剩余运行时间 |
| `wheel stop` | 提交双轮停车命令，正常调度下最迟在下一个 10 ms 周期执行 |
| `wheel left 150 500` | 左轮 C 以 +150 千分比开环运行 500 ms，右轮 D 输出为零 |
| `wheel right -150 500` | 右轮 D 以 -150 千分比开环运行 500 ms，左轮 C 输出为零 |
| `wheel both 150 150 500` | 参数依次为左轮 C、右轮 D、时长；此例双轮均以 +150 千分比运行 500 ms，仅在单轮验收后使用 |

`report_ms` 允许 `0` 或 `100`～`60000`，单位为毫秒；只存 RAM，复位后为 `0`，自动上报也依次输出上述两行。它不改变车轮任务的 10 ms 周期。当前尚无 PID 或速度闭环参数。

车轮命令须先确认电机供电、两个驱动轮架空。手动占空比范围为 -300～300，时长为 10～2000 ms 且必须为 10 的倍数；超过范围会拒绝，不悄悄截断。命令回复 `queued` 表示已提交，并不等同于实际车轮已运动；用 `wheel status` 查看周期任务实际采用的输出。每条命令同时给定两轮，超时后为零输出，直接反向时先保留一个周期的零输出。清理后的源码在上电或复位时不提交运动请求，等待显式限时命令；停车或超时后持续保持零输出。

命令的正值约定为车体前进。两路电机符号均为 -1，正命令对应 IN1 低、IN2 高；前进时左轮 TIM4 原始计数增加、右轮 TIM8 原始计数减少，因此编码器符号分别为 +1、-1。状态字段按 `left/right` 区分：`duty_left/right` 为控制层实际采用的有符号占空比千分值，不因板级电机符号取反而改变；`delta_left/right` 是最近一次 10 ms 采样的增量，`total_left/right` 是启动以来累计计数，两者均统一为前进正、后退负；`raw_left/right` 是未做方向修正的硬件计数器原值，范围为 0～65535。低频串口上报不会保存每一帧 10 ms 样本，累计值用于观察整个动作的计数变化，不能把若干上报帧的 `delta` 相加当作整个动作的总量。

需要现场核对左右和方向时，一次只发送一路限时命令，停车并确认实际车轮后再测下一路。以“C 通道 / 软件 left”或“D 通道 / 软件 right”标识当前动作，避免连续切换两路后把用户观察对应到错误指令。

旧 `wheel a/b` 命令及 `*_a/b` 状态字段已删除，不保留别名。板级映射为 C 左轮：PB15 / TIM12_CH2、PD12/PD13 / TIM4；D 右轮：PB14 / TIM12_CH1、PC6/PC7 / TIM8。

`status` 中 `interval`、`min`、`max` 单位为 RTOS tick，目前 1 tick = 1 ms；`robot_stack_free`、`debug_stack_free` 单位为字节。`delay_errors` 记录错过的周期调度点，`rx_overflow` 记录接收队列丢弃字节数，`rx_errors` 和 `tx_errors` 记录串口接收/发送错误。所有计数复位后清零。

调试命令采用“发送一条，等待一条回复”的方式。单行最多 95 字节；超长行、非 ASCII 控制字节及发生丢字节的残缺行会被丢弃，并在行结束时返回 `ERR`。输入错误不会改变已有参数。大量无等待查询会超过有限队列的承载能力，发生丢字节时先发送换行，再重发完整命令。该文本接口不用于高频控制数据流。

## 模块职责与后续接入

- `App/bsp/debug_uart`：唯一 UART5 收发模块，中断收发、接收队列、错误恢复与计数。
- `App/debug/debug_console`：调试任务、文本命令与状态上报；不直接写电机、舵机或其他模块内部变量。
- `App/debug/wheel_commands`：车轮命令解析，调用 `wheel_drive` 公开接口；不直接写 GPIO、PWM 或串口。
- `App/runtime/runtime_monitor`：保留 10 ms 周期观测，通过临界区保护的公开快照提供状态。
- `App/config/debug_uart_config.h`：UART5 句柄映射、队列容量与发送超时；引脚、波特率、中断配置由 `rescue.ioc` 生成，不重复维护寄存器配置。

UART5 不使用 DMA；旧 UART5 RX DMA 配置已从 `.ioc` 与生成代码移除。调试任务栈为 3072 字节、优先级为 `osPriorityLow`，基础任务为 `osPriorityBelowNormal`；串口等待通过队列或任务标志让出 CPU。HAL 回调只处理 UART5，不占用 USART2 视觉串口或 USART3 IMU 串口。

当前 CubeMX 在仅启用 UART5 自动初始化时没有自动保留 `main.c` 中的 `usart.h` 引用，因此该引用放在 `USER CODE Includes` 中。已在副本中重新生成并核对相关 Core 文件，后续生成时保留用户区。

## 复测

先烧录本次固件，仅需核心板供电。电脑 Python 环境需要 `pyserial`，然后在仓库根目录运行：

```powershell
python -X utf8 tools/check_board.py --port COM5
```

只保留这一个日常板级检查脚本，查询链路、10 ms 周期及双轮零输出，结果保存在 `build/board-check.json`，不发送运动命令。关闭其他占用串口的终端；当前清理版上电默认零输出，可直接检查。如果回退到历史临时测试版 `c7a7f1a`，复位后应先发送 `wheel stop` 并确认 `wheel status` 为零，或等待 60 秒到期，再执行检查。该脚本配合 C/D 固件的 `duty_left/right` 字段使用，旧 A/B 固件需使用其对应版本工具。

阶段性串口压力、复位与解析测试脚本已移除；通过的输入和结果保留在历史验证记录中。后续每个阶段按实际变更补充检查，避免长期堆积一次性测试程序。

基础串口的烧录、完整 Flash 回读、人工注入队列溢出与复位检查证据见[无线串口验证记录](validation/2026-10-12-uart5-debug.md)。历史 A/B 车轮固件的烧录、回读及零输出复测见[阶段 2 验证记录](validation/2026-10-12-stage2-wheels.md)；初次切换 C/D 的记录见[C/D 同步记录](validation/2026-10-13-cd-wheel-mapping.md)，电机左右与方向修正见[修正记录](validation/2026-10-14-wheel-direction-fix.md)，当前编码器符号及实测结果见[专项验收记录](validation/2026-10-14-encoder-direction-increment.md)。
