# 工创赛智能救援机器人

STM32H750 与 MaixCAM Pro 救援机器人。当前驱动板采用 TB67H450，左轮 M2、右轮 M1。用户已确认双轮底盘电机测试无问题；编码器增量和累计以车体前进为正。

## 当前能力

- 上电保持电机关闭；UART8 提供限时开环命令及 VOFA FireWater 观测。
- 命令占空比范围 ±800‰，单次 10～2000 ms；支持停车、到期和周期故障锁定。
- 左编码器 TIM5，右编码器 TIM3；10 ms 周期采样。当前没有 PID。
- 本次清理版本尚未烧录：板上已有的测试固件复位后仍会自动驱动 60 秒。

接线、调试命令与测试记录见 [M1/M2 联调记录](docs/validation/2026-11-10-m1-m2-uart8.md) 和 [电机接口说明](docs/基础电机与编码器接口.md)。

## 构建与测试

在 `firmware/stm32/` 下运行 `cmake --build --preset stm32-debug` 或 `stm32-release`。主机验证见 [车轮测试](tests/wheel_io/README.md) 和 [基础安全测试](tests/safe_baseline/README.md)。

`rescue.ioc` 已同步资源分配；完整 CubeMX 重生成和故障关闭瞬态仍待验证。历史资料保存在 `docs/validation/` 和 Git 历史中。
