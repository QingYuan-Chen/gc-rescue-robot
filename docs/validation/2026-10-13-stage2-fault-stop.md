# 2026-10-13 阶段 2 故障停机修复

基线：`cf529bd07b0c47eb6c8e2799727c9d5b0d146836`，分支 `develop`。本记录对应阶段 2 故障停机修复增量；用户已确认提交并推送至 `develop`。用户授权烧录后，Debug 已完成下载、完整回读、零输出检查与复位复测；没有驱动车轮。

## 问题与修改

致命异常原来直接停在死循环中，CPU 停止调度后，TIM15 仍可能维持故障前的 PWM。普通 `wheel_drive_stop()` 只提交停车请求，依赖车轮任务继续执行，不能用于这种故障路径。

- 在 `bsp/motor` 增加 `motor_emergency_stop()`：检查外设时钟后直接访问固定寄存器地址，避免依赖尚未初始化的 HAL 句柄。
- TIM15 先将 A/B 空闲电平设为低，再清除主输出使能及自动恢复使能，并启用 OSSI；保留通道使能以保持空闲电平，然后停止计数、清零两路比较值。方向脚通过 BSRR 清零，最后执行 DSB 确认写入完成。
- `Error_Handler()` 先屏蔽中断，再调用该接口，随后停留至复位。NMI、HardFault、MemManage、BusFault、UsageFault 和 FreeRTOS `configASSERT` 统一进入该处理；停机路径不使用串口、延时、RTOS 或 HAL 函数。
- `wheel_drive.c` 的单行条件、错位注释及表达式换行已恢复规范格式。恢复后文件与基线逐字节一致，控制逻辑保持原样。

所有执行器输出写入仍由 `bsp/motor` 承担。新增故障接口只用于不可恢复故障，调用后不恢复正常控制；没有新增自动动作、故障注入任务或长期测试框架。三个 Core 文件仅修改 `USER CODE` 保留区，原 UTF-8、CRLF 格式保留；App 文件保持 UTF-8 无 BOM、LF。

## 离线验证

- STM32CubeCLT 1.20.0 / ARM GCC 13.3.1：`stm32-debug`、`stm32-release` 配置和构建通过，无编译告警。
- 使用 Clang 22.1.8 与 MinGW 头文件和库，直接编译实际 `motor.c`、板级映射及 `FreeRTOSConfig.h`，仅替代 HAL 和寄存器。16 种时钟/句柄状态组合、重复停机、方向脚掩码、无 HAL 调用，以及断言单次求值和错误入口路由均通过。主机模拟不能验证实际引脚电平及停机响应时间。
- 两份 ELF 的反汇编确认：五个异常入口均调用 `Error_Handler()`；该入口先关闭中断再调用停机接口；停机接口没有其他函数调用，并包含 DSB。
- App 文件的 `clang-format --dry-run --Werror`、`git diff --check` 及 Core 用户区边界检查通过。
- 本轮临时测试源码和可执行文件已删除，只保留检查结果。此前已烧录固件的快照哈希核对一致，没有覆盖。

| 配置 | BIN 字节数 | RAM_D1 静态占用 | BIN SHA256 |
| --- | ---: | ---: | --- |
| Debug | 61712 | 24536 | `679b4d1683c9d402a487f016d6a5bd475715c67a5822117af38974ad13bd1bb7` |
| Release | 63824 | 24544 | `078c6087c66b4b61159657a86cbaa03ee67f88be9c2db247d3e1569e081d3b32` |

本地证据位于 `build/stage2-fault-stop-validation/`：`host-check.json`、`firmware-check.json`、`fault-paths-debug.txt`、`fault-paths-release.txt` 和 `source-hashes.json`。这些文件不入 Git。

## 烧录与零输出验证

2026-10-13 14:13（UTC+8），用户重新连接 DAPLink 并授权烧录后，使用无线 SWD 100 kHz 完成 Debug 下载。增量构建返回 `ninja: no work to do`，源码及 BIN SHA256 与上述离线验证结果一致。

当前识别到 Horco DAPLink `FAED:4873 / 3FEE83BB`、UART `COM5`。实时读取核心板 IDCODE 为 `0x20036450`、UID 为 `0x0042001e 0x33335115 0x39333631`、Flash 为 128 KiB，与此前核心板一致。

烧录前串口第一次查询返回 `ERR line discarded; resend command`，重新同步后 `ping`、`status` 正常，双轮命令及 CCR 均为零；旧固件状态记录了 1 次接收错误。未据此修改串口代码或发送运动命令，烧录后的检查和复位复测中串口错误均为 0。

先完整备份 128 KiB Flash，确认备份与此前阶段 2 上板版本一致，再擦写、执行 OpenOCD 校验并完整回读。回读前 61712 字节与 Debug BIN 逐字节一致，其余区域为 `0xFF`。OpenOCD 按写入粒度将映像补齐到 61728 字节，仍以原始 BIN 和完整回读作为校验依据。

| 产物 | 字节数 | SHA256 |
| --- | ---: | --- |
| 烧录前完整备份 | 131072 | `d00406807e8c5b6c6f5b13104b32112f127af0ab9680742073ee7180bb96d008` |
| 烧录后完整回读 | 131072 | `8e061c8af35f1f2113798e02a4f046164cf7f24d44c30fe4668083255e851d70` |

上板检查仅涉及核心板和无线链路，不需要电机动作：

1. UART5 `ping`、`status` 正常；480 MHz 主频、1 kHz RTOS 节拍及 10 ms 任务周期符合预期，周期最小/最大值均为 10 ms，调度、串口接收、发送及溢出错误均为 0。
2. 双轮命令和剩余时间为 0；TIM15 两路 `CCR=0`，`PSC=239`、`ARR=999`，双通道与主输出已正常启动。A/B 方向、PWM 及未启用的 C/D PWM、舵机信号脚读取为低，TIM1 时钟关闭。
3. TIM2/TIM3 编码器定时器运行，CPU 的 CFSR/HFSR 均为 0。静止编码器读数为 0，不代表已验证运动反馈。
4. 再执行一次 `reset run` 后，日常板级检查再次通过，双轮继续保持零输出。最终核心板处于正常运行状态。

本轮证据和固件快照位于 `build/stage2-fault-stop-validation/flash-20261013-141128/`，包括 `context.json`、`source-hashes.json`、`probe.log`、`serial-precheck.json`、`backup.log`、`backup-check.json`、`flash-before.bin`、`flash.log`、`flash-after-debug.bin`、`flash-check.json`、`board-after.json`、`register-check.log/.json`、`board-after-reset.json` 及 `stm32-debug.bin/.elf/.hex`。首次串口查询的失败记录保留为 `board-before.json`。本轮辅助操作通过 Python 标准输入执行，没有新增测试源码或可执行文件。

## 验收边界

本次修复的 Debug 已完成烧录、回读、基础运行、零输出和复位复测。没有执行实机故障注入、测量 PWM 波形或驱动车轮，因此故障触发后的实际关断仍待专门验收；Release 尚未烧录。此前版本的历史结果见[原阶段记录](2026-10-12-stage2-wheels.md)。

此前架空测试中未观察到命令对应的编码器增量，其根因尚未确定，本次没有修改运动算法或声称解决该现象。车轮运动测试按用户要求保持暂停；阶段 2 仍未完成全部验收。烧录验证后，用户已授权将本次修复及验证记录提交并推送至 `develop`；本次不合并 `main`。
