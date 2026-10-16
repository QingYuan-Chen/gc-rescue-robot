# VOFA 固件烧录恢复与停车验证

日期：2026-10-16。基线为 `develop` 的 `12990cab273032e0f19724e97089a4f1d649423f` 加未提交的 VOFA、持续闭环与滑块接口改动。用户要求修复失败的烧录，本次完成新 Debug 固件下载、回读和 UART5 查询；没有启动电机、编辑增益、调参、提交或推送。Release 未烧录。

## 故障定位与处理

原始日志中的 `cannot read IDR`、`DP initialisation failed` 表明当时 SWD 调试访问失败，随后擦除和复位无法完成；“上传完成，但存在问题”不能作为烧录成功依据。新建 OpenOCD 会话后，沿用工程的 100 kHz 配置恢复连接。没有复现物理链路故障，不能确定其来自无线收发器、供电还是接线，也不宣称已经永久消除链路中断。

本次确认的设备：

| 项目 | 读数 |
| --- | --- |
| CMSIS-DAP | Horco，VID:PID `FAED:4873`，序列号 `3FEE83BB` |
| UART | `COM5`，115200 / 8N1 |
| SWD DPIDR | `0x6ba02477` |
| STM32 ID | `0x20036450` |
| STM32 UID | `0x0042001e 0x33335115 0x39333631`，与历史核心板一致 |
| OpenOCD | xPack 0.12.0-7，`stm32h750vbt6.cfg`，100 kHz |
| GDB 端点 | `stm32h7x.cpu0` 监听 `127.0.0.1:3333`；AP2 的 GDB 服务禁用 |

另外单独复现了 CLion 内置 GDB 加载当前 ELF 时的 Windows ABI 警告。其配置为 Windows 目标加多架构支持，ARM ELF 的 OS ABI 自动落到 Windows；同一 ELF 用 STM32CubeCLT 的 `arm-none-eabi-gdb` 加载时为 `armv7e-m` / ABI `none`，无该警告。这是调试器选择问题，不能用它解释之前的 SWD ID 读取失败。

已将本机 `firmware/stm32/.idea/debugServers/DAPLink_OpenOCD.xml` 中的 Debugger 改为 STM32CubeCLT 的 ARM GDB；OpenOCD 配置、3333 端口及 `monitor reset halt` 保持不变。修改前备份位于本地日志目录，`.idea` 不入 Git。配置格式按本机 CLion 2026.1 的 `DebuggerData` 序列化实现核对。

当前会话没有可用的桌面控制运行入口，因此没有操作 CLion 的上传按钮。已在命令行使用配置中的同一 ARM GDB、OpenOCD、配置文件和端口完成连接、`load`、校验及复位运行。重新打开工程后，在 Debug Servers → DAPLink-OpenOCD 确认 Debugger 为 `arm-none-eabi-gdb`，再运行 `rescue.elf`；若 IDE 内存设置覆盖了磁盘配置，在该页面重新选择 ARM GDB。不要只改编译工具链而保留服务器中的 Bundled GDB。

## 固件、备份与回读

构建使用 `stm32-debug`，复核结果为 `ninja: no work to do`。当前固件标识：

| 文件 | 字节数 | SHA256 |
| --- | --- | --- |
| `build/stm32-debug/rescue.bin` | 68168 | `9d95064f0a2c432a1bea4856bfc48ab3401896ba74064e3ff931916d3e7d9091` |
| `build/stm32-debug/rescue.elf` | 按 ELF 容器保存 | `3b73e36e464010c17e944bd59bfcbd49b1ce496019f0df709fe5d69243d48445` |
| 烧录前 128 KiB Flash | 131072 | `1fb9a60a22f2b111b4f42aec67e91debd3267cd33c6eeaeffb80f7cd944013da` |
| 直接 BIN 下载后的完整 Flash | 131072 | `a35403930540a09b42137b4659624ff2ab08d13e8cc79dfea3b0ebb3d1afb264` |
| 最终 GDB ELF 下载后的完整 Flash | 131072 | `276687139a2d28b4f0662be5edeab66559269f87387e95ca41997519b5ae9228` |

