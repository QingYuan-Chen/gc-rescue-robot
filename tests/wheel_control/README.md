# 车轮控制规则主机回归

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

一套测试覆盖初始化、参数边界、零目标保留状态、单轮停用、两轮独立 PID、模式切换、每轮换向、停止优先、续期与超时、tick 回绕、周期异常锁定、命令解析及状态输出。断言不依赖 `assert` 或 `NDEBUG`，优化构建仍执行全部检查。

该测试不能证明真实电机响应、编码器物理正确性、RTOS 调度性能或速度闭环稳定性。硬件验收结果另记在 [阶段 4 验证记录](../../docs/validation/2026-10-14-stage4-speed.md)，按左轮 C、右轮 D、双轮顺序推进。
