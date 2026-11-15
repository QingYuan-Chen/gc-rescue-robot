#!/usr/bin/env python3
"""MaixCAM Pro 按键连拍采集程序。

功能：按下板载 USER 键后，在 2 秒时间窗内连续拍摄 5 张照片并保存到设备目录；
      每次触发生成一个独立子目录，便于后续整理为数据集。

运行方式：
  - 在 MaixCAM Pro 终端执行：python capture_burst.py
  - 或在 MaixVision 中打开本文件直接运行。

按键说明：
  - 短按 USER 键：触发一组连拍；拍摄过程中再次短按会被忽略。
  - 长按 USER 键 3 秒：退出程序。创建 key.Key 对象后，MaixPy 默认的
    “按 USER 键退出” 行为会被取消，因此需要自行提供退出方式。
"""

import os
import traceback

from maix import app, camera, display, image, key, time

# ---------------- 采集参数（按需要调整） ----------------
SAVE_ROOT = "/root/data/burst_capture"  # 采集根目录，每次触发新建一个子目录
BURST_COUNT = 5  # 每次触发拍摄张数
BURST_WINDOW_MS = 2000  # 要求的时间窗：5 张照片须在触发后 2 秒内拍完
SHOT_INTERVAL_MS = 400  # 相邻两张的间隔，400 ms 时第 5 张在 1.6 s 完成，留有余量
FRAME_WIDTH = 640  # 采集分辨率，MaixCAM Pro 支持更高分辨率，但会占用更多内存与时间
FRAME_HEIGHT = 480
FRAME_FPS = 30
SKIP_FRAMES = 30  # 上电初期图像未稳定的帧数，先丢弃
EXIT_LONG_PRESS_MS = 3000  # 长按多久退出，避免按键停留过久误退出

# USER 键的输入事件 id。MaixCAM Pro 的 gpio-keys 节点把 "User Key" 映射为
# linux,code = 431（KEY_DISPLAYTOGGLE），不在 MaixPy 的 Keys 枚举内；
# KEY_OK 保留给其他把 USER 键映射为 OK 的板子。
TRIGGER_KEYS = (key.Keys.KEY_OK, 431)

TOTAL_SPAN_MS = SHOT_INTERVAL_MS * (BURST_COUNT - 1)


