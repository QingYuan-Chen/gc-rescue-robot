# 2026-10-13 C/D 车轮映射同步

基线：`1c0aff8d5d2d2c618f1313fc3b32a09f616d9b09`，分支 `develop`。用户将电机换到原 C/D 插座，并确认“C 是右轮，D 是左轮”。本次在已有未提交的上电测试接入上同步代码、配置和文档，未烧录、未提交、未推送。

> 后续状态：2026-10-14 重新插接后 SWD 恢复，C/D Debug 烧录、完整回读、启动输出与串口停车检查通过，详见[烧录记录](2026-10-14-cd-wheel-flash.md)。以下保留 2026-10-13 的修改与连接失败记录。

## 当前接线

| 逻辑轮 | 电机插座 | PWM | IN1 / IN2 | 编码器插座第 3 / 4 脚 | 编码器定时器 |
| --- | --- | --- | --- | --- | --- |
| 左轮 | D / ENCODERD1 | PB14 / TIM12_CH1 | PD2 / PD1 | PC6 / PC7 | TIM8_CH1 / CH2 |
| 右轮 | C / ENCODERC1 | PB15 / TIM12_CH2 | PD3 / PD4 | PD12 / PD13 | TIM4_CH1 / CH2 |

依据为用户确认及 `hardware/Netlist_Schematic1_4_2026-09-18.tel`。网表 `CO1/CO2` 连接 ENCODERD1，`DO1/DO2` 连接 ENCODERC1，不能按电机网络字母推断编码器插座。两路编码器均采用 TI12、16 位全量程 `ARR=65535`、输入滤波 10；PWM 保持 `PSC=239, ARR=999`，对应当前时钟下的 1 kHz。

## 修改范围与行为

- 板级映射集中在 `App/config/wheel_board.h`；业务标识统一为 `WHEEL_LEFT/WHEEL_RIGHT`，命令参数与状态数组按左、右排列。
- `main.c` 与 `.ioc` 启用 TIM4、TIM8、TIM12，取消 TIM2、TIM3、TIM15 初始化调用。启动保持模块改为将未用 A/B 的 PE5/PE6 拉低，不再覆盖 C/D 的 PB14/PB15 复用配置；舵机保持未启动。
- TIM12 没有 TIM15 的主输出关断机制。致命故障入口先将 PB14/PB15 预置低电平并切为推挽 GPIO，再停用 TIM12 通道和计数器、清除比较值与 GPIOD 四个方向脚；继续由 `bsp/motor` 唯一执行关断，不依赖 HAL 句柄或任务调度。
- 串口命令改为 `wheel left <duty> <ms>`、`wheel right <duty> <ms>`、`wheel both <left> <right> <ms>`；状态字段改为 `duty_left/right`、`delta_left/right`、`total_left/right`、`raw_left/right`，不保留旧 A/B 别名。控制台帮助、日常板级检查工具和当前文档同步更新。
- 保留当前排查所需的临时上电行为：双轮各 30% 固定占空比、60 秒后停止，普通命令或 `wheel stop` 可覆盖，停车后不会自动重启。普通手动命令仍限 ±300 千分比、10～2000 ms、10 ms 步进；测试结束后删除启动请求，恢复默认零输出。

这是开环输出，不能保证恒定转速。电机和编码器方向符号暂为 +1，新接线的物理前进方向仍须逐轮确认。

## 已完成验证

