# UART5 / VOFA+ 八通道调参接口验证

日期：2026-10-15 开始实施。基线：`develop` 的 `12990cab273032e0f19724e97089a4f1d649423f`。本记录对应基线上的未提交工作区差异，提交和烧录均待用户审核确认。

> 本文记录 2026-10-15 初版的验证与固件哈希。2026-10-16 已按用户要求替换限时速度入口，并支持滑块冒号命令；当前行为和新固件标识见 [连续调参与滑块验证](2026-10-16-vofa-continuous-tuning.md)，下述初版到期规则及产物标识不代表最新工作区。

## 范围与结果

已实现 [VOFA 左轮调参](../VOFA左轮调参.md)：UART5 115200/8N1、FireWater 八通道（采样时间、目标、反馈、误差、最终输出、Kp/Ki/Kd），100 Hz 目标采集/发送，32 帧有界缓冲；左轮参数草稿、整组采用和实际生效回读。Kd 固定为零，非零设置明确拒绝。回算、停止优先、到期停车、模式切换和右轮独立参数规则保留。

| 检查 | 结果 |
| --- | --- |
| 主机 GCC 15.2.0 / `-Og` | 3 个 CTest 目标全部通过，共 10802 项检查 |
| 主机 GCC 15.2.0 / `-Ofast` | 同样 3 个目标全部通过，共 10802 项检查 |
| ARM GCC 13.3.1 / `stm32-debug` | 配置与构建通过，自有模块 `-Wall -Wextra -Werror`，本次构建无告警 |
| ARM GCC 13.3.1 / `stm32-release` | 配置与构建通过，本次构建无告警 |
| 格式与文档 | 修改的 C 文件按仓库 clang-format 格式化，UTF-8 无 BOM、LF；差异与本地链接检查通过 |
| 烧录、无线串口、VOFA GUI、实际车轮 | 未执行，不能以离线模拟或构建代替验收 |

所有测试代码均在 `tests/wheel_control/`，不进入 STM32 固件。没有新增上电运动、自动整定或串口运动脚本；没有修改原工程、生成外设配置及接线。

## 离线证据

- `wheel_control_contract`：19 组场景、5420 项检查。包含已有控制规则，以及参数草稿不影响当前值、整组更新、延迟回执、重复申请 busy、拒绝无效输入/非零 Kd、右轮不被修改、运行时保积分、Ki=0 清积分、参数不续期、停止/到期/周期故障并发、同一帧参数/输出一致性、停车后的反馈误差、tick 回绕、缓冲溢出与时间戳缺口。
- `pi_contract`：4 组场景、4033 项检查，继续核对 PI 时间尺度、积分回算、Ki=0、正负饱和与普通 PWM 量化规则。
- `vofa_console_contract`：正式控制台循环，1349 项检查。模拟 UART 线占用和周期采样，产生 33 帧波形；210 ms 可读的停止输入在 220 ms 控制周期停车。该数值仅是确定性主机模拟，不是硬件停止时延。期间确认 queued 先于 applied、非零 Kd 拒绝、八通道参数回读、手动帮助/错误查询不伪造波形、VOFA 与周期长文本互斥、右轮始终无输出；关闭采集后恢复低频状态上报。

命令：

```powershell
cmake -S tests/wheel_control -B build/vofa/host-og -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_C_FLAGS=-Og
cmake --build build/vofa/host-og
ctest --test-dir build/vofa/host-og --output-on-failure -V

cmake -S tests/wheel_control -B build/vofa/host-ofast -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_C_FLAGS=-Ofast
cmake --build build/vofa/host-ofast
ctest --test-dir build/vofa/host-ofast --output-on-failure -V
```

上述 `gcc` 必须为主机编译器，命令应逐条检查返回码；本机编译器路径只通过命令行指定，不写入共享配置。构建日志与 CTest 结果位于 `build/vofa/host-*/Testing/Temporary/`。

## 实施中发现并修复

1. `-Ofast` 将单精度除以 1000 转成倒数相乘，使某些定点输入转换得到的增益出现额外浮点舍入误差。仅在参数采用时改用双精度缩放后转 float；草稿初始化也使用双精度舍入。控制周期中的 PI 运算保持单精度，两种优化等级回归一致。
2. [VOFA 官方 FireWater 源码](https://github.com/je00/Vodka/blob/master/dataengines/firewater/firewater.cpp)的冒号分支未检查数字转换是否成功，原帮助/错误回执中的冒号可能被当成采样。普通文本回执已移除冒号，只有波形使用 `vofa:`；控制台集成测试对全部非波形输出检查此约束。本机发现 FireWater 插件，但未启动 GUI 或把官方仓库代码当作本机 DLL 的运行验证。
3. 原调试循环每处理一个接收字节就进行上报调度，不适合连续波形。已改为优先批量接收（每轮最多 256 字节）、轮询参数应用结果，再发一帧波形；积压时不等待接收，采集中空闲最多等 1 ms。

## 资源与固件

最大字段宽度的 FireWater 行为 74 字节；100 Hz 为 7400 字节/秒，占理论 11520 字节/秒线速的 64.2%，未计入回执和手动状态查询。无线实际吞吐、丢样及调度抖动待实测。缓冲满时丢最旧样本、累计 dropped；关闭采集会主动清除待发样本。

用 Debug 实际编译参数额外输出静态栈报告，`robot_app_run` 为 24 字节、`wheel_drive_step` 为 72 字节、`vofa_stream_capture` 为 176 字节、`debug_console_run` 为 144 字节。报告存于 `build/vofa/stack/`，这些是单函数静态帧，未包含完整调用链、库函数、中断和调度现场，不能代替板上的 `robot_stack_free/debug_stack_free` 测量；任务栈配置未改。

| 预设 | BIN 字节数 / Flash 占用 | RAM_D1 占用 | BIN SHA256 |
| --- | --- | --- | --- |
| `stm32-debug` | 67696 | 26712 | `FC7158A9B3DB06A3D3E35D964DFE74FA4629CB5B215FCE6938021B44C03E89D8` |
| `stm32-release` | 69664 | 26712 | `AD0BE2D34CEFE961C5D24A0BCF8001857C761E8A16E80EDC086E5B982B521D55` |

构建入口仍为 `firmware/stm32/CMakePresets.json`，CMake 3.28.1、Ninja、STM32CubeCLT 1.20.0；BIN、ELF 和 HEX 位于仓库 `build/stm32-debug/`、`build/stm32-release/`，不进入 Git。

## 待上板

用户确认烧录后，先纯查询确认固件、10 ms 周期、停车与零增益默认，然后检查 VOFA 通道映射、采样间隔、参数生效回读、无线丢样与任务栈余量。用户再提供参数并亲自启动左轮，核对真实输出和跟随效果。未进行新 PI 的电机整定、右轮验证或阶段 4 完整验收。
