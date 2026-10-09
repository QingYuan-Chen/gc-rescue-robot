# 工创赛智能救援机器人

STM32H750 控制与 MaixCAM Pro 视觉的智能救援机器人项目。

当前提交为恢复后的参考工程及重构设计基线，保留原控制流程和本地已有外设配置。**代码尚未完成两轮差速适配和视觉协议统一，不是可直接参赛的整车版本。**

## 已确认的机械结构

- 底盘：左右两个后部驱动轮，前部被动万向轮，两轮差速驱动。
- 左轮接电机 A：编码器 PA6/PA7（TIM3），PWM PE5（TIM15_CH1），方向 PB10/PE12。
- 右轮接电机 B：编码器 PA0/PA1（TIM2），PWM PE6（TIM15_CH2），方向 PE10/PE8。
- C/D 电机通道不使用；左右轮实际电机方向和编码器极性仍需标定。
- 车头为舵机控制的环状夹爪；MaixCAM Pro 安装于车体上的独立一维俯仰电动云台。

## 仓库内容

| 目录 | 内容 |
| --- | --- |
| `firmware/stm32/` | STM32 固件、CubeMX 配置、CMake、链接及调试配置 |
| `firmware/stm32/gc_rescue/` | 原电机、PID、舵机、IMU、视觉通信及任务代码 |
| `firmware/stm32/Drivers/` | 构建所需 CMSIS、STM32 HAL 及原许可证 |
| `firmware/stm32/Middlewares/` | FreeRTOS 源码及原许可证 |
| `vision/maixcam/main.py` | 现有 MaixCAM 视觉参考程序 |
| `hardware/` | 核心板原理图 PDF 与 TEL 网表 |
| `docs/` | 机器人重构方案 |

不纳入编译产物、IDE 缓存、临时文件、应用打包产物、模型权重及数据集。

## 编译 STM32 固件

准备 CMake 3.28 或更高版本、Ninja 与 GNU Arm Embedded 工具链，将 `cmake`、`ninja`、`arm-none-eabi-*` 加入 PATH。在仓库根目录运行：

```powershell
cmake -S firmware/stm32 -B build/stm32 -G Ninja -DCMAKE_BUILD_TYPE=Debug
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败' }
cmake --build build/stm32 --parallel 8
if ($LASTEXITCODE -ne 0) { throw '固件构建失败' }
```

产物位于 `build/stm32/`，包括 `rescue.elf`、`rescue.hex` 和 `rescue.bin`。恢复时的环境、兼容处理和历史构建结果见 [BUILD_NOTES.md](firmware/stm32/BUILD_NOTES.md)。其中旧目录名与校验值是历史记录，新目录构建的调试信息可能使产物哈希变化。

2026-10-09 在本仓库目录重新完成全部 53 步编译链接（ARM GCC 13.3.1）；FLASH 使用 40,664 字节，RAM_D1 使用 25,416 字节。保留 6 条既有 `volatile` 限定符丢弃警告；未进行烧录或实车验证。

## 当前限制与后续工作

1. 原电机代码仍使用 TIM1，与舵机资源冲突；按当前实车接线，应重构为 TIM15 的 A/B 两路输出和双轮速度环。
2. STM32 旧接收协议与 MaixCAM 的 `55 AA + 12 字节` 数据帧不兼容，需要统一协议后再联调。
3. 原任务流程面向抓球，尚未实现新赛项流程；原任务开关和舵机调试逻辑保留。
4. 视觉参考程序使用 YOLOv5，并引用设备上的 `/root/models/gc/290808/model_290808.mud`。本仓库不包含该旧模型；方案中的新四类模型尚待训练与板端验证，不能把设计方案视为已实现功能。
5. 编译成功不代表硬件动作正确；烧录、轮向、舵机限位、供电和整车行为需分别验证。

详细方案见 [智能救援机器人重构方案](docs/智能救援机器人重构方案.md)。下一阶段按硬件审计、A/B 电机闭环、舵机标定、通信协议、视觉与比赛状态机的顺序推进。

## 来源与许可证说明

STM32 参考代码源于 [StandByWind/gc_rescue](https://github.com/StandByWind/gc_rescue)，恢复基线为提交 `8e1bed2`，本快照同时保留恢复后的本地修改。视觉源码及硬件资料来自本地项目。

第三方源码保留其原有版权声明与许可证。当前未为项目自有代码新增统一许可证；第三方许可证不自动适用于整个仓库。
