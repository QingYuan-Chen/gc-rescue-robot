# UART5 无线调试串口

UART5 通过无线 DAPLink 提供命令响应、运行状态和参数读写入口。基础串口在 2026-10-12 已完成 Debug 上板验收；本轮阶段 2 Debug 固件已烧录，`ping`、双行 `status`、静止状态下的 `wheel stop` 及复位后零输出检查通过。非零车轮命令和编码器物理方向仍待实车验收。

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
| `wheel a 150 500` | A 轮以 +150 千分比开环运行 500 ms，B 轮输出为零 |
| `wheel b -150 500` | B 轮以 -150 千分比开环运行 500 ms，A 轮输出为零 |
| `wheel both 150 150 500` | 双轮均以 +150 千分比运行 500 ms，仅在单轮验收后使用 |

`report_ms` 允许 `0` 或 `100`～`60000`，单位为毫秒；只存 RAM，复位后为 `0`，自动上报也依次输出上述两行。它不改变车轮任务的 10 ms 周期。当前尚无 PID 或速度闭环参数。

车轮命令须先确认电机供电、两个驱动轮架空。手动占空比范围为 -300～300，时长为 10～2000 ms 且必须为 10 的倍数；超过范围会拒绝，不悄悄截断。命令回复 `queued` 表示已提交，并不等同于实际车轮已运动；用 `wheel status` 查看周期任务实际采用的输出。每条命令同时给定两轮，上电、复位及超时后均为零输出，直接反向时先保留一个周期的零输出。

命令的正值约定为车体前进，当前符号暂沿用原 A/B 设置，需要逐轮实测确认。编码器 `delta_a/b` 是最近一次 10 ms 采样的增量，`total_a/b` 是启动以来累计计数，`raw_a/b` 是硬件计数器原值；TIM3 为 16 位，TIM2 为 32 位。低频串口上报不会保存每一帧 10 ms 样本，累计值用于观察整个动作的计数变化。

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

只保留这一个日常板级检查脚本，查询链路、10 ms 周期及双轮零输出，结果保存在 `build/board-check.json`，不发送运动命令。复测前复位核心板，关闭其他占用串口的终端。该脚本配合阶段 2 新固件使用；旧串口固件尚无 `OK wheel` 响应。

阶段性串口压力、复位与解析测试脚本已移除；通过的输入和结果保留在历史验证记录中。后续每个阶段按实际变更补充检查，避免长期堆积一次性测试程序。

基础串口的烧录、完整 Flash 回读、人工注入队列溢出与复位检查证据见[无线串口验证记录](validation/2026-10-12-uart5-debug.md)。当前车轮固件的烧录、回读及零输出复测见[阶段 2 验证记录](validation/2026-10-12-stage2-wheels.md)；本次没有重复进行串口压力或队列溢出测试。
