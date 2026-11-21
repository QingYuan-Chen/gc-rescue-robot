"""只读查询 TB67H450 基础驱动的链路、周期、编码器和停车状态。"""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path

import serial
from serial.tools import list_ports


def query(port, command, prefix):
    port.write(command.encode("ascii") + b"\r\n")
    # status 依次返回基础运行、安全 GPIO、车轮输入输出三个快照。
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
            result[key] = value
    return result


def check_idle(runtime, safety, motor):
    if runtime["period_ticks"] != 10 or runtime["min"] != 10 or runtime["max"] != 10:
        raise RuntimeError("10 ms 周期检查未通过")
    if any(runtime[key] for key in ("delay_errors", "rx_errors", "tx_errors", "rx_overflow")):
        raise RuntimeError("存在历史调度或串口错误，请查看输出记录")
    if (safety["profile"], safety["left"], safety["right"]) != ("tb67h450-basic", "M2", "M1"):
        raise RuntimeError("板上固件或端口定义不是当前基础驱动版")
    if safety["outputs_low"] != 1 or safety["motor_control"] != 1 or safety["encoders"] != 1:
        raise RuntimeError("输出未处于 GPIO 双低停车状态或固件能力不匹配")
    if motor["ready"] != 1 or motor["fault"] != 0:
        raise RuntimeError("车轮模块未就绪或故障已锁定")
    if (motor["left"], motor["right"]) != ("M2", "M1"):
        raise RuntimeError("车轮模块端口定义不匹配")
    if any(motor[key] for key in ("request_left", "request_right", "applied_left",
                                 "applied_right", "phase_left", "phase_right", "remaining_ms")):
        raise RuntimeError("存在活动电机命令；本工具只查询，不改变命令或输出")


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
            safety_line = port.readline().decode("ascii").strip()
            if not safety_line.startswith("OK safety "):
                raise RuntimeError(f"没有收到输出状态：{safety_line!r}")
            result["safety"] = fields(safety_line)
            motor_line = port.readline().decode("ascii").strip()
            if not motor_line.startswith("OK motor "):
                raise RuntimeError(f"没有收到车轮状态：{motor_line!r}")
            result["motor"] = fields(motor_line)
            check_idle(result["runtime"], result["safety"], result["motor"])
            result["passed"] = True
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"通过：{args.port}，周期 10 ms、输出锁存为低且为 GPIO 输出模式；未执行运动。记录：{args.output}")


if __name__ == "__main__":
    main()
