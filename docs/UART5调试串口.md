# UART5 无线调试串口

UART5 通过无线 DAPLink 提供命令响应、运行状态和参数读写入口。基础串口在 2026-10-12 已完成 Debug 上板验收。当前为 C 左轮、D 右轮，两路电机符号均为 -1，编码器符号为左 +1、右 -1。2026-10-14 已完成[编码器方向与增量专项验收](validation/2026-10-14-encoder-direction-increment.md)，修正版 Debug 已烧录并完整回读一致；用户逐路确认了实际左右轮。

**以下状态说明对应 2026-10-16 的新 PI、持续闭环与 VOFA 滑块接口；Debug 已烧录并通过停车、目标设置和基础周期查询，尚未整定。左右 Kp/Ki 上电默认均为零，未在线改参时闭环不会产生驱动输出；开环命令保持原有行为。** 当前烧录和串口结果见[恢复验证](validation/2026-10-16-vofa-flash.md)；旧算法的运动结果保存在[阶段 4 历史记录](validation/2026-10-14-stage4-speed.md)，不能作为新 PI 运动验收。新设计及验证见 [PI 控制设计](车轮PI控制设计.md)和[连续调参验证](validation/2026-10-16-vofa-continuous-tuning.md)。调参统一使用 C 左轮，由用户按 [VOFA 方案](VOFA左轮调参.md)亲自操作。

测试版保留为历史提交 `c7a7f1a`。[移除临时上电测试](validation/2026-10-14-wheel-startup-cleanup.md)的提交 `b16fa3f` 已按授权合入 `main`。当前源码仍默认零输出，通过显式命令启动开环检查或持续闭环，没有新增固件测试任务或上电运动程序。

## 接线与终端

| 拓展板 UART5 | STM32 引脚 | 连接到无线 DAPLink 的核心板端 |
| --- | --- | --- |
| 1 | PB13 / TX | RX |
| 2 | PB12 / RX | TX |
| 3 | GND | GND |

引脚来自 `hardware/Netlist_Schematic1_4_2026-09-18.tel`；使用 3.3 V TTL 信号。电脑端收发器连接 USB，2026-10-12 实测识别为 `COM5`，VID:PID 为 `FAED:4873`，设备序列号为 `3FEE83BB`。COM 编号可能变化，应按设备身份选择串口。

终端设置：**115200 波特率、8 数据位、无校验、1 停止位，无硬件流控**。开启发送换行；支持 CR、LF、CRLF。命令使用小写 ASCII，终端可开启本地回显，固件不逐字回显。

## 当前命令

| 命令 | 作用与示例回复 |
| --- | --- |
| `help` | 列出命令 |
| `ping` | 检查双向链路，回复 `OK pong` |
| `status` | 依次返回 `OK status` 基础状态、`OK wheel` 车轮状态两行 |
| `get report_ms` | 读取上报周期，例如 `OK report_ms=0` |
| `set report_ms 1000` | 每 1000 ms 上报一次 `status` |
| `set report_ms 0` | 关闭自动上报 |
| `vofa on` / `vofa off` | 开关八通道 FireWater 采集；开启时关闭 `report_ms`，不改变运动 |
| `vofa status` | 查询采集、发送、积压和本地丢样统计 |
| `pid edit left <kp\|ki\|kd>:<值>` | 编辑参数草稿，冒号也可用空格；Kp/Ki 为 0～10000、最多三位小数，Kd 只接受 0 |
| `pid apply left` | 整组提交；先返回 `queued`，周期任务处理后返回 `applied` 或拒绝原因 |
| `pid get left` | 查询实际生效 Kp/Ki/Kd；Kd 为零且 `d_enabled=0` |
| `wheel status` | 查询目标参数、当前目标、双轮输出、编码器及开环剩余时间 |
| `wheel stop` | 提交双轮停车命令，正常调度下最迟在下一个 10 ms 周期执行 |
| `wheel left 150 500` | 左轮 C 以 +150 千分比开环运行 500 ms，右轮 D 输出为零 |
| `wheel right -150 500` | 右轮 D 以 -150 千分比开环运行 500 ms，左轮 C 输出为零 |
| `wheel both 150 150 500` | 参数依次为左轮 C、右轮 D、时长；此例双轮均以 +150 千分比运行 500 ms，仅在单轮验收后使用 |
| `wheel target left:3.000` | 设置左轮目标为 +3 counts/10 ms；未启动时只保存，已闭环时下一周期采用 |
| `wheel target right:-3.000` | 设置右轮目标，左轮参数保持不变；不改变当前启用侧 |
| `wheel target both 3 3` | 同时设置左右轮目标；不启动或改变当前模式 |
| `wheel start left` / `wheel start right` | 使用已保存目标启动指定轮持续闭环，另一轮停用且输出为零 |
| `wheel start both` | 按各自已保存目标启用双轮持续闭环，单轮验收后使用 |
| `wheel target left:0` | 左轮已启动时维持闭环零速目标，仍可能纠偏出力；要退出控制使用 `wheel stop` |

