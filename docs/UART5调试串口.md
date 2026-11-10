> 当前固件使用 UART8/CN5，见 [基础电机与编码器接口](基础电机与编码器接口.md)；以下为历史 UART5 说明。

# UART5 调试串口

本说明对应 2026-11-10 的 TB67H450 上电测试版源码。当前未烧录，不据此推定板上已有相同协议。

## 接线

| 板端信号 | STM32 引脚 | 接外部串口 |
| --- | --- | --- |
| UART5 TX | PB13，经 R29 至 CN4.2 | RX |
| UART5 RX | PB12，经 R30 至 CN4.3 | TX |
| GND | CN4.4 | GND |

依据为当前加过流保护版网表。使用 3.3 V TTL，115200、8N1、无流控；不要把 CN4.1 的 +5 V 接到信号输入。串口号与下载器身份需要现场重新识别，旧 COM5 只属历史。

## VOFA 波形

串口连接 UART5，选择 FireWater 协议、115200 8N1；每 100 ms 自动发送一行，五个通道依次为：左轮实际占空比、右轮实际占空比（千分比）、左轮编码器增量、右轮编码器增量（每 10 ms 的 count）、故障标志。串口发送框使用文本并附加换行，可发送下列命令；命令回复也会出现在串口数据中。

上电后双轮以 100‰（10%）运行 60 秒并自动停车，每次复位会重新启动。使用 `motor stop` 可提前停车；运动命令会替换上电测试，时长为 10～2000 ms。

## 命令

| 命令 | 响应 |
| --- | --- |
| `help` | 当前可用命令 |
| `ping` | `OK pong` |
| `status` | `OK status ...` 基础周期与串口统计，再返回 `OK safety ...` 和 `OK motor ...` |
| `encoder`、`motor status` | 车轮快照：计数、增量、累计、请求、实际占空比和阶段 |
| `motor stop` | 提交双轮停车，在下一周期执行 |
| `motor left <duty> <ms>` | 左轮限时开环，同时将右轮设为零 |
| `motor right <duty> <ms>` | 右轮限时开环，同时将左轮设为零 |
| `motor both <left> <right> <ms>` | 同时指定双轮限时开环 |
| `get report_ms` | 自动报告周期 |
| `set report_ms 0` | 关闭自动报告 |
| `set report_ms 1000` | 每秒报告；有效范围 100～60000 ms |

安全状态示例：

```text
OK safety profile=tb67h450-basic left=M3 right=M4 outputs_low=1 motor_control=1 encoders=1
```

`outputs_low` 根据全部四路驱动输入和舵机信号的 GPIO 模式与输出锁存寄存器计算；不等同于已测量芯片端实际电压。`motor_control=1 encoders=1` 表示固件支持这两个模块，并非正在运动；运动时 `outputs_low=0` 属预期结果。`wheel`、`pid`、`vofa` 等旧命令统一返回未知命令，不会启动电机。

`duty` 为有符号千分比，当前范围 -800～800，`ms` 为 10～2000。正负号仅为电气方向，车体前进符号待实测。零为高阻停止。命令有效期包含唤醒阶段，新命令替换旧请求；停车优先。`OK motor queued` 只代表接收，输出状态以查询结果为准。接口与时序见[基础电机与编码器接口](基础电机与编码器接口.md)。

`OK motor` 字段：`ready/fault` 为就绪和故障锁定；`request_left/right` 为当前有效请求；`applied_left/right` 为实际已接入的 PWM 占空比；`phase_left/right` 为 0=停止、1=唤醒、2=运行；`remaining_ms` 为请求剩余时间。`counter_*` 是 16 位硬件计数，`delta_*` 是本次采样增量，`total_*` 是 64 位累计计数，采样时刻/间隔见 `sample_ms/interval_ms`。

`tools/check_board.py` 只发送 `ping` 和 `status`，检查空闲状态；发现活动请求或故障就报错，不发送停车或运动命令。本轮使用串口替身测试了正常/异常回复，没有连接真实串口。后续经授权烧录后，可用当时的实际串口执行工具。
