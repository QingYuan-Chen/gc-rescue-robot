# 差速与转向环实现验证

日期：2026-11-16。在速度环之上加入 JY61P 连续航向、差速 `v/ω`、原地定角转向和锁定航向直行。

## 实现范围

- `App/runtime/heading.c/.h`：角度帧展开为连续航向，陀螺角速度，数据年龄、帧周期、跳变和超时判断。
- `imu rate <hz>`：通过 USART3 发送维特智能解锁、输出频率和保存命令。
- `App/control/chassis_kinematics.c/.h`：`v/ω` 到左右轮 RPM，转向优先饱和。
- `App/control/heading_control.c/.h`：角度 P、陀螺角速度阻尼、限幅、角加速度限制和持续到位判据。
- `App/runtime/chassis.c/.h`：`vw`、`turn`、`straight` 状态机，时长、距离、IMU 失效和停车处理。
- `App/runtime/wheel_io.c/.h`：底盘控制源与开环/速度调试互斥，`motor stop` 统一释放。
- `App/debug/chassis_commands.c/.h`：`chassis vw/turn/straight/stop/get`、`turn set`、`hold set`。
- `vofa on chassis`、`vofa on turn` 两个波形视图。

## 几何与初值

轮径 65 mm、轮距 220 mm、1980 counts/转。速度上限 300 mm/s；转向 Kp 3.0、Kd 0.22、ω_max 90°/s、α 180°/s²、容差 2° / 5°/s；保向 Kp 1.0、Kd 0.00、ω 限幅 30°/s。参数为当前整定值，仍需负载和重复性复核。

## 构建

- `stm32-debug`：FLASH 87,872 B / RAM_D1 27,208 B，BIN SHA256 `F0DADD9D6EAA99B0F12EDE07887DFF257C0A76005C1017E594DECD48A75B511F`。
- `stm32-release`：FLASH 94,576 B / RAM_D1 27,232 B，BIN SHA256 `469A708D36F2A3B148DB608583152B9C0B26CE98B5C756DF79BBF5C17B67711C`。

## 主机测试

MinGW GCC 15.2.0，`ctest` 6/6 通过，`-O0`/`-O2` 单独编译运行通过：

- `heading_check`：连续航向、跨 ±180°、跳变和超时。
- `kinematics_check`：正负速度/角速度、转向饱和和输入校验。
- `heading_control_check`：输出方向、角加速度限制、阻尼和持续到位。
- `chassis_check`：`vw` 声明与到期、饱和、转向/直行状态、拒绝占用。
- `chassis_commands_check`：命令解析、转向/直行、参数设置和非法输入。
- `wheel_io_chassis_check`：底盘声明、与速度目标/开环互斥、停车释放。

## 未验证项

未烧录、未操作电机。以下仍待上板：

- JY61P 实际帧率、安装轴和符号；需先配置 100 Hz。
- 架空转向方向和轮速；落地差速比例、轮径/轮距标定。
- 转向 Kp/Kd、保向增益、容差和超时。
- IMU 断流/跳变时停车，以及 `motor stop` 的实际响应。