上表保留各命令的接口语义，运动示例不代表已经配置好新 PI 参数。右轮和双轮能力继续保留，但不自动进入调参流程。目标为 0 时仍显示 `mode=speed` 和启用状态，不代表车轮已机械停稳。

`report_ms` 允许 `0` 或 `100`～`60000`，单位为毫秒；只存 RAM，复位后为 `0`，自动上报依次输出上述两行。它不改变车轮任务的 10 ms 周期；VOFA 采集开启时拒绝非零 `report_ms`。左轮增益可在线整组更新，实际值随八通道波形逐帧回读；参数仅存 RAM，复位恢复 `App/config/wheel_pid_config.h`。参数单位、应用时机、回执以及控件模板见 [VOFA 左轮调参](VOFA左轮调参.md)。

控制模式、比赛动作及未来异常接入遵循 [底盘控制规则与比赛动作](底盘控制规则与比赛动作.md)。`duty=0` 为开环零输出，速度目标 0 则保持闭环；显式停止撤销请求并清 PID。普通停止不锁定故障，下次明确新请求可启动；周期异常则锁定停车，不接受开环或闭环请求，排查后复位恢复。

速度目标单位是每 10 ms 的编码器计数，允许 -10～10，最多三位小数，例如 `0.250`；不接受指数、NaN、Inf 或省略整数位的 `.5`。解析后以千分之一计数保存，送入 PID 时恢复为浮点 counts/10 ms。该输入范围覆盖此前开环观测量级，不能理解为已验证的可达速度。PI 直接输出占空比，闭环统一限制为 ±250 千分比，并将限幅与换向后的实际输出用于积分回算；开环仍为 ±300 千分比。

启动前确认电机供电、两个驱动轮架空。闭环通过目标参数和开始命令分别控制，启动后无到期停车，停止发送命令、关闭波形或断开 VOFA 均不会自动停止；结束调试发送 `wheel stop`。旧 `wheel speed ... <ms>` 接口已删除。仅开环检查保留 10～2000 ms、10 ms 步长的时长约束，超范围拒绝，其期限从采纳开始并包含模式过渡和换向。

命令回复 `queued` 表示已交接，并不等同于已经采用；用 `wheel status` 查看实际状态。同一周期停止优先，已排队停止不能被启动请求覆盖；此时目标参数可保存但不会启动电机。开闭环互切先零输出一个完整周期并清 PI，每轮输出直接变号时单独保留一个零输出周期。正常闭环目标更新、重复开始同一组轮子均保留积分；停止后重新开始清控制状态并采用已保存目标。复位后目标与增益均恢复默认零值，上电不提交启动请求。

命令的正值约定为车体前进。两路电机符号均为 -1，正命令对应 IN1 低、IN2 高；前进时左轮 TIM4 原始计数增加、右轮 TIM8 原始计数减少，因此编码器符号分别为 +1、-1。状态字段按 `left/right` 区分：`duty_left/right` 为控制层实际采用的有符号占空比千分值，不因板级电机符号取反而改变；`delta_left/right` 是最近一次 10 ms 采样的增量，`total_left/right` 是启动以来累计计数，两者均统一为前进正、后退负；`raw_left/right` 是未做方向修正的硬件计数器原值，范围为 0～65535。低频串口上报不会保存每一帧 10 ms 样本，累计值用于观察整个动作的计数变化，不能把若干上报帧的 `delta` 相加当作整个动作的总量。

