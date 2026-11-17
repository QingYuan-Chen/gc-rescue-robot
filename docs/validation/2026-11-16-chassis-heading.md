# 差速与转向环实现验证

日期：2026-11-16。在速度环之上加入 JY61P 连续航向、差速 `v/ω`、原地定角转向和锁定航向直行。

## 实现范围

- `App/runtime/heading.c/.h`：角度帧展开为连续航向，陀螺角速度，数据年龄、帧周期、跳变和超时判断。
- `imu rate <hz>`：通过 USART3 发送维特智能解锁、输出频率和保存命令。
- `App/control/chassis_kinematics.c/.h`：`v/ω` 到左右轮 RPM，转向优先饱和。
- `App/control/heading_control.c/.h`：角度 P、陀螺角速度阻尼、可选积分（积分分离、条件积分、限幅）、限幅、角加速度限制和持续到位判据。
- `App/runtime/chassis.c/.h`：`vw`、`turn`、`straight` 状态机，时长、距离、IMU 失效和停车处理；直行时按线速度和起步时间冻结积分。
- `App/runtime/wheel_io.c/.h`：底盘控制源与开环/速度调试互斥，`motor stop` 统一释放。
- `App/debug/chassis_commands.c/.h`：`chassis vw/turn/straight/stop/get`、`turn set`、`hold set`、`hold get`。
- `vofa on chassis`、`vofa on turn` 两个波形视图。

## 几何与整定值

轮径 65 mm、轮距 220 mm、1980 counts/转。速度上限 300 mm/s；转向 Kp 3.0、Kd 0.22、ω_max 90°/s、α 180°/s²、容差 2° / 5°/s；保向 Kp 1.0、Kd 0.00、Ki 0.10、积分区间 5°、积分限幅 5°/s、ω 限幅 30°/s。参数为当前整定值，仍需负载和重复性复核。

## 直行保向积分

直行环本轮去掉 D 项的整定作用（`Kd=0`，代码保留），改为弱积分消除两轮速度差带来的稳态偏角：

```text
ω = Kp·e + I − Kd·rate
I += Ki·e·dt                条件满足时
ω 限幅到 ±ω_max，再限制角加速度
```

- 积分分离：`|e| > izone` 时冻结，不清零。
- 条件积分：输出已饱和且与误差同向时不再同向积分。
- 低速冻结：`|v_cmd| < 30 mm/s` 时不积分。
- 起步冻结：`straight` 开始后 300 ms 内不积分。
- 每次 `straight` 开始清零积分；积分按 °/s 存储，改 Ki 输出不跳变。
- 转向环 `CHASSIS_TURN_KI=0` 且调用时关闭积分，行为与上一版一致。

## 构建

- `stm32-debug`：FLASH 88,984 B / RAM_D1 27,248 B，BIN SHA256 `E310709EE056CFD9E7C247D13BC397382F130346FA73B06B6659A30F6F52334A`。
- `stm32-release`：FLASH 95,600 B / RAM_D1 27,272 B，BIN SHA256 `27D4E8D91317FAF04837203067FE0828F6AB709ACD0B6997ACF2A5B537585FD6`。

## 主机测试

MinGW GCC 15.2.0，`ctest` 6/6 通过，`-O0`/`-O2` 单独编译运行通过：

- `heading_check`：连续航向、跨 ±180°、跳变和超时。
- `kinematics_check`：正负速度/角速度、转向饱和和输入校验。
- `heading_control_check`：输出方向、角加速度限制、阻尼、持续到位；积分分离、积分限幅、饱和时禁止同向积分、冻结与清零、改 Ki 输出连续、参数范围。
- `chassis_check`：`vw` 声明与到期、饱和、转向/直行状态、直行起步 300 ms 与低速积分冻结、拒绝占用。
- `chassis_commands_check`：命令解析、转向/直行、参数设置、`hold get`、转向环拒绝积分参数和非法输入。
- `wheel_io_chassis_check`：底盘声明、与速度目标/开环互斥、停车释放。

## 未验证项

未烧录、未操作电机。以下仍待上板：

- JY61P 实际帧率、安装轴和符号；需先配置 100 Hz。
- 架空转向方向和轮速；落地差速比例、轮径/轮距标定。
- 转向 Kp/Kd、保向增益、容差和超时。
- 直行保向 Ki / izone / ilimit：稳态偏角是否收敛、有无低频摆动、`i_mdps` 是否长期顶在限幅。
- IMU 断流/跳变时停车，以及 `motor stop` 的实际响应。
