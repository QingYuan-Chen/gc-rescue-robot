# 工创赛智能救援机器人

STM32H750 与 MaixCAM Pro 救援机器人。当前驱动板采用 TB67H450，左轮 M2、右轮 M1。用户已确认双轮底盘电机测试无问题；编码器增量和累计以车体前进为正。

## 当前能力

- 上电保持电机关闭；UART8 提供限时开环命令及 VOFA FireWater 观测。
- 命令占空比范围 ±800‰，单次 10～2000 ms；支持停车、到期和周期故障锁定。
- 左编码器 TIM5，右编码器 TIM3；新增 10 ms 双轮 PI 和 VOFA+ 调参接口，待上板验收。
- 新增差速层：`chassis vw`、原地定角转向 `chassis turn`、锁定航向直行 `chassis straight`，带限速、超时和差速饱和处理，待上板验收。
- JY61P 使用 USART3 PD8/PD9 接收，提供连续航向和角速度；UART8 `imu get` 可查看原始数据和 `heading` 状态，待上板验收。
- 夹爪使用 PE11，PC1 打开（1750 μs）、PC2 闭合（2300 μs）；恢复按键功能的版本尚未烧录。

接线、调试命令与测试记录见 [M1/M2 联调记录](docs/validation/2026-11-10-m1-m2-uart8.md)、[电机接口说明](docs/基础电机与编码器接口.md)、[速度环 VOFA+ 调试](docs/底盘速度环VOFA调试.md)、[差速与转向环调试](docs/底盘差速与转向环调试.md)和[JY61P 接入说明](docs/JY61P陀螺仪接入.md)。

后续实施顺序、接口与验收条件见[转向环与识别夹取实施规划](docs/转向环与识别夹取实施规划.md)。

## 构建与测试

在 `firmware/stm32/` 下运行 `cmake --build --preset stm32-debug` 或 `stm32-release`。夹爪按键测试说明见[测试记录](docs/validation/2026-11-11-gripper-key-test.md)。

`rescue.ioc` 已同步资源分配；完整 CubeMX 重生成和故障关闭瞬态仍待验证。历史资料保存在 `docs/validation/` 和 Git 历史中。