新增状态字段：

| 字段 | 含义 |
| --- | --- |
| `mode`、`source` | 模式为 `stop/open/speed`；活动请求来源为 `debug`，停车时为 `none` |
| `tick_ms`、`interval_ms` | 本次采样 tick 和与上次采样的间隔；初始化基线的间隔为 0 |
| `ready`、`fault` | 时间基线是否建立、是否锁定周期故障；正常可控时为 1、0 |
| `transition` | 开闭环互切的零输出周期；此时 `mode` 已显示目标模式，PID 尚未运行 |
| `enable_left/right` | 该轮是否参与当前模式；单轮闭环未测侧为 0，双轮零目标仍为 1 |
| `remaining_ms` | 仅表示开环剩余时长；持续闭环及停车为 0，必须结合 `mode` 判断是否运行 |
| `setpoint_left_milli`、`setpoint_right_milli` | 已保存的左右目标参数，单位为千分之一计数/10 ms；普通停车保留，复位清零 |
| `target_left/right_milli` | 左右目标的千分之一计数/10 ms；例如 `3000` 表示 3 counts/10 ms；完整字段名为 `target_left_milli`、`target_right_milli` |
| `controller`、`output_unit` | 固定为 `pi`、`permille`，标识当前控制器和输出单位 |
| `p_left/right` | 本周期比例项，占空比千分比 |
| `i_left/right` | 本周期输出使用的积分项，占空比千分比 |
| `i_next_left/right` | 正常积分及约束回算后，留给下一周期的积分项，占空比千分比 |
| `pi_left/right` | 未限幅的 P+I 输出，占空比千分比，显示三位小数 |
| `request_left/right` | 已施加 ±250 限幅及向零取整、尚未施加换向间隔的请求；开环为原请求 |
| `limited_left/right` | 本周期是否因 ±250 千分比幅值限制而改变浮点输出；普通取整不置位 |
| `reverse_left/right` | 本周期是否处于对应轮子的换向零输出间隔 |
| `stop` | 最近停止原因为 `boot/command/expired/period`；新动作不清历史原因，当前是否停车看 `mode` |
| `reject` | 最近控制接口拒绝原因为 `none/invalid/not_ready/stop_pending/period_fault/busy`；合法请求不清历史，语法错误仅返回 `ERR` |

旧 `pid_left/right` CCR 字段已删除，不保留别名；历史日志按旧版本解释。当前满足 `pi=P+I`，而 `i_next` 是下一周期状态，不能拿它替换本周期的 I 做求和。

停止、到期和模式切换不清编码器累计值。`mode=stop`、PID 已清理与机械停稳分别判断；停稳范围与连续采样窗口尚未实测，当前没有“已停稳”或“动作成功”字段。周期异常含 `osDelayUntil` 失败或实际采样间隔不等于 10 ms，优先停车、清 PID，继续采样与上报，之后运动请求返回 `ERR wheel period_fault_reset_required`。普通 `wheel stop` 不能清除这个故障。

需要现场核对左右和方向时，一次只发送一路限时命令，停车并确认实际车轮后再测下一路。以“C 通道 / 软件 left”或“D 通道 / 软件 right”标识当前动作，避免连续切换两路后把用户观察对应到错误指令。

旧 `wheel a/b` 命令及 `*_a/b` 状态字段已删除，不保留别名。板级映射为 C 左轮：PB15 / TIM12_CH2、PD12/PD13 / TIM4；D 右轮：PB14 / TIM12_CH1、PC6/PC7 / TIM8。

`status` 中 `interval`、`min`、`max` 单位为 RTOS tick，目前 1 tick = 1 ms；`robot_stack_free`、`debug_stack_free` 单位为字节。`delay_errors` 记录错过的周期调度点，`rx_overflow` 记录接收队列丢弃字节数，`rx_errors` 和 `tx_errors` 记录串口接收/发送错误。所有计数复位后清零。

