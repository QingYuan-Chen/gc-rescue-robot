# 工创赛智能救援机器人

STM32H750 与 MaixCAM Pro 智能救援机器人。硬件更换为 **TB67H450**，按新的硬件基线从基础层重新开始。左轮接 **M4**，右轮接 **M3**。旧驱动、旧 PID 参数和旧硬件验收结果不作为新版本的实现依据。

## 当前版本

2026-10-22 完成安全基础版源码清理。当前只运行 GPIO 输出保持、10 ms 基础运行观测、FreeRTOS 和 UART5 查询；没有电机控制、编码器采样、PID、自动转轮或运动命令。

- 四路 TB67H450 的全部 IN1/IN2 在 `main()` 入口、HAL 初始化前配置为低电平 GPIO，故障入口也保持相同状态。舵机信号同样保持低电平。
- M3/M4 编码器引脚暂作普通输入；电机 PWM 和编码器定时器初始化、启动代码已移除。
- 调试命令仅保留 `help`、`ping`、`status`、`get report_ms`、`set report_ms`；旧 `wheel`、`pid`、`vofa` 命令均被拒绝。
- 本轮按用户要求**不烧录、不执行硬件操作**。编译和主机测试通过不代表板上固件已更新，也不代表真实上电行为已验收。

新版接线、资料来源及待验证项见 [TB67H450 硬件基线](docs/TB67H450硬件基线.md)。本轮检查结果见 [安全基础版清理记录](docs/validation/2026-10-22-safe-baseline-cleanup.md)。

## 目录

| 位置 | 内容 |
| --- | --- |
| `firmware/stm32/Core/` | 启动、时钟、GPIO、UART5、RTOS 和 HAL 时基 |
| `firmware/stm32/App/bsp/board_outputs.c` | 唯一的执行器信号保持与 GPIO 状态检查入口 |
| `firmware/stm32/App/robot_app.c` | 10 ms 基础运行观测，不包含控制器 |
| `firmware/stm32/App/debug/` | UART5 查询控制台 |
| `firmware/stm32/App/runtime/` | 周期及任务运行统计 |
| `firmware/stm32/Drivers/`、`Middlewares/` | HAL、CMSIS、FreeRTOS，保留原许可证 |
| `hardware/` | 加过流保护版原理图 PDF 与网表 |
| `vision/maixcam/` | 视觉参考代码，本轮未接入基础固件 |
| `tests/safe_baseline/` | GPIO 关闭顺序、状态检查和旧命令拒绝的主机验证 |
| `tools/check_board.py` | 安全基础版只读查询工具，本轮未连接硬件运行 |
| `docs/validation/` | 历史与当前验证记录；旧记录只适用于记录时的硬件与固件 |

旧电机/PID/车轮模块、旧测试及依赖这些模块的原工程参考业务代码已从源码树移除。清理前的未提交内容保存在本地 `build/hardware-reset-20261021-220252/before.zip`，已提交历史仍由 Git 保存；同级 `../gc_rescue/` 原工程备份未改动。

## 构建

共享入口为 [CMakePresets.json](firmware/stm32/CMakePresets.json)。将 CMake、Ninja 和 ARM GCC 加入 PATH，在 `firmware/stm32/` 目录运行：

```powershell
cmake --preset stm32-debug
cmake --build --preset stm32-debug --parallel 8
```

Release 使用 `stm32-release`。产物分别位于仓库根目录 `build/stm32-debug/` 和 `build/stm32-release/`。本机工具路径留在 CLion 或被忽略的 `CMakeUserPresets.json`，不写进共享构建配置。

`rescue.ioc` 已收敛为当前基础版配置，不保留旧电机 PWM、编码器定时器或旧 C/D 标签。本轮检查配置与源码一致性，未通过 CubeMX 重新生成验证。

主机验证方法见 [测试说明](tests/safe_baseline/README.md)，串口查询说明见 [UART5 调试串口](docs/UART5调试串口.md)。

## 后续顺序

安全基础版审核 → 经授权烧录并验证上电/复位输入电平 → M4/M3 单轮开环与停车 → 手动转轮核对编码器映射、方向和每圈计数 → 重新设计速度控制器 → 转向与外设 → 赛题协同。

当前不移植旧 PID，不继承旧电机与编码器方向符号。下一阶段具体实现见 [重构方案](docs/智能救援机器人重构方案.md)。

## 协作

日常开发在 `develop` 进行；完成差异和检查后先交用户审核，确认后才提交或推送，不自动合并 `main`。规范见 [工程规范](docs/工程规范.md) 与 [AGENTS.md](AGENTS.md)。新增自有代码使用 C11、简体中文注释、UTF-8 无 BOM 和 LF，保留第三方许可证及生成文件原有编码与格式。
