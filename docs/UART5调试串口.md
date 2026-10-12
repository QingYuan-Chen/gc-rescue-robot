# UART5 无线调试串口

UART5 通过无线 DAPLink 提供命令响应、运行状态和参数读写入口。2026-10-12 已完成 Debug 固件上板验收；本次只接入调试串口，车轮控制仍按阶段 2 的顺序开展。

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
| `status` | 查询时钟、任务周期、栈余量、串口统计与当前上报周期 |
| `get report_ms` | 读取上报周期，例如 `OK report_ms=0` |
| `set report_ms 1000` | 每 1000 ms 上报一次 `status` |
| `set report_ms 0` | 关闭自动上报 |

`report_ms` 允许 `0` 或 `100`～`60000`，单位为毫秒；只存 RAM，复位后为 `0`。它不改变基础任务的 10 ms 周期。当前尚无 PID、轮速或电机输出参数，这些参数随相应模块迁移后再通过模块公开接口接入。

`status` 中 `interval`、`min`、`max` 单位为 RTOS tick，目前 1 tick = 1 ms；`robot_stack_free`、`debug_stack_free` 单位为字节。`delay_errors` 记录错过的周期调度点，`rx_overflow` 记录接收队列丢弃字节数，`rx_errors` 和 `tx_errors` 记录串口接收/发送错误。所有计数复位后清零。

调试命令采用“发送一条，等待一条回复”的方式。单行最多 95 字节；超长行、非 ASCII 控制字节及发生丢字节的残缺行会被丢弃，并在行结束时返回 `ERR`。输入错误不会改变已有参数。大量无等待查询会超过有限队列的承载能力，发生丢字节时先发送换行，再重发完整命令。该文本接口不用于高频控制数据流。

## 模块职责与后续接入

- `App/bsp/debug_uart`：唯一 UART5 收发模块，中断收发、接收队列、错误恢复与计数。
- `App/debug/debug_console`：调试任务、文本命令与状态上报；不直接写电机、舵机或其他模块内部变量。
- `App/runtime/runtime_monitor`：保留 10 ms 周期观测，通过临界区保护的公开快照提供状态。
- `App/config/debug_uart_config.h`：UART5 句柄映射、队列容量与发送超时；引脚、波特率、中断配置由 `rescue.ioc` 生成，不重复维护寄存器配置。

UART5 不使用 DMA；旧 UART5 RX DMA 配置已从 `.ioc` 与生成代码移除。调试任务栈为 3072 字节、优先级为 `osPriorityLow`，基础任务为 `osPriorityBelowNormal`；串口等待通过队列或任务标志让出 CPU。HAL 回调只处理 UART5，不占用 USART2 视觉串口或 USART3 IMU 串口。

当前 CubeMX 在仅启用 UART5 自动初始化时没有自动保留 `main.c` 中的 `usart.h` 引用，因此该引用放在 `USER CODE Includes` 中。已在副本中重新生成并核对相关 Core 文件，后续生成时保留用户区。

## 复测

先烧录本次固件，仅需核心板供电。电脑 Python 环境需要 `pyserial`，然后在仓库根目录运行：

```powershell
python -X utf8 tools/test_debug_uart.py --port COM5 --rounds 100
```

脚本检查命令、分段发送、换行、退格、参数上下限、错误恢复、自动上报、连续问答及周期状态；结果保存在 `build/uart5-validation/serial.json`。复测前复位核心板，以清除历史错误计数。复测时关闭其他占用同一串口的终端。脚本只修改上报周期，完成后恢复为 `0`。

烧录、完整 Flash 回读、人工注入队列溢出与复位检查的证据见[无线串口验证记录](validation/2026-10-12-uart5-debug.md)。
