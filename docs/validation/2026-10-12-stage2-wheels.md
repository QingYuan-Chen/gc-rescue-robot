# 2026-10-12 阶段 2 与测试程序清理

基线：`aea769226df1e4aa1c62524b839f17230649360a`。本记录对应 `develop` 上的阶段 2 开发增量，用户已确认提交和推送；运动验收边界见下文。

## 当前结论

A/B 电机输出、编码器增量采样、10 ms 车轮任务及无线限时调试命令已实现。Debug/Release 构建和实际模块的主机逻辑检查通过。用户重新连接 DAPLink 后，**Debug 烧录、128 KiB 完整回读、10 ms 周期、双轮零输出及复位复测通过；非零物理输出与编码器方向仍待验收**。Release 尚未烧录。

## 测试程序清理

| 对象 | 处理及原因 |
| --- | --- |
| `tools/test_debug_uart.py` | 已删除；完整串口验收已留档，不再作为每阶段必跑程序 |
| `tools/check_board.py` | 唯一保留的日常板级检查工具：链路、周期、停车状态，不发运动命令 |
| `runtime_monitor_run()` | 已删除独立观察循环；由实际车轮任务调用轻量周期统计 |
| UART5 调试控制台 | 保留，用于本阶段动作验收和后续调参，不是一次性测试任务 |
| 阶段 1、UART5 的日志、固件快照、Flash 备份、验证文档 | 保留，用于追溯和恢复 |
| `build/stage1-cubemx-check/rescue/`、`build/stage1-regeneration/rescue/`、`build/uart5-regeneration/rescue/` | 三套生成验证副本约 30 MB，拟删除；两次清理命令被自动审批以 `blocked by policy` 拦截，文件仍保留 |
| 空的 `tests/`、`build/uart5-host/`、历史一次性 `.cfg` | 与上述清理一起被拦截，尚未删除 |
| 旧 `gc_rescue/Motor/sin_generate.c/.h` | 不参与构建，拟清理的旧正弦测试；同批删除被拦截，当前仍保留 |

本轮临时主机测试放在操作系统临时目录，仅结果记录留在 `build/stage2-validation/host-check.json`，没有新增另一套长期测试框架。CubeMX 检查副本位于 `build/stage2-regeneration/`；由于当前删除操作受阻，该副本也尚未清理。以上清理不涉及同级原工程 `../gc_rescue/`、用户 `tmp/` 或第三方库。

## 实现与边界

- `bsp/motor` 是 A/B 的唯一实际输出写入模块；TIM15_CH1/CH2 分别对应 A/B。底层接口为 -1000～1000 千分比，零值同时清除比较值和方向脚。停转/换向时先将 PWM 写零，避免方向翻转时继续沿用非零脉冲；不接入 TIM1 舵机输出。
- 原 TIM15 的 `PSC=239, ARR=999` 保留，在当前 240 MHz 定时器时钟下为 1 kHz。原 CH1 初始比较值 100 改为 0，并同步 `.ioc`。
- `bsp/encoder` 只读取 A/TIM3、B/TIM2，使用连续计数差值，处理 16/32 位跨零回绕，不再读后清零。每次采样位移须小于对应计数范围的一半；保持原滤波值 10。
- `control/wheel_drive` 独占硬件调用，每 10 ms 采样并更新双轮输出；调试任务通过临界区保护的命令及快照交接。
- 手动命令限制 ±300 千分比、10～2000 ms（10 ms 步进），到期停车；反向前先保持 10 ms 零输出；错过周期时取消当前命令并重新对齐。启动无自动动作。
- `debug/wheel_commands` 单独承担命令解析，避免向控制台继续堆积硬件逻辑。`status` 和周期上报增加第二行车轮状态。
- 引脚、通道和符号在 `config/wheel_board.h` 集中维护，周期及手动限值在 `config/wheel_config.h`；A/B 方向暂沿用原 IN1 高、IN2 低为正，编码器不取反，**仍待实车确认**。
- 未引入 PID、速度环、转向环或赛题任务。尚需提取的旧 PID 参数及其原调用流程继续作为不链接的迁移参考；车轮底层完成实测接管后再清理对应旧实现，避免提前丢失算法/单位来源。

## 软件验证

工具链为 STM32CubeCLT 1.20.0 / ARM GCC 13.3.1，两个 CMake 预设通过，新模块启用 `-Wall -Wextra -Werror`，没有编译告警。

| 配置 | Flash / BIN 字节 | RAM_D1 静态占用 | BIN SHA256 |
| --- | --- | --- | --- |
| Debug | 62176 | 24536 | `46ea9213bf6e8f5a49032af70aef09c6d8861e347eb36589cef026d1353c7245` |
| Release | 64000 | 24544 | `f2201a0381be92dd7aa2724a5933f98d6d64ccfab388c41c129efb6874b0584d` |

