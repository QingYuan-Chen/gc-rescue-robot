# 工程恢复与构建记录

## 2026-10-11 当前重构入口

本仓库的 `firmware/stm32/` 为后续逐层重构目录，构建方法与目录说明以 [仓库 README](../../README.md) 为准。仓库主分支为 `main`，远端为 `QingYuan-Chen/gc-rescue-robot`。

同级原工程 `../gc_rescue/`（相对仓库根目录）已在新路径下完成 53/53 步构建，并通过无线 CMSIS-DAP 烧录和完整 Flash 回读校验。固件 BIN 为 40,664 字节，SHA256 为 `ca3a763e4847abc09f9edce60cc746fcd44869447de252b8f974e182bf15c8f0`。操作结束后芯片保持暂停，未验证用户程序或整车运行，详见 [原工程验证记录](../../docs/validation/2026-10-11-original-firmware.md)。

用户确认进入重构，原工程审核问题不再作为整改任务或开始重构的前置条件。下一步在本目录建立最小运行基础，再逐项实现 A/B 车轮输出与编码器，随后迁移原 PID 和参数、速度环及转向环。

本次仅更新文档与忽略规则，没有修改本仓库固件源码或重新执行构建、烧录。下方保留恢复时的历史命令、产物及验证边界；历史 `cmake-build-verified` 不作为当前统一构建目录。

## 2026-09-17 历史恢复记录

验证日期：2026-09-17。

## 修复范围

从 Git 提交 `8e1bed2` 恢复缺失的 20 个业务文件。业务目录 `gc_rescue/` 与该提交一致，未改抓球流程、任务开关或视觉通信协议。原有外设配置修改保留。

本次修复在 `CMakeLists.txt` 和 `CMakeLists_template.txt` 的 `project()` 周围设置局部 `CMP0125 OLD` 兼容策略，随后立即恢复策略作用域。
本机的 CMake 3.28.1 和独立对照版本 3.31.6 在最小 C/C++/汇编 ARM 工程启用 `CMP0125 NEW` 时均出现进程退出码 `0xC0000409`；单独切换 CMP0125 后配置成功。此记录定位的是可复现的触发条件，并未分析 CMake 原生崩溃堆栈。
最终构建使用原有 STM32CubeCLT 工具链，不依赖另行下载的对照版本，也不需要更改全局环境。

## 构建方法

在本工程根目录的 PowerShell 中执行：

```powershell
cmake -S . -B cmake-build-verified -G Ninja -DCMAKE_BUILD_TYPE=Debug
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置失败' }
cmake --build cmake-build-verified --parallel 8
if ($LASTEXITCODE -ne 0) { throw '编译失败' }
```

需要 PATH 中存在 `cmake`、`ninja` 及 `arm-none-eabi-*`。本次使用 CMake 3.28.1、ARM GCC 13.3.1，来自 `D:/STM32CubeCLT_1.20.0`。

原来的 `cmake-build-debug/` 含有复制前 `D:/competiion/gc_new/gc_rescue` 的路径缓存，已保留；不要直接复用它。CLion 可将构建目录改为新的目录并重新加载 CMake。

## 验证结果

完整执行 53 个编译/链接步骤，退出码为 0，并生成 ELF、HEX、BIN。

- FLASH：40,664 / 131,072 字节（31.02%）。
- RAM_D1：25,416 / 524,288 字节（4.85%，链接脚本统计，含预留堆栈）。
- 保留 6 条既有 `-Wdiscarded-qualifiers` 警告，来自 MaixCAM、陀螺仪 DMA 缓冲区传给 HAL/memset 时丢弃 `volatile` 限定；未通过关闭警告或强制转换掩盖。
- 未烧录、未验证实车。旧控制端与当前 `视觉Maxicam/main.py` 的协议差异仍待后续单独修复。

产物位于 `cmake-build-verified/`，下列校验值仅对应本次构建：

| 文件 | 字节数 | SHA256 |
| --- | ---: | --- |
| rescue.elf | 654044 | `1c8d2e705ae33a3526aaf113d1702ba634888ea6b3168f6c79562d7452788439` |
| rescue.hex | 114435 | `5527822cf018c0f4cbc3ba752526b10ab5dc05e81d55a554e08f4be29a7bae26` |
| rescue.bin | 40664 | `ca3a763e4847abc09f9edce60cc746fcd44869447de252b8f974e182bf15c8f0` |
