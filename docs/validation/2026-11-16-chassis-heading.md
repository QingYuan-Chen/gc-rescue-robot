# 差速与转向环实现验证

日期：2026-11-16。在速度环之上加入 JY61P 连续航向、差速 `v/ω`、原地定角转向和锁定航向直行。

## 实现范围

- `App/runtime/heading.c/.h`：角度帧展开为连续航向，陀螺角速度，数据年龄、帧周期、跳变和超时判断。
- `imu rate <hz>`：通过 USART3 发送维特智能解锁、输出频率和保存命令。
- `App/control/chassis_kinematics.c/.h`：`v/ω` 到左右轮 RPM，转向优先饱和。
- `App/control/heading_control.c/.h`：角度 P（小偏差线性、大偏差制动曲线）、陀螺角速度阻尼、可选积分（积分分离、条件积分、限幅）、限幅、角加速度限制和持续到位判据。
- `App/runtime/chassis.c/.h`：`vw`、`turn`、`straight` 状态机，时长、距离、IMU 失效和停车处理；直行时按线速度和起步时间冻结积分。
- `App/runtime/wheel_io.c/.h`：底盘控制源与开环/速度调试互斥，`motor stop` 统一释放。
- `App/debug/chassis_commands.c/.h`：`chassis vw/turn/straight/stop/get`、`turn set`、`hold set`、`hold get`。
- `App/debug/speed_commands.c/.h`：只保留 `speed target/stop/get` 和 `vofa` 波形；速度环 PID 在线整定（`speed cfg/set/apply`）、参数暂存和速度事件队列已移除，PI 参数固定在 `wheel_speed.c`。
- `vofa on chassis`、`vofa on turn` 两个波形视图。

## 几何与整定值

轮径 65 mm、轮距 220 mm、1980 counts/转。速度上限 300 mm/s；转向 Kp 3.0、Kd 0.00、制动减速度 100°/s²、ω_max 90°/s、α 180°/s²、容差 2° / 5°/s；保向 Kp 1.0、Kd 0.00、Ki 0.10、积分区间 5°、积分限幅 5°/s、ω 限幅 30°/s。参数为当前整定值，仍需负载和重复性复核。

## 转向制动曲线

实测大角度转向需要 Kd、小角度（小于 40°）反而要释放 D：纯线性 P 在近目标段要求减速度 `Kp·ω`，超过 `accel` 后无法及时减速。改为两段曲线：

```text
e1 = brake / Kp²
|e| ≤ e1：  ω_p = Kp·e
|e| > e1：  ω_p = sign(e)·√(2·brake·(|e| − e1/2))
```

两段在 `e1` 处值与斜率连续；线性段减速度不超过 `brake`，制动段恒为 `brake`。`brake ≤ accel` 时减速不受角加速度限制，所有角度共用一套参数，`Kd` 可以取 0。`brake = 0` 退回线性 P，直行保向使用该模式。

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
- 转向环默认 `ki=0` 且调用时关闭积分，行为与上一版一致。

## 构建

- `stm32-debug`：FLASH 86,272 B / RAM_D1 26,472 B，BIN SHA256 `A5B1898FCF18A7A0668474C613664128762054742367BA5B5DFA37DE587F1CD9`。
- `stm32-release`：FLASH 91,528 B / RAM_D1 26,504 B，BIN SHA256 `103096DE7779BA33B5F870524B312BD693BF65B800CBBD92395DD3AC4735DD96`。
- 精简 `chassis_config.h`（只保留轮径、轮距和命令时长上下限 4 个共享宏，其余常量写在使用处）后重新构建，两个 BIN 的哈希与精简前一致，说明行为未变。
- 移除速度环 PID 在线整定（`speed cfg/set/apply`、速度事件队列和参数暂存）后重新构建，FLASH 减少约 3 KB；`speed target/stop/get` 和 `vofa` 波形保留。

## 主机测试

MinGW GCC 15.2.0，`ctest` 6/6 通过，`-O0`/`-O2` 单独编译运行通过：

- `heading_check`：连续航向、跨 ±180°、跳变和超时。
- `kinematics_check`：正负速度/角速度、转向饱和和输入校验。
- `heading_control_check`：输出方向、角加速度限制、阻尼、持续到位；积分分离、积分限幅、饱和时禁止同向积分、冻结与清零、改 Ki 输出连续、参数范围；制动曲线的切换点连续、对称、单调，以及带 150 ms 一阶滞后的航向仿真中 30°/40°/90°/180° 超调小于 2.5°，纯线性 P 的 180° 明显超调。
- `chassis_check`：`vw` 声明与到期、饱和、转向/直行状态、直行起步 300 ms 与低速积分冻结、拒绝占用。
- `chassis_commands_check`：命令解析、转向/直行、参数设置、`turn set brake` 的滑块与超限拒绝、`hold get`、转向环拒绝积分参数和非法输入。
- `wheel_io_chassis_check`：底盘声明、与速度目标/开环互斥、停车释放、停车后切入闭环速度目标。

## 未验证项

> 2026-11-19 更新：本节的待上板项已通过实车验收，见[底盘、航向与夹爪实车验收](2026-11-19-chassis-field-acceptance.md)。以下保留为当时的实现状态说明；其中量化判据（测角误差、超调、横向偏差、漂移量）仍待复测补记。

未烧录、未操作电机。以下仍待上板：

- JY61P 实际帧率、安装轴和符号；需先配置 100 Hz。
- 架空转向方向和轮速；落地差速比例、轮径/轮距标定。
- 转向 Kp/Kd、制动减速度 brake、保向增益、容差和超时。
- 转向制动曲线在真实速度环滞后下的超调；`brake` 实测 100°/s² 效果最佳，再按 30°/40°/90°/180° 的超调调整。
- 直行保向 Ki / izone / ilimit：稳态偏角是否收敛、有无低频摆动、`i_mdps` 是否长期顶在限幅。
- IMU 断流/跳变时停车，以及 `motor stop` 的实际响应。