1. `stm32-debug`、`stm32-release` 预设配置与构建通过，最终构建无告警；两者 RAM_D1 静态占用均为 24536 字节。
2. 编译运行实际 `motor.c`、`encoder.c`、`board_startup.c`、`wheel_drive.c`、`wheel_commands.c`，通过 7 组主机检查：左右 PWM 与方向独立映射、两路编码器正反回绕、启动输出与超时、左右命令及覆盖/换向/停车/旧命令拒绝、初始化失败不启动、未用输出保持、故障关断与时钟未开启及重复调用。
3. 实际 `tools/check_board.py` 的串口接口使用离线替身：新左右轮字段下零输出通过，非零输出按预期拒绝；未打开真实串口。
4. CubeMX 在临时副本重新生成后，`main.c`、`tim.c/.h`、`freertos.c` 四个文件的非注释内容与当前工程一致；`.ioc` 初始化选择与 `main.c` 调用一致。本机仍出现已知扩展包 PDSC 兼容和 Makefile 导出 `sysmem.c/syscalls.c` 路径重复诊断，仅确认上述 Core 一致性，不将完整 IDE 导出记为通过。
5. Debug ELF 中已仅保留本阶段需要的 TIM4、TIM8、TIM12 初始化和 TIM17 时基；故障停机反汇编为直接寄存器操作，不调用 HAL/RTOS。
6. 修改模块通过 clang-format 和差异空白检查；UTF-8 编码保留，Core 与 `.ioc` 的 CRLF 保留，App 与文档保持 LF。一次性主机测试源码、可执行文件和本次 CubeMX 临时副本均已销毁。

| 固件快照 | BIN 字节数 | SHA256 |
| --- | ---: | --- |
| C/D 临时测试 Debug | 61616 | `2249e4d31a10e1b47ac0f0a56168fe286d4a148b7da9487b9b2f057925fc0fd7` |
| C/D 临时测试 Release | 63784 | `42f3f4789318aa66ccf7d8d31d4f2a2412fbcb9099d546748b2daa351ecb060b` |

本地证据及固件快照保存在 `build/stage2-cd-mapping/validation-20261013-213051/`：`build-check.json`、`build-debug.log`、`build-release.log`、`host-check.json`、`tool-offline-duty-0.json`、`tool-offline-duty-150.json`、`cubemx-check.json`、`cubemx.log` 和 `cd-wheel-test-debug/release.bin/.elf/.hex`。非零输出工具用例的 `passed=false` 是预期的停车检查拒绝。

## 待上板验证

接线同步时未连接硬件、未复位或发送运动命令；随后按用户要求尝试烧录，但未能建立 SWD 连接，详见下节。最近一次成功烧录仍为 [A/B 上电测试版本](2026-10-13-wheel-startup-test.md)，上述新固件构建和主机检查不代替新接线的烧录校验、实际 PWM、电机方向、编码器反馈及故障停机实测。

下次烧录此临时固件后，上电会自动启动 C/D 双轮测试。实际转动需要电机驱动供电、驱动轮架空；先逐轮确认 D 左、C 右的驱动与反馈对应，再确认正方向和停止行为。阶段 3 的 PID 移植仍等待阶段 2 验收。

## 当日烧录尝试：SWD 连接未建立

用户要求“烧录一下”后，重新执行 Debug 增量构建，源码及 BIN/ELF/HEX 与上述已验证快照一致。电脑端实时识别无线 DAPLink 为 `FAED:4873 / 3FEE83BB`，串口为 `COM5`。

首次串口查询返回 `ERR line discarded; resend command`；发送换行重新同步后，`ping` 与连续两次 `status` 查询成功，采样计数持续增加，10 ms 周期正常，旧 A/B 两轮占空比均为 0。接收错误累计为 1，后续查询没有增长；调度错误、发送错误及接收溢出均为 0。仅据此确认串口链路与旧固件仍在运行，不推断该次接收错误的具体原因。

OpenOCD 能打开同一 DAPLink 的 CMSIS-DAPv2 接口，但在 100 kHz 两次、50 kHz 一次的 SWD 身份探测中均返回 `Error connecting DP: cannot read IDR`，未取得核心板 IDCODE/UID。因此没有进行 Flash 备份、擦除或写入，也未复位核心板或发送运动命令；C/D 固件尚未烧录。下一步需要检查核心板端接收器到 SWD 插座的连接，再重试身份探测和烧录。

证据目录：`build/stage2-cd-mapping/flash-20261013-221448/`。包含 `context.json`、`firmware-check.json`、`serial-precheck.json`、三次 `probe-attempt-*.log`、`connection-check.json` 及待烧录固件快照。本次未提交或推送。
