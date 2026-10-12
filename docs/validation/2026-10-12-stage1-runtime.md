# 阶段 1：最小运行基础验证

日期：2026-10-12。代码基线：`9312925aa780626193320bf9d19555db240544d0` 加本次阶段 1 变更，分支 `develop`。运行验证完成时尚未提交；2026-10-12 用户审核通过，并授权提交、推送到 `develop`。

## 实现范围

- `App/robot_app` 接入新应用；`bsp/board_startup` 在调度器启动前保持执行器信号为低电平；`runtime/runtime_monitor` 每 10 ms 更新私有观察数据。
- 保留原启动、时钟、MPU、HAL 与 FreeRTOS。HAL 使用 TIM17，RTOS 使用 SysTick，两个时基均按 1 ms 配置。
- `main` 只自动调用 GPIO 初始化；电机、舵机、编码器、UART 和 DMA 不启动。生成的 `robot_task` 是唯一应用任务，栈为 1024 字节、优先级为 `osPriorityLow`；FreeRTOS 自身的空闲任务及定时器服务任务继续保留。
- `.ioc` 同步任务、外设初始化调用及中断使能。任务创建失败和调度器启动失败进入 `Error_Handler`。
- CMake 显式登记当前 35 个源文件，选项采用目标配置；3 个新 App 源文件以 `-Wall -Wextra -Werror` 编译。有效入口和 CLion 模板共用 `cmake/` 定义。
- 删除已替代的 `gc_rescue/car_task.c/.h`、旧模块 `CMakeLists.txt` 和原任务开关。其余旧算法、参数及外设配置仍供后续迁移参考，未进入当前固件；同级原工程及备份未改动。

## 构建与配置核对

工具：STM32CubeCLT 1.20.0 中的 CMake 3.28.1、Ninja、ARM GCC 13.3.1。使用既有 `stm32-debug`、`stm32-release` 预设。

| 项目 | Debug | Release |
| --- | --- | --- |
| 配置与构建 | 36/36 步通过 | 36/36 步通过 |
| App 严格告警检查 | 4/4 步增量通过，无警告，BIN 未变 | 4/4 步增量通过，无警告，BIN 未变 |
| FLASH | 17,788 / 131,072 字节 | 18,616 / 131,072 字节 |
| RAM_D1 | 23,272 / 524,288 字节，含链接脚本预留区 | 23,272 / 524,288 字节，含链接脚本预留区 |
| 上板验证 | 本记录中的下载和运行检查通过 | 未烧录，不能沿用 Debug 的运行结论 |

编译命令清单确认旧 `gc_rescue/` 不在编译范围，新 App 的严格告警选项实际生效。ELF 中保留 `robot_app`、`robot_task_entry` 和私有 `runtime_status`，没有旧启动任务及 `my_car`。

将 `.ioc` 和 Core 复制到被忽略的 `build/stage1-regeneration/`，使用本机 CubeMX 在副本上生成代码。`main.c`、`freertos.c`、`stm32h7xx_it.c/.h`、`dma.c`、`tim.c`、`usart.c` 这 7 个文件的非注释词法内容与工作树一致，应用接入的 USER CODE 保留区正常保留。CMake 有效入口与模板的有效配置内容一致。新 App 文件通过 clang-format 检查，差异通过 `git diff --check`。

该再生成检查仅验收上述 Core 内容：临时 Makefile 导出还出现本机扩展包 PDSC 兼容提示及 `sysmem.c` / `syscalls.c` 绝对路径重复错误，未用其工程文件或库文件替换工作树，也未宣称完整 IDE 导出流程通过。当前共享 CMake 入口已独立构建通过。

## 下载链路、供电与回读

用户在本轮确认核心板已上电。此阶段只需核心板供电，不需要执行器供电，未下发车轮、夹爪或云台运动指令；执行器供电状态未通过工具独立测量。

- 下载器：Horco CMSIS-DAP v2，序列号 `3fee83bb`，SWD。
- 目标：Cortex-M7 r1p1，`DBGMCU_IDCODE=0x20036450`，Flash 容量寄存器为 128 KiB。
- 写入前完整备份 128 KiB；其 SHA256 与阶段 0 留档的写入后 Flash 一致。
- 以 1000 kHz 写入 Debug BIN 到 `0x08000000`，OpenOCD 校验通过；独立完整回读后确认前 17,788 字节与 BIN 完全一致，余下 Flash 全为 `0xFF`。
- 最终严格告警构建的 Debug BIN 与已烧录、已实测的 BIN 完全相同，无需重新烧录。

## 运行观察

先执行 3 次软件复位，每次启动后通过 SWD 读取观察数据。第三次启动后持续观察，未进行执行器测试。

