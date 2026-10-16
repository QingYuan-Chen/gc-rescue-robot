# 车轮 PI 与控制规则主机回归

直接编译正式 `wheel_drive`、PID、车轮命令和状态格式化模块，只用主机替身提供电机输出、编码器输入及 RTOS 临界区。用于持续核对阶段 4 控制约定，不连接串口或 DAPLink，不进入 STM32 固件。

在仓库根目录使用 C11 主机 GCC、CMake 和 Ninja：

```powershell
cmake -S tests/wheel_control -B build/wheel-control-tests -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_C_FLAGS=-Og
if ($LASTEXITCODE -ne 0) { throw '主机配置失败' }
cmake --build build/wheel-control-tests
if ($LASTEXITCODE -ne 0) { throw '主机编译失败' }
ctest --test-dir build/wheel-control-tests --output-on-failure
if ($LASTEXITCODE -ne 0) { throw '控制规则测试失败' }
```

也使用独立构建目录验证 `-Ofast`。不要使用 ARM 编译器运行此主机测试；本机工具位置只在命令行或本地配置中指定。

三项测试直接使用正式 C 模块：

- `pi_contract`：4 组场景，检查显式积分时间尺度、Ki=0 的纯 P、零目标保留状态、正负饱和恢复、换向回算，以及取整不吞掉小积分。
- `wheel_control_contract`：21 组场景，检查初始化、参数边界、左右状态隔离、模式切换、停止优先、换向最终输出回算、长时间限幅、开环到期、tick 回绕、周期故障、协议及默认参数；覆盖独立目标输入、超过 2 秒的持续闭环、滑块冒号语法、旧限时速度命令拒绝、在线编辑/整组采用/回读、积分保留/清零、八通道同帧一致性、缓冲溢出与最大行宽。
- `vofa_console_contract`：运行正式控制台循环，模拟 115200/8N1 发送占用和逐周期采样，使用实际滑块格式 `pid edit left kp:542` 和 `wheel target left:2.500`，核对改参回执、波形持续发送、关闭采集后继续运行、主动停车、停车后改目标不重启、长文本轮询互斥及右轮不被启动。模拟结果不能作为无线吞吐或上板时延实测。

控制规则测试通过初始化接口传入独立测试参数，另用固件实际 `wheel_pid_defaults` 验证零增益状态。测试增益不是电机整定结果，不参与固件构建。断言不依赖 `assert` 或 `NDEBUG`，优化构建仍执行全部检查。

该测试不能证明真实电机响应、编码器物理正确性、RTOS 调度性能或速度闭环稳定性。新 PI 离线结果见 [重构验证](../../docs/validation/2026-10-15-wheel-pi-refactor.md)，调参接口见 [VOFA 初版验证](../../docs/validation/2026-10-15-vofa-interface.md)及[连续调参与滑块验证](../../docs/validation/2026-10-16-vofa-continuous-tuning.md)；[旧阶段 4 记录](../../docs/validation/2026-10-14-stage4-speed.md)不作为新算法硬件证据。后续由用户按 [VOFA 方案](../../docs/VOFA左轮调参.md)亲自操作 C 左轮。
