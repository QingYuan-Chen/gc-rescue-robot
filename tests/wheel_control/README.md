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

两项测试直接使用正式 C 模块：

- `pi_contract`：4 组场景，检查显式积分时间尺度、Ki=0 的纯 P、零目标保留状态、正负饱和恢复、换向回算，以及取整不吞掉小积分。
- `wheel_control_contract`：15 组场景，检查初始化、参数边界、左右状态隔离、模式切换、停止优先、换向最终输出回算、长时间限幅、续期与到期、tick 回绕、周期故障、协议及默认参数。

控制规则测试通过初始化接口传入独立测试参数，另用固件实际 `wheel_pid_defaults` 验证零增益状态。测试增益不是电机整定结果，不参与固件构建。断言不依赖 `assert` 或 `NDEBUG`，优化构建仍执行全部检查。

该测试不能证明真实电机响应、编码器物理正确性、RTOS 调度性能或速度闭环稳定性。新 PI 离线结果见 [重构验证](../../docs/validation/2026-10-15-wheel-pi-refactor.md)；[旧阶段 4 记录](../../docs/validation/2026-10-14-stage4-speed.md)不作为新算法硬件证据。后续调参统一使用 C 左轮，由用户亲自操作并另行提供具体方案。
