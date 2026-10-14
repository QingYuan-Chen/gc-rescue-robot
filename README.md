# 工创赛智能救援机器人

STM32H750 控制与 MaixCAM Pro 视觉的智能救援机器人项目。

当前仓库保存原工程参考源码，并从底层开始逐项重构。**阶段 1、UART5 无线调试及阶段 2 车轮底层已通过 Debug 上板验收。2026-10-14 编码器专项测试修正了右轮反馈符号，逐轮正反转、10 ms 增量快照、累计计数、16 位回绕及停车检查通过；用户逐路确认 C 左轮、D 右轮。当前上电默认零输出，下一阶段为原 PID 算法及参数等效移植。**

原工程审核发现的问题不纳入本次重构的整改范围，也不作为开始重构的前置条件。重构工程按已确认的硬件映射建立自己的模块及验收记录；原代码用于提取算法、参数和配置参考。

## 仓库与当前进度

- 远端仓库：[QingYuan-Chen/gc-rescue-robot](https://github.com/QingYuan-Chen/gc-rescue-robot)。
- 日常开发使用 `develop`；完成修改和验证后先交用户审核，确认后才提交或推送。阶段稳定版本经授权合入 `main`；远端名为 `origin`。
- 重构位置：本仓库的 `firmware/stm32/`；同级 `../gc_rescue/` 保存本地原工程及烧录备份，不作为继续修复旧问题的工作项。
- 当前源码：C 左轮、D 右轮，两路电机符号均为 -1，编码器符号为左 +1、右 -1，反馈统一为前进正、后退负。[编码器专项验收](docs/validation/2026-10-14-encoder-direction-increment.md)已通过；用户已审核并授权将本次符号修正及记录提交、推送到 `develop`。此前[清理上电测试](docs/validation/2026-10-14-wheel-startup-cleanup.md)的提交 `b16fa3f` 已按授权合入 `main`，测试版 `c7a7f1a` 保留为历史。
- 验证记录：[UART5 无线调试](docs/validation/2026-10-12-uart5-debug.md)、[阶段 1 最小运行基础](docs/validation/2026-10-12-stage1-runtime.md)、[原工程构建与无线烧录](docs/validation/2026-10-11-original-firmware.md)。串口和基础运行验证不代表车轮或整车任务验收通过。
- 阶段 2 当前结论：Debug 在驱动轮架空条件下的开环输出与编码器反馈通过；Release 仅完成构建。此前[阶段 2 增量与清理记录](docs/validation/2026-10-12-stage2-wheels.md)保留为历史；其中生成副本清理曾被自动审批拦截，相关目录仍保留。

## 已确认的机械结构

- 底盘：左右两个后部驱动轮，前部被动万向轮，两轮差速驱动。
- 左轮接电机 C：编码器 PD12/PD13（TIM4_CH1/CH2），PWM PB15（TIM12_CH2），方向 IN1=PD3、IN2=PD4。
- 右轮接电机 D：编码器 PC6/PC7（TIM8_CH1/CH2），PWM PB14（TIM12_CH1），方向 IN1=PD2、IN2=PD1。
- A/B 电机通道不使用；业务命令按左、右排列。电机符号均为 -1，正命令对应 IN1 低、IN2 高。前进时左轮 TIM4 原始计数增加、右轮 TIM8 原始计数减少，编码器符号分别为 +1、-1；对外有符号增量均为正，原始计数保留硬件方向。
- 车头环状夹爪：Servo1 / PE9 / TIM1_CH1。
- MaixCAM Pro 安装于车体独立的一维俯仰电动云台：Servo2 / PE11 / TIM1_CH2。

2026-10-10 用户确认现有硬件和机构已在原工程中实践使用，并确认上述两个舵机接口。其余硬件和机构沿用；不要求全面硬件审计或新增传感器后才开始软件重构。

2026-10-11 用户在确认烧录无问题后决定进入重构，不再等待原工程问题整改或整车功能恢复。实施顺序继续遵循自底向上：先通过左右轮开环输出与编码器验收，再移植原 PID 算法、参数、速度环及转向环，最后进入赛题任务与视觉、云台和夹爪协同。

## 仓库内容

| 目录 | 内容 |
| --- | --- |
| `firmware/stm32/` | STM32 固件、CubeMX 配置、CMake、链接及调试配置 |
| `firmware/stm32/App/` | 左右轮电机与编码器底层、10 ms 车轮任务、运行统计及 UART5 调试控制台 |
| `firmware/stm32/cmake/` | 共用工具链定义与显式固件源文件清单 |
| `firmware/stm32/gc_rescue/` | 未迁移的算法和参数参考，已退出当前构建；模块接管时继续清理 |
| `firmware/stm32/Drivers/` | 构建所需 CMSIS、STM32 HAL 及原许可证 |
| `firmware/stm32/Middlewares/` | FreeRTOS 源码及原许可证 |
| `vision/maixcam/main.py` | 现有 MaixCAM 视觉参考程序 |
| `hardware/` | 核心板原理图 PDF 与 TEL 网表 |
| `docs/` | 机器人重构方案与阶段验证记录 |
| `docs/validation/` | 可追溯的构建、烧录及后续模块验收结果 |

`.gitignore` 排除编译产物、CMake/Ninja 与 IDE 缓存、临时文件、烧录备份、应用打包产物、模型权重、数据集和本地凭据。STM32 源码、`.ioc`、链接脚本、OpenOCD 配置、硬件资料和验证文档保留在 Git 中。需要留档的结果写入 `docs/validation/`，固件与完整 Flash 备份保留在本地归档中。

## 编译 STM32 固件

统一使用 [CMakePresets.json](firmware/stm32/CMakePresets.json)。当前验证组合为 STM32CubeCLT 1.20.0 中的 CMake 3.28.1、Ninja 和 ARM GCC 13.3.1；将 `cmake`、`ninja`、`arm-none-eabi-*` 加入 PATH。在 `firmware/stm32/` 目录运行：

```powershell
cmake --preset stm32-debug
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败' }
cmake --build --preset stm32-debug --parallel 8
if ($LASTEXITCODE -ne 0) { throw '固件构建失败' }
```

产物位于仓库根目录的 `build/stm32-debug/`，包括 `rescue.elf`、`rescue.hex` 和 `rescue.bin`；优化构建使用 `stm32-release`，输出到 `build/stm32-release/`。既有 `build/stm32/`、`cmake-build-*` 作为历史产物保留，后续只使用上述预设。

CLion 打开 `firmware/stm32/`，使用名为 `STM32` 的工具链，加载并启用 `stm32-debug` 预设。工具安装绝对路径留在本机工具链设置，`CMakeUserPresets.json` 不入 Git。配置细节见 [工程规范](docs/工程规范.md)。恢复时的兼容处理和历史验证记录见 [BUILD_NOTES.md](firmware/stm32/BUILD_NOTES.md)。

当前源码初始化 GPIO、TIM17 HAL 时基、UART5、TIM12、TIM8/TIM4 和 FreeRTOS。电机模块以零占空比初始化 C/D PWM，车轮任务每 10 ms 采样编码器；初始化不提交运动请求，收到显式限时命令前持续保持零输出。A/B PWM 保持低电平，其编码器、舵机及 DMA 均未启动。原时钟观察循环已由实际车轮任务替换，周期统计作为其轻量观测保留。UART5 继续使用独立的低优先级调试任务。

无线 DAPLink 调试串口使用 **115200、8N1、无流控**，本次实测为 `COM5`。发送 `help` 查看命令，`status` 查询状态，`set report_ms 1000` 每秒上报，`set report_ms 0` 关闭；参数仅存 RAM。接线、命令和复测方法见 [UART5 调试串口](docs/UART5调试串口.md)。

日常检查只保留 `tools/check_board.py`，查询无线链路、10 ms 周期及停车状态，不发送运动命令。阶段性长串口测试已删除；其历史结果保留在验证记录中。

## 开发约定

[工程规范 V1.4](docs/工程规范.md) 是目录、命名、模块接口、CLion 配置和验收方式的统一入口；[AGENTS.md](AGENTS.md) 约束后续协作。

新增重构代码放在 `firmware/stm32/App/`，采用 C11、UTF-8、LF、4 空格缩进，函数与变量使用 `snake_case`，接口名称明确单位。`.editorconfig`、`.clang-format` 与 `.clang-format-ignore` 共同约束格式；旧参考代码、生成代码与第三方库保留原格式和编码。

项目自有注释统一使用中文，仅必要术语和专有名词保留英文。模块和源文件遵循高内聚、低耦合、单一职责；新方案替换时同步清理失效实现、配置及说明，历史通过 Git 追溯。每完成一个可检查的增量，先提供差异及验证结果，用户审核确认后再提交到 `develop`；推送及稳定版本合入 `main` 同样需要相应授权。

2026-10-12 已完成当前自有注释统一及[工程规范核查](docs/validation/2026-10-12-comment-and-conventions-audit.md)。旧参考实现仍有模块耦合、职责混杂和构建组织方面的迁移项，记录中已对应到后续重构阶段；本次 Debug/Release 固件 BIN 与注释整理前一致。

## 重构顺序与验收

| 阶段 | 范围 | 当前状态 |
| --- | --- | --- |
| 0 | 原工程构建、下载链路与参考资料留档 | 已完成构建、烧录和回读校验；作为进入重构的依据 |
| 1 | 启动、时钟、基础时间源、最小任务与 SWD 状态观察 | Debug 已上板通过并经用户审核；Release 仅完成构建 |
| 2 | 左右轮方向、PWM、停车与编码器反馈 | Debug 架空开环验收通过；编码器左 +1、右 -1，用户已确认验收；Release 未烧录 |
| 3 | 原 PID 算法及参数等效移植 | 底层前置验收已通过，待开始 |
| 4 | 单轮与双轮速度环 | PID 对照通过后进行 |
| 5 | IMU 反馈、转向环与底盘基础运动 | 速度环通过后进行 |
| 6 | 夹爪、云台及通信基础控制 | 逐个外设验收 |
| 7 | 根据赛题设计任务与动作顺序 | 所需底层能力通过后进行 |
| 8 | 视觉、云台、夹爪协同及比赛联调 | 按任务需求逐步接入 |

每层验收通过后再进入依赖它的下一层；移植原 PID 时同时保留采样周期、反馈单位、方向、限幅和输出尺度。算法对照和重构后的实车验收在对应阶段完成，不回到原工程开展问题整改。

详细方案见 [智能救援机器人重构方案 V1.1](docs/智能救援机器人重构方案.md)。当前源码默认零输出，阶段 2 的 Debug 架空验收已完成；下一步提取原 PID 算法、参数及对应周期、单位和输出尺度，Release 未烧录。本次编码器验收不包含每轮一圈计数或物理速度标定。保留的 `gc_rescue/` 及视觉脚本用于后续迁移参考，不能视为已完成新赛项功能。视觉参考脚本引用设备上的旧 YOLOv5 模型，本仓库不包含模型权重；新模型、任务策略和新通信协议在后续对应阶段实现。

## 来源与许可证说明

STM32 参考代码源于 [StandByWind/gc_rescue](https://github.com/StandByWind/gc_rescue)，恢复基线为提交 `8e1bed2`，本快照同时保留恢复后的本地修改。视觉源码及硬件资料来自本地项目。

第三方源码保留其原有版权声明与许可证。当前未为项目自有代码新增统一许可证；第三方许可证不自动适用于整个仓库。
