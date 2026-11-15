# MaixCAM Pro 按键连拍采集程序

2026-11-11 新增 `vision/maixcam/capture_burst.py`：短按板载 USER 键后，在 2 秒时间窗内连续拍摄 5 张照片，按组保存到设备目录。本记录只覆盖 PC 侧静态检查与桩模块仿真，**未在 MaixCAM Pro 上实测**。

## 程序行为

| 项目 | 当前值 |
| --- | --- |
| 触发 | USER 键短按，`key.Keys.KEY_OK`（MaixPy 输入子系统的 OK 键） |
| 张数 / 时间窗 | 5 张 / 2000 ms |
| 间隔 | 400 ms，第 5 张约在触发后 1.6 s 完成，留有时间余量 |
| 分辨率 / 帧率 | 640×480 / 30 fps，启动丢 30 帧 |
| 保存位置 | `/root/data/burst_capture/cap_YYYYmmdd_HHMMSS/`，含 `01.jpg`～`05.jpg` 和 `meta.txt` |
| 写入保护 | 每拍完一组调用 `os.sync()` 主动刷盘 |
| 重复触发 | 拍摄中再次按键忽略；拍摄完成后可再次触发 |
| 退出 | 长按 USER 键 3 秒（创建 `key.Key` 后 MaixPy 默认按键退出被取消） |

`meta.txt` 记录触发时间、时间窗、分辨率、成功率、每张相对触发的实际毫秒数与本组总耗时。

## 已执行的检查

- `python -m py_compile vision/maixcam/capture_burst.py` 通过（Python 3.13.9）。
- PC 桩模块仿真（`build/maixcam-sim/simulate_capture.py`，`maix` 模块用假实现，时钟按每轮 33 ms 递增）：
  - 模拟一次短按后生成 1 个采集目录、按序保存 5 张 01～05.jpg；
  - `meta.txt` 中 5 张时刻递增，最后一张 1650 ms，未超过 2000 ms 时间窗；
  - 主循环按预期结束，退出摘要正确。

## 待上板确认

1. MaixCAM Pro 的 USER 键输入事件 id 是否为 `KEY_OK`；若日志打印其他 id，把该值加入脚本顶部 `TRIGGER_KEYS`。
2. 按键回调、拍摄间隔与 `img.save()` 实际耗时；当前桩仿真未包含真实存图开销。
3. `/root/data` 的写入权限与剩余空间；若改用其他目录，修改 `SAVE_ROOT`。
4. 屏幕预览文字为英文（MaixPy 默认字体不含中文字形），实际显示效果与触摸无关。
5. 不同分辨率（如 1280×720）下的内存占用和连拍稳定性。

按[工程规范](../工程规范.md)，本程序尚未上板验收，不能视为稳定的数据集采集入口；实测后再补充固件/脚本标识与现场结果。