调试命令采用“发送一条，等待回复”的方式，`pid apply left` 还须等待最终 `applied` 或拒绝回执。单行最多 95 字节；超长行、非 ASCII 控制字节及发生丢字节的残缺行会被丢弃，并在行结束时返回 `ERR`。输入错误不会改变已有参数。大量无等待查询会超过有限队列的承载能力，发生丢字节时先发送换行，再重发完整命令。下行命令不用于高频控制，上行波形由独立采样缓冲按 100 Hz 目标发送。

## 模块职责与后续接入

- `App/bsp/debug_uart`：唯一 UART5 收发模块，中断收发、接收队列、错误恢复与计数。
- `App/debug/debug_console`：调试任务、文本命令与状态上报；不直接写电机、舵机或其他模块内部变量。
- `App/debug/wheel_commands`：车轮命令解析，调用 `wheel_drive` 公开接口；不直接写 GPIO、PWM 或串口。
- `App/debug/wheel_status`：读取车轮快照并格式化状态，不参与控制计算；控制台复用任务私有的静态响应缓冲区。
- `App/debug/pid_commands`：管理参数草稿、校验和异步采用回执，只调用控制层公开接口。
- `App/debug/vofa_stream`：采样缓冲、八通道格式化与统计，控制任务不格式化或发送串口数据。
- `App/runtime/runtime_monitor`：保留 10 ms 周期观测，通过临界区保护的公开快照提供状态。
- `App/config/debug_uart_config.h`：UART5 句柄映射、队列容量与发送超时；引脚、波特率、中断配置由 `rescue.ioc` 生成，不重复维护寄存器配置。

UART5 不使用 DMA；旧 UART5 RX DMA 配置已从 `.ioc` 与生成代码移除。调试任务栈为 3072 字节、优先级为 `osPriorityLow`，基础任务为 `osPriorityBelowNormal`；串口等待通过队列或任务标志让出 CPU。HAL 回调只处理 UART5，不占用 USART2 视觉串口或 USART3 IMU 串口。

当前 CubeMX 在仅启用 UART5 自动初始化时没有自动保留 `main.c` 中的 `usart.h` 引用，因此该引用放在 `USER CODE Includes` 中。已在副本中重新生成并核对相关 Core 文件，后续生成时保留用户区。

## 复测

2026-10-16 新 PI 的 Debug 烧录后，已使用当前工具通过纯查询检查；本次没有启动电机或调参。纯查询只需核心板供电，转轮另需电机供电及对应授权。电脑 Python 环境需要 `pyserial`，在仓库根目录运行：

```powershell
python -X utf8 tools/check_board.py --port COM5
```

只保留这一个日常板级检查脚本，查询链路、10 ms 周期及停车控制状态，结果保存在 `build/board-check.json`，不发送运动命令。关闭其他占用串口的终端。脚本要求 `ready=1`、`fault=0`、`mode=stop`，同时核对请求、实际输出、PID 和期限已清理，不能用闭环瞬时零输出冒充停车。它只接受 `controller=pi output_unit=permille` 并检查 P、I、下一周期 I 和输出均已清理；旧阶段 4 固件使用提交 `57ab4f7` 的脚本，历史阶段 2 固件使用 `41e0e1f` 的脚本，不在当前工具中堆叠兼容分支。2026-10-14 的左轮振荡期间 `fault=0`，因为当前仅检测周期异常，尚未实现跟随失败或振荡检测；这个字段和检查脚本通过均不能代替速度跟随验收。

阶段性串口压力、复位与解析测试脚本已移除；通过的输入和结果保留在历史验证记录中。后续每个阶段按实际变更补充检查，避免长期堆积一次性测试程序。

基础串口的烧录、完整 Flash 回读、人工注入队列溢出与复位检查证据见[无线串口验证记录](validation/2026-10-12-uart5-debug.md)。历史 A/B 车轮固件的烧录、回读及零输出复测见[阶段 2 验证记录](validation/2026-10-12-stage2-wheels.md)；初次切换 C/D 的记录见[C/D 同步记录](validation/2026-10-13-cd-wheel-mapping.md)，电机左右与方向修正见[修正记录](validation/2026-10-14-wheel-direction-fix.md)，当前编码器符号及实测结果见[专项验收记录](validation/2026-10-14-encoder-direction-increment.md)。
