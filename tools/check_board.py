"""复测无线链路、控制周期和停车状态；不发送运动命令。"""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path

import serial
from serial.tools import list_ports


def query(port, command, prefix):
    port.write(command.encode("ascii") + b"\r\n")
    # status 会依次返回基础状态和车轮状态，按前缀读取目标行。
    for _ in range(10):
        line = port.readline().decode("ascii").strip()
        if line.startswith(prefix):
            return line
        if line.startswith("ERR ") or not line:
            break
    raise RuntimeError(f"{command} 没有收到预期回复，最后响应：{line!r}")


def fields(line):
    result = {}
    for item in line.split()[2:]:
        key, value = item.split("=", 1)
        try:
            result[key] = int(value)
        except ValueError:
            try:
                result[key] = float(value)
            except ValueError:
                result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="当前 DAPLink 串口，例如 COM5")
    parser.add_argument("--output", type=Path, default=Path("build/board-check.json"))
    args = parser.parse_args()
    ports = {port.device: port for port in list_ports.comports()}
    if args.port not in ports:
        parser.error(f"找不到 {args.port}，当前串口：{', '.join(ports)}")
    result = {
        "time_utc": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "hwid": ports[args.port].hwid,
        "passed": False,
    }
    try:
        with serial.Serial(args.port, 115200, timeout=2, write_timeout=2) as port:
            port.reset_input_buffer()
            result["ping"] = query(port, "ping", "OK pong")
            result["runtime"] = fields(query(port, "status", "OK status "))
            # 先读取 status 附带的车轮快照，避免遗留响应与下一条查询混淆。
            wheel_line = port.readline().decode("ascii").strip()
            if not wheel_line.startswith("OK wheel "):
                raise RuntimeError(f"没有收到车轮状态：{wheel_line!r}")
            result["wheels"] = fields(wheel_line)
            runtime, wheels = result["runtime"], result["wheels"]
            if runtime["period_ticks"] != 10 or runtime["min"] != 10 or runtime["max"] != 10:
                raise RuntimeError("10 ms 周期检查未通过")
            if any(runtime[key] for key in ("delay_errors", "rx_errors", "tx_errors", "rx_overflow")):
                raise RuntimeError("存在历史调度或串口错误，请查看输出记录")
            if wheels["mode"] != "stop" or wheels["ready"] != 1 or wheels["fault"] != 0:
                raise RuntimeError("车轮未处于正常停车状态，请查看模式与故障原因")
            if any(wheels[key] != 0 for key in (
                "duty_left", "duty_right", "request_left", "request_right", "remaining_ms",
                "enable_left", "enable_right", "pid_left", "pid_right", "transition",
            )):
                raise RuntimeError("停车输出或控制状态未清理完成")
            result["passed"] = True
    except Exception as error:
        result["error"] = str(error)
        raise
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"通过：{args.port}，链路正常、周期 10 ms、已执行停车且无故障。记录：{args.output}")


if __name__ == "__main__":
    main()
