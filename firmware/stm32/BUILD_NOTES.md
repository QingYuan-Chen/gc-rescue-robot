# 工程恢复与构建记录

## 2026-10-12 注释统一与规范核查

详细范围、未迁移的结构差异及验证证据见[核查记录](../../docs/validation/2026-10-12-comment-and-conventions-audit.md)。本次统一项目自有注释、纠正说明与实现不一致之处并移除废弃注释代码，保留旧文件编码、有效代码与构建参数。

`stm32-debug`、`stm32-release` 均重新配置成功，各完成 11/11 步增量构建。两种 BIN 的 SHA256 与下方此前构建记录一致；各出现 3 条原有 MaixCAM `volatile` 警告，未重编译的 IMU 文件不计入本次增量警告数量。未执行烧录或设备运行验证。

## 2026-10-12 协作规范更新

当前约定见 [工程规范 V1.1](../../docs/工程规范.md)：项目自有注释统一使用中文，模块保持高内聚、低耦合与单一职责，方案替换时同步清理旧实现和相关配置。开发增量及时提交到 `develop`，通过相应阶段验收的稳定版本再合入 `main`。

本次更新规范、配置说明和注释，不改变 CMake 预设的构建参数，也不修改固件源码。下方保留此前构建的实际记录。

## 2026-10-11 工程规范与统一预设

开发约定见 [工程规范](../../docs/工程规范.md)。本次新增 `CMakePresets.json`，使用当前有效 CMake 构建入口和原编译选项，未改固件源码、控制算法或参数。命令行与 CLion 统一使用 `stm32-debug`，Release 按需使用 `stm32-release`。

在 `firmware/stm32/` 执行 `cmake --preset <预设名>` 和 `cmake --build --preset <预设名> --parallel 8`。默认产物统一到仓库根目录的 `build/stm32-debug/` 或 `build/stm32-release/`，本机绝对工具路径不进入共享预设。

两种预设均已用 CMake 3.28.1、Ninja、ARM GCC 13.3.1 完成 53/53 步构建，各保留 6 条既有 `volatile` 警告：

| 预设 | FLASH 字节 | RAM_D1 字节 | BIN SHA256 |
| --- | ---: | ---: | --- |
| `stm32-debug` | 40664 | 25416 | `ca3a763e4847abc09f9edce60cc746fcd44869447de252b8f974e182bf15c8f0` |
| `stm32-release` | 41936 | 25456 | `94b5eb0bd729cf45043314a50066d9482156334c5041515d4373047f697bd6c4` |

Debug BIN 与已烧录的原工程固件一致。此次仅验证配置、构建与开发规则，没有再次烧录或启动程序；Release 的编译成功不代表其硬件运行已验收。日志保留在各构建目录的 `configure-conventions.log` 和 `build-conventions.log`。

格式配置通过 CLion 自带 clang-format 22 解析与样例检查；旧业务、CubeMX 和第三方源码的排除规则已验证，没有对它们进行格式化或转码。已有 CLion 窗口仍需加载并启用新预设，项目文件本身不自动修改本机的活动配置。

## 2026-10-11 当前重构入口

本仓库的 `firmware/stm32/` 为后续逐层重构目录，构建方法与目录说明以 [仓库 README](../../README.md) 为准。日常开发使用 `develop`，稳定版本合入 `main`，远端为 `QingYuan-Chen/gc-rescue-robot`。

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
