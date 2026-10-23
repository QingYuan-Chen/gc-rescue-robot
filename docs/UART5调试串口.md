# UART5 调试串口

本说明对应 2026-10-22 的 TB67H450 安全基础版源码。当前未烧录，不据此推定板上已有相同协议。

## 接线

| 板端信号 | STM32 引脚 | 接外部串口 |
| --- | --- | --- |
| UART5 TX | PB13，经 R29 至 CN4.2 | RX |
| UART5 RX | PB12，经 R30 至 CN4.3 | TX |
| GND | CN4.4 | GND |

依据为当前加过流保护版网表。使用 3.3 V TTL，115200、8N1、无流控；不要把 CN4.1 的 +5 V 接到信号输入。串口号与下载器身份需要现场重新识别，旧 COM5 只属历史。

## 命令

| 命令 | 响应 |
| --- | --- |
| `help` | 当前可用命令 |
| `ping` | `OK pong` |
| `status` | `OK status ...` 基础周期与串口统计，再返回 `OK safety ...` |
| `get report_ms` | 自动报告周期 |
| `set report_ms 0` | 关闭自动报告 |
| `set report_ms 1000` | 每秒报告；有效范围 100～60000 ms |

安全状态示例：

```text
OK safety profile=tb67h450-safe left=M4 right=M3 outputs_low=1 motor_control=0 encoders=0
```

`outputs_low` 根据全部四路驱动输入和舵机信号的 GPIO 模式与输出锁存寄存器计算；不等同于已测量芯片端实际电压。电机控制和编码器采样当前未实现。`wheel`、`pid`、`vofa` 等旧命令统一返回未知命令，不会启动电机。

`tools/check_board.py` 已按此协议更新，本轮仅检查脚本语法，没有连接串口执行。后续经授权烧录基础版后，可用当前实际串口执行该工具；旧固件的 `OK wheel` 状态不再适配。