class BurstCapture:
    """按键触发的连拍采集器：主循环负责读图、计时、保存和屏幕提示。"""

    def __init__(self):
        self._cam = camera.Camera(FRAME_WIDTH, FRAME_HEIGHT, fps=FRAME_FPS)
        self._cam.skip_frames(SKIP_FRAMES)
        self._disp = display.Display()

        # 按键回调在独立线程执行，这里只置标志，拍摄动作全部交回主循环。
        self._key = key.Key(self._on_key, long_press_time=EXIT_LONG_PRESS_MS)

        self._trigger_pending = False
        self._exit_requested = False
        self._busy = False
        self._session_count = 0
        self._folder = ""
        self._burst_start_ms = 0
        self._shot_index = 0
        self._saved_count = 0
        self._failed_count = 0
        self._last_shot_elapsed_ms = -1
        self._burst_meta = []

    # ---------------- 按键回调 ----------------
    def _on_key(self, key_id, state):
        if state == key.State.KEY_PRESSED:
            if key_id in TRIGGER_KEYS:
                self._trigger_pending = True
            else:
                print(
                    f"[按键] 收到未配置的按键 id={key_id}，"
                    f"当前触发键为 {TRIGGER_KEYS[0]}；如为该键请修改 TRIGGER_KEYS"
                )
        elif state == key.State.KEY_LONG_PRESSED and key_id in TRIGGER_KEYS:
            print(f"[按键] 长按 {EXIT_LONG_PRESS_MS} ms，当前一组拍完后退出")
            self._exit_requested = True

    # ---------------- 主循环 ----------------
    def run(self):
        os.makedirs(SAVE_ROOT, exist_ok=True)
        print(f"[就绪] 短按 USER 键拍摄 {BURST_COUNT} 张，时间窗 {BURST_WINDOW_MS} ms，"
              f"保存到 {SAVE_ROOT}；长按 {EXIT_LONG_PRESS_MS // 1000} 秒退出")
        if TOTAL_SPAN_MS > BURST_WINDOW_MS:
            print(f"[警告] 计划跨度 {TOTAL_SPAN_MS} ms 超过时间窗 {BURST_WINDOW_MS} ms，"
                  f"请调小 SHOT_INTERVAL_MS")
        try:
            while not app.need_exit():
                if self._exit_requested and not self._busy:
                    break
                img = self._cam.read()
                now_ms = time.ticks_ms()
                if self._trigger_pending:
                    self._trigger_pending = False
                    if self._busy:
                        print("[采集] 上一组仍在拍摄，忽略本次按键")
                    else:
                        self._start_burst(now_ms)
                if self._busy:
                    self._capture_due(img, now_ms)
                self._show(img)
        finally:
            self._key.close()
            self._print_summary()

    def _start_burst(self, now_ms):
        self._folder = self._new_folder()
        self._session_count += 1
        self._burst_start_ms = now_ms
        self._shot_index = 0
        self._saved_count = 0
        self._failed_count = 0
        self._last_shot_elapsed_ms = -1
        self._burst_meta = []
        self._busy = True
        print(f"[采集] 第 {self._session_count} 组开始：目标 {BURST_COUNT} 张，"
              f"间隔 {SHOT_INTERVAL_MS} ms，目录 {self._folder}")

    def _new_folder(self):
        # 目录名取触发时刻；没有 RTC 电池时网络校时可能跳变，用序号后缀保证唯一。
        stamp = time.localtime().strftime("cap_%Y%m%d_%H%M%S")
        name = stamp
        suffix = 1
        while os.path.exists(os.path.join(SAVE_ROOT, name)):
            suffix += 1
            name = f"{stamp}_{suffix}"
        path = os.path.join(SAVE_ROOT, name)
        os.makedirs(path)
        return path

    def _capture_due(self, img, now_ms):
        # 每轮主循环最多保存一张，避免落后进度时把同一帧重复存成多张。
        if self._shot_index >= BURST_COUNT:
            self._finish_burst(now_ms)
            return
        due_ms = self._burst_start_ms + self._shot_index * SHOT_INTERVAL_MS
        if now_ms < due_ms:
            return
        self._save_shot(img, now_ms)
        if self._shot_index >= BURST_COUNT:
            self._finish_burst(now_ms)

    def _save_shot(self, img, now_ms):
        number = self._shot_index + 1
        file_path = os.path.join(self._folder, f"{number:02d}.jpg")
        elapsed_ms = now_ms - self._burst_start_ms
        try:
            img.save(file_path)
            self._saved_count += 1
            self._last_shot_elapsed_ms = elapsed_ms
            self._burst_meta.append((number, file_path, elapsed_ms))
            print(f"[采集] {number}/{BURST_COUNT} 已保存 {file_path}（触发后 {elapsed_ms} ms）")
        except Exception as exc:  # 存储空间不足、写保护等情况下不中断剩余拍摄
            self._failed_count += 1
            print(f"[采集] {number}/{BURST_COUNT} 保存失败：{exc}")
        self._shot_index += 1

    def _finish_burst(self, now_ms):
        self._busy = False
        last_ms = self._last_shot_elapsed_ms
        in_window = last_ms >= 0 and last_ms <= BURST_WINDOW_MS
        if self._saved_count == BURST_COUNT and in_window:
            print(f"[采集] 第 {self._session_count} 组完成：{BURST_COUNT} 张全部保存，"
                  f"最后一张在触发后 {last_ms} ms，满足 {BURST_WINDOW_MS} ms 时间窗")
        else:
            print(f"[采集] 第 {self._session_count} 组异常：成功 {self._saved_count}/{BURST_COUNT}，"
                  f"最后一张在 {last_ms} ms，未满足 {BURST_WINDOW_MS} ms 时间窗")
        self._write_meta(now_ms)
        try:
            os.sync()  # 采集场景主动刷新到 TF 卡，降低断电丢图风险
        except OSError as exc:
            print(f"[采集] 刷盘失败：{exc}")

    def _write_meta(self, now_ms):
        meta_path = os.path.join(self._folder, "meta.txt")
        try:
            with open(meta_path, "w", encoding="utf-8") as file:
                file.write(f"触发时间: {time.localtime().strftime('%Y-%m-%d %H:%M:%S')}\n")
                file.write(f"时间窗: {BURST_WINDOW_MS} ms，目标 {BURST_COUNT} 张，"
                           f"间隔 {SHOT_INTERVAL_MS} ms\n")
                file.write(f"分辨率: {FRAME_WIDTH}x{FRAME_HEIGHT}\n")
                file.write(f"成功: {self._saved_count}/{BURST_COUNT}，失败: {self._failed_count}\n")
                for number, path, elapsed_ms in self._burst_meta:
                    file.write(f"第 {number} 张: {os.path.basename(path)}，触发后 {elapsed_ms} ms\n")
                file.write(f"本组总耗时: {now_ms - self._burst_start_ms} ms\n")
        except Exception as exc:
            print(f"[采集] 写入 meta.txt 失败：{exc}")

    # ---------------- 屏幕提示 ----------------
    def _show(self, img):
        if self._busy:
            elapsed_ms = time.ticks_ms() - self._burst_start_ms
            label = f"CAPTURE {self._shot_index}/{BURST_COUNT}  {elapsed_ms} ms"
            color = image.Color.from_rgb(255, 200, 0)
            img.draw_rect(0, 0, img.width(), img.height(),
                          image.Color.from_rgb(255, 0, 0), thickness=2)
        else:
            label = (f"USER: {BURST_COUNT} shots / {BURST_WINDOW_MS} ms    "
                     f"hold {EXIT_LONG_PRESS_MS // 1000}s = exit")
            color = image.Color.from_rgb(0, 255, 0)
        img.draw_string(4, 4, label, color, scale=1)
        if not self._busy and self._last_shot_elapsed_ms >= 0:
            status = f"session {self._session_count}  last shot {self._last_shot_elapsed_ms} ms"
            img.draw_string(4, 20, status, color, scale=1)
        self._disp.show(img)

    def _print_summary(self):
        if self._session_count:
            print(f"[退出] 共触发 {self._session_count} 组，保存目录 {SAVE_ROOT}")
        else:
            print("[退出] 本次没有触发采集")


def main():
    try:
        BurstCapture().run()
    except KeyboardInterrupt:
        print("[退出] 收到中断，程序结束")
    except Exception:
        traceback.print_exc()


if __name__ == "__main__":
    main()