主机使用 Clang 22.1.8 配合 MinGW 头文件/库，直接编译实际 `motor.c`、`encoder.c`、`wheel_drive.c`、`wheel_commands.c`，仅替代硬件寄存器及 RTOS 临界区接口。检查通过：

1. A/B 通道独立、正负方向映射、千分比限幅、零值停车。
2. 16 位及 32 位编码器在零点两侧的正反向差值。
3. 限幅/时长拒绝、定时停止、换向零输出间隔、显式停车。
4. 完整单轮/双轮命令、超大数值/多余字段/非法时长拒绝。

主机模拟不能验证电路、脉冲波形、电机物理方向或实际调度。CubeMX 副本再生成后，`main.c`、`tim.c/.h`、`freertos.c` 共 4 个文件非注释内容与工程一致；本机生成器仍有此前记录的扩展包与 Makefile 导出诊断，不将完整 IDE 导出记为通过。

## 上板验证：烧录与零输出

首次尝试时，电脑能枚举 Horco DAPLink（`FAED:4873 / 3FEE83BB`）及 COM5，但两次 OpenOCD 初始化均返回 `CMSIS-DAP command CMD_INFO failed`，UART `ping` 超时。当时失败发生在 Flash 备份/擦写之前，未写入新固件；原始日志保留为 `build/stage2-validation/flash.log`、`reconnect.log`。

2026-10-12 约 23:00，用户重新连接 DAPLink 并授权烧录后，使用同一 OpenOCD 配置和 100 kHz SWD 成功连接。实时读取 IDCODE 为 `0x20036450`，UID 为 `0x0042001e 0x33335115 0x39333631`，Flash 容量 128 KiB，与此前核心板记录一致。本次没有修改固件源码，增量构建返回 `ninja: no work to do`，烧录 BIN 与上述 Debug 校验值一致。

先备份完整 Flash，再擦写 Debug BIN、执行 OpenOCD 校验并完整回读。回读的前 62176 字节与 BIN 一致，其余字节均为 `0xFF`；烧录前备份与此前已验收 UART5 固件的完整 Flash 一致。

| 本地证据（位于 `build/stage2-validation/`） | 字节数 | SHA256 |
| --- | ---: | --- |
| `flash-before.bin` | 131072 | `d16c06ef22f3dd4563f09849e040e64ba5ef22e764574b9a52dcfdc090959619` |
| `flash-after-debug.bin` | 131072 | `d00406807e8c5b6c6f5b13104b32112f127af0ab9680742073ee7180bb96d008` |

完成以下板级检查，仅需核心板及无线收发器供电；本次未确认电机供电条件，也未发送任何非零输出命令：

1. `tools/check_board.py --port COM5` 通过：`ping` 正常，`status` 返回基础状态和车轮状态两行；时钟 480 MHz，RTOS 节拍 1 kHz，周期最小/最大值均为 10 ms，调度、串口收发及溢出错误均为 0。
2. 实际寄存器检查通过：TIM15 两路 PWM 已启动，`PSC=239`、`ARR=999`、`CCR1=CCR2=0`；A/B 方向脚、PWM 引脚及未启用的 C/D、舵机信号脚的输入回读和输出锁存值均为低。TIM1 时钟未开启。
3. TIM3/TIM2 编码器计数器已启动，静止状态原始计数均为 0；CFSR/HFSR 均为 0。该检查不能证明编码器运动方向及实际脉冲反馈正确。
4. 烧录后启动，再执行 1 次 `reset run`，复位后日常板级检查再次通过。静止状态发送 `wheel stop` 返回 `OK wheel stop queued`；后续两次状态采样计数从 3297 增至 3401，双轮占空比、剩余时间及编码器计数始终为 0。此项仅验证停车命令接入和静止状态，不代表已验证运动中停车。
5. 本次观测到车轮任务、调试任务最小剩余栈空间分别为 884、1232 字节；未进行带运动或串口压力条件下的栈验收。

成功连接和烧录日志：`probe-retry.log`、`flash-retry.log`。结果记录：`flash-check.json`、`board-check.json`、`board-check-after-reset.json`、`register-check.log/.json`、`zero-output-check.json`。以上文件均保留在 `build/stage2-validation/`；`source-hashes.json` 为烧录前源码与文档快照，本次随后仅同步更新验证文档和当前进度说明。

## 下一步

实际运动前确认两驱动轮架空及电机供电；按 A 轮方向/PWM/停车 → B 轮 → A 编码器 → B 编码器 → 双轮顺序验收。本次没有验证非零 PWM 波形、正反转、运动中停车、换向间隔或编码器物理方向，阶段 2 仍未完成全部验收，不能进入阶段 3。
