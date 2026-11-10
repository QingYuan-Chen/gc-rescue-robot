# 基础车轮输入输出主机验证

编译实际 `tim.c`、HAL 时基、编码器、电机、周期任务和命令解析模块，只替换硬件和 RTOS。

```powershell
cmake -S tests/wheel_io -B build/tests-wheel-io -G Ninja -DCMAKE_C_COMPILER=C:/mingw64/bin/gcc.exe
cmake --build build/tests-wheel-io
ctest --test-dir build/tests-wheel-io --output-on-failure
python tests/wheel_io/test_check_board.py
```

8 项 CTest 覆盖定时器/引脚分配、上电零输出、限时到期及提前停车、编码器双向回绕与 64 位累计、PWM 极性和启停顺序、换向隔离、期限跨 tick 回绕、停止优先、周期/启动错误锁定、命令解析。配置和数值期望按硬件定义独立给定，测试开启 `-Wall -Wextra -Werror` 与断言。

Python 测试以串口替身运行只读检查工具，覆盖空闲通过、故障/活动命令拒绝及只发送 `ping`/`status`。不访问真实串口。

测试验证软件调用与计算，不仿真 STM32 内部 PWM 波形、芯片电压、电流或机械运动。真实方向、波形及上电/复位状态仍待上板测量。