| 观察项 | 实际结果 |
| --- | --- |
| 三次复位 | 每次均启动任务，约 1 秒后采样计数均为 112 |
| `SystemCoreClock` | 480,000,000 Hz，为固件报告的配置值 |
| RTOS 节拍 | 1000 Hz；SysTick LOAD=479999 |
| HAL 时基 | TIM17 PSC=239、ARR=999，运行状态正常 |
| 周期 | 10 tick，即 10 ms；所有已采集快照的最小、最大间隔均为 10 tick |
| 调度错误 | `delay_error_count=0` |
| 时间与计数 | 最后一组记录 `sample_count=3549`，HAL/RTOS 时间均为 35,490 ms |
| 任务栈 | 最小未使用栈空间 876 字节 / 已分配 1024 字节 |
| 故障寄存器 | 重连时读取 CFSR=0、HFSR=0 |
| A/B 方向、PWM 及舵机信号 | GPIO 模式为推挽输出；对应 ODR 和 IDR 位均为 0 |
| TIM1/TIM15 | APB2 时钟未使能；APB2ENR=0x00040000，仅 TIM17 位使能 |
| 最终芯片状态 | running，继续运行阶段 1 最小固件 |

1000 kHz 连续采样过程中，读取了 5 组数据后出现一次 SWD DPIDR/内存读取失败。改为 100 kHz 后成功重连，未再复位芯片；任务计数由掉线前的 617 增长到重连时的 1525，之后连续 20 组采样通过，最终达到 3549。未观察到固件故障或计数回退；无线中断的物理根因尚未确认，不能仅据此次降速认定已经根治。共享 OpenOCD 配置默认改为本次连续采样通过的 100 kHz。

以上 GPIO 结论来自寄存器与引脚输入回读，未使用示波器测量，也不代表电机、编码器或舵机功能已经验收。

## 复查方法与本地证据

在 CLion/GDB 中加载本次 Debug ELF，通过 SWD 查看 `runtime_monitor.c` 中的私有变量 `runtime_status`。持续运行时观察 `sample_count`、`hal_tick_ms` 和 `rtos_tick` 增长，检查周期范围与 `delay_error_count`。调试暂停会影响观察窗口，正式复查前重新复位启动；不要向观察结构写值或通过调试器调用执行器函数。

`runtime_status` 的地址由每次 ELF 符号表确定，不在长期脚本中固定。可运行：

```powershell
arm-none-eabi-nm -S --defined-only build/stm32-debug/rescue.elf |
    Select-String -Pattern 'runtime_status'
```

本次地址为 `0x240051bc`，大小为 44 字节；只适用于本记录的固件。具体下载、备份和采样命令保留在下列本地配置脚本中，重新执行下载会写入 Flash，应遵循当前用户授权。

本地证据目录：`build/stage1-validation/`，按仓库规则不进入 Git：

- `backup.cfg/.log`、`flash-debug.cfg/.log` 与完整写入前后 Flash。
- `runtime-debug.cfg/.log`：含三次复位和一次链路中断的原始记录。
- `runtime-reconnect.cfg/.log`：100 kHz 重连、寄存器及连续 20 组运行数据。
- `verification.json`：已自动核对的运行数值、寄存器、固件与备份标识。
- `source-hashes.json`：本轮修改/新增固件文件的 SHA256。
- Debug/Release 的 ELF、HEX、BIN 快照，避免后续构建覆盖本阶段证据。

构建日志保留在 `build/stm32-debug/`、`build/stm32-release/` 的 `configure-stage1.log`、`build-stage1.log` 和 `build-stage1-strict.log`。再生成日志保留在 `build/stage1-regeneration/`。

| 归档文件 | 字节 | SHA256 |
| --- | ---: | --- |
| `stm32-debug.bin` | 17788 | `828ec0f07b17036fac27a438163bf7f162b0455409bc34c445e595e27953e9e6` |
| `stm32-debug.elf` | 447292 | `bab75a0fc655ee47410241978103fe24ea9bf649caaa2aca77d57fafe029ce4b` |
| `stm32-debug.hex` | 50093 | `9b12a0b612f592dd323ef9cfea738eb51d43982b23c6a78ca9f920f214cc1304` |
| `stm32-release.bin` | 18616 | `658c7cb2b4ac475d12f2cbabc1750754e8a1fc80243e6a3e9fc22ff8f56f9769` |
| `stm32-release.elf` | 47924 | `59ec8c593545592c8e06ec2d85534183e00c4a85455bc0c1a2338bd9e90e585b` |
| `stm32-release.hex` | 52425 | `b7776bf61cfb3e646161925d092f7a12ef8e62691856e2b086c915466feeb137` |
| `flash-before.bin` | 131072 | `046e4721415a8b01678baca9357ca3c1c9eac2e2b245f660c873aeefaa84eec3` |
| `flash-after-debug.bin` | 131072 | `2c8a01ca856d8bd02c08a0e9d24ebe9069d5df8abb5ff19a70903f14ede369af` |

## 阶段结论

阶段 1 的 Debug 构建、下载、复位运行、时基观察及默认输出检查通过，可作为下一阶段车轮底层开发的基础。用户已审核本次变更，并授权提交、推送到 `develop`。

下一阶段从 A 左轮方向、PWM 和停车行为开始，再验证 B 轮及两路编码器；需实际运动时明确执行器供电和架空条件。PID、速度环、转向环、视觉及赛题任务本轮未接入。
