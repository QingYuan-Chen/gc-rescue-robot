# 安全基础版主机验证

直接编译正式 `board_outputs.c`、`debug_console.c` 和生成的 `main.h`，仅替换 HAL/RTOS 与串口收发。

覆盖新版 M3/M4 引脚映射、输出模式切换前先清低电平、全部驱动/舵机引脚的高电平及模式错误检测，以及旧 wheel/PID/VOFA 命令被拒绝且不写输出。

```powershell
cmake -S tests/safe_baseline -B build/safe-baseline -G Ninja -DCMAKE_C_COMPILER=C:/mingw64/bin/gcc.exe
cmake --build build/safe-baseline
ctest --test-dir build/safe-baseline --output-on-failure
```

主机验证不能替代真实引脚测量、复位上电观察或烧录回读。
