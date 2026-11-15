# JY61P 串口接入

原理图 P1 的第 3 脚经 R26 接 PD8（USART3_TX），第 4 脚经 R27 接 PD9（USART3_RX）。使用 USART3、115200、8N1，速率依据旧工程 `Core/Src/usart.c` 和 `rescue.ioc`；实际模块配置和插接方向需上板核对。UART8 保留为调试口。

JY61P 使用 11 字节帧，帧头为 `0x55`，校验为前 10 字节之和的低 8 位。驱动接收加速度 `0x51`、角速度 `0x52` 和欧拉角 `0x53`，分别记录最后一次接收时间和有效帧数。协议及量程依据[维特智能 JY61P/JY901 手册](https://device.report/m/3fb7849216e571da4e7378bd5e1a0a3e830d09962b9f96a6fdcf548cec7aeba2)。

UART8 发送 `imu get` 查看状态：

- `receiving` 为串口接收状态；`accel_frames`、`gyro_frames`、`angle_frames` 是各类有效帧计数；`checksum_errors` 和 `uart_errors` 是错误计数。
- `accel_mg` 按 X、Y、Z 顺序输出毫 g；`gyro_mdeg_s` 输出毫度/秒；`angle_mdeg` 输出毫度。每类数据的 `age_ms` 表示距最近一帧的时间。
- 每种数据收到有效帧后才显示。方向沿用传感器坐标；传感器安装轴与车体方向仍需实测。JY61P 的 Z 轴角度会随时间漂移。

串口使用中断逐字节接收，初始化和 IRQ 接入位于 `App/bsp/imu.c` 与 `stm32h7xx_it.c` 的用户代码区；`.ioc` 同步了 USART3 和引脚。CubeMX 重生成后需核对 USART3 初始化及中断入口，避免重复生成。当前尚未烧录，实际模块波特率、输出内容、数据方向和接收负载待上板确认。
