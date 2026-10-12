"""测试 UART5 调试命令和参数读写，只操作状态上报参数，不驱动执行器。"""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import time

import serial
from serial.tools import list_ports


class DebugLink:
    def __init__(self, port, transcript):
        self.port = port
        self.transcript = transcript

    def send(self, data):
        self.transcript.append({"tx": data.decode("ascii", errors="backslashreplace")})
        self.port.write(data)
        self.port.flush()

    def receive(self, prefix, timeout=3.0):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            data = self.port.readline()
            if not data:
                continue
            line = data.decode("ascii").strip()
            self.transcript.append({"rx": line})
            if line.startswith(prefix):
                return line
            if line.startswith("ERR "):
                raise AssertionError(f"收到意外错误：{line}")
        raise AssertionError(f"等待 {prefix!r} 超时")

    def command(self, command, prefix):
        self.send(command.encode("ascii") + b"\r\n")
        return self.receive(prefix)

    def status(self):
        line = self.command("status", "OK status ")
        return {key: int(value) for key, value in
                (field.split("=", 1) for field in line.split()[2:])}


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run_checks(link, rounds, report):
    link.command("set report_ms 0", "OK report_ms=0")
    initial = link.status()
    report["initial_status"] = initial
    require(initial["period_ticks"] == 10, "基础周期不是 10 ms")
    require(initial["tick_hz"] == 1000, "RTOS 节拍不是 1 kHz")
    link.command("help", "OK help:")

    for ending in (b"\r", b"\n", b"\r\n"):
        link.send(b"ping" + ending)
        link.receive("OK pong")
    link.send(b"pi")
    time.sleep(0.05)
    link.send(b"ng\r\n")
    link.receive("OK pong")
    link.send(b"pinx\bg\n")
    link.receive("OK pong")
    report["line_endings_and_fragmentation"] = "passed"

    link.command("set report_ms 60000", "OK report_ms=60000")
    link.command("get report_ms", "OK report_ms=60000")
    for value in ("-1", "99", "60001", "100x", "999999999999999", "100 200"):
        link.command("set report_ms " + value, "ERR ")
        link.command("get report_ms", "OK report_ms=60000")
    link.command("set unknown 1", "ERR unknown")
    link.send(b"x" * 120 + b"\r\n")
    link.receive("ERR line discarded")
    link.send(b"set report_ms 1000\0garbage\n")
    link.receive("ERR line discarded")
    link.command("get report_ms", "OK report_ms=60000")
    link.command("ping", "OK pong")
    report["parameter_validation_and_recovery"] = "passed"

    link.command("set report_ms 100", "OK report_ms=100")
    reports = [link.receive("OK status ") for _ in range(6)]
    report["periodic_report_lines"] = reports
    link.command("set report_ms 0", "OK report_ms=0")
    link.command("get report_ms", "OK report_ms=0")

    start = time.monotonic()
    for _ in range(rounds):
        link.command("ping", "OK pong")
    report["ping_rounds"] = rounds
    report["ping_elapsed_s"] = round(time.monotonic() - start, 3)
    final = link.status()
    report["final_status"] = final
    require(final["samples"] > initial["samples"], "周期任务计数没有增长")
    require(final["min"] == final["max"] == 10, "调试负载下 10 ms 周期出现抖动")
    for field in ("delay_errors", "rx_overflow", "rx_errors", "tx_errors"):
        require(final[field] == 0, f"{field} 不为零")
    require(final["robot_stack_free"] >= 128, "基础任务剩余栈不足 128 字节")
    require(final["debug_stack_free"] >= 256, "调试任务剩余栈不足 256 字节")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="明确指定 DAPLink 的串口，如 COM5")
    parser.add_argument("--rounds", type=int, default=100, help="连续 ping 的次数")
    parser.add_argument("--output", type=Path, default=Path("build/uart5-validation/serial.json"))
    args = parser.parse_args()
    if args.rounds < 1:
        parser.error("rounds 必须大于零")
    ports = {port.device: port for port in list_ports.comports()}
    if args.port not in ports:
        parser.error(f"找不到 {args.port}，当前串口：{', '.join(ports)}")

    report = {
        "time_utc": datetime.now(timezone.utc).isoformat(),
        "port": args.port,
        "hwid": ports[args.port].hwid,
        "baudrate": 115200,
        "passed": False,
        "transcript": [],
    }
    try:
        with serial.Serial(args.port, 115200, timeout=0.5, write_timeout=2) as port:
            port.reset_input_buffer()
            link = DebugLink(port, report["transcript"])
            try:
                run_checks(link, args.rounds, report)
                report["passed"] = True
            finally:
                # 验收结束后关闭周期上报，保持串口安静。
                link.command("set report_ms 0", "OK report_ms=0")
    except Exception as error:
        report["passed"] = False
        report["error"] = str(error)
        raise
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"通过：{args.port}，连续 {args.rounds} 次 ping；记录：{args.output}")
    print(json.dumps(report["final_status"], ensure_ascii=False))


if __name__ == "__main__":
    main()