烧录前备份包含旧 `wheel speed` 命令而没有 `wheel target`，确认板上仍是旧接口，不能用电脑已构建新版来推断板上已更新。

直接 BIN 下载后，`verify_image` 与 128 KiB 回读通过：前 68168 字节完全匹配 BIN，其余 62904 字节为 `0xFF`。

随后为复核 CLion 使用的 GDB 下载路径，执行 ARM GDB `load`，成功写入 68156 字节有效节内容。GDB 按 ELF 节下载，将两个对齐间隙保持为 Flash 擦除值 `0xFF`；默认 BIN 在这些间隙填 `0x00`：`0x08000298..0x0800029F` 共 8 字节，`0x0800F23C..0x0800F23F` 共 4 字节。因此直接拿默认 BIN 或 OpenOCD 的 ELF 段映像校验会报告这 12 字节不同，不是有效代码或数据丢失。

最终以 `arm-none-eabi-objcopy -O binary --gap-fill=0xff` 从同一 ELF 导出预期映像，`verify_image` 通过；完整 128 KiB 回读逐字节匹配该映像及尾部 `0xFF`。没有为消除填充差异修改源码、链接脚本或构建产物。最后执行 `reset run` 并结束本次启动的调试进程，核心板恢复运行。

## UART5 与停车结果

| 检查 | 实测结果 |
| --- | --- |
| `ping` | `OK pong` |
| `help` | 已列出 `wheel target`、`wheel start`、`pid edit/apply/get` 和冒号分隔说明 |
| `pid get left` | Kp/Ki/Kd 均为 `0.000`，`pending=0` |
| `wheel target left:0.1`，末尾 LF | 返回 `OK wheel command queued`；后续状态 `setpoint_left_milli=100` |
| 目标输入后的停车状态 | `mode=stop`、当前控制目标为零、左右 PWM 输出为零；没有启动电机 |
| 恢复目标 | 发送 `wheel target left:0`，确认保存目标恢复零 |
| `vofa status` | `enabled=0`、`channels=8`、`period_ms=10`；本次没有开启波形 |
| 最终复位后的 `tools/check_board.py` | 通过，`ready=1 fault=0 mode=stop`，两轮启用标志、请求、PI 状态及实际输出均为零 |
| 周期和串口状态 | 查询快照的周期、最小和最大间隔均为 10 ms，调度和串口错误计数为零 |
| PWM 寄存器 | 复位检查时 TIM12 CCR1/CCR2 均为零；运行后通过串口确认最终输出为零 |

实际目标验证时查询覆盖到约 40 秒运行时间，无周期故障；GDB 下载结束后再次复位并通过停车检查。没有用断点暂停期间的状态推断正常调度性能。任务栈读数仅为本次空闲查询快照，不能代替波形采集和运动负载下的栈验收。

## 本地证据与未完成项

本地备份、回读和日志位于 `build/vofa/flash-recovery/`，由 `.gitignore` 排除：`flash-before.bin`、`flash-after.bin`、`gdb-flash-after.bin`、`flash-write-verify.log`、`gdb-flash-check.log`、`gdb-verify-sections.log`、`uart-first-query.log`、`uart-target-check.log` 和 `board-check-final.json`。中间校验日志也保留，按上文解释其填充差异；没有把阶段性测试代码加入固件。

通过范围为烧录、有效程序内容回读、UART5 命令解析、目标保存和停车基础状态。CLion GUI 的下一次上传、VOFA+ 控件与连续八通道显示、非零增益 applied 回执、无线采集吞吐、真实持续闭环与运动停车、PID 整定、右轮与双轮完整验收仍未完成。后续运动和调参由用户决定并亲自操作，不把本次下载成功写成阶段 4 速度环验收完成。
