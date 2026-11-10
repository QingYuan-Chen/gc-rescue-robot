"""使用串口替身运行完整查询工具，不枚举或连接真实串口。"""
import importlib.util
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("check_board", ROOT / "tools/check_board.py")
board = importlib.util.module_from_spec(spec)
spec.loader.exec_module(board)


class FakePort:
    def __init__(self, motor):
        self.sent = []
        self.lines = []
        self.motor = motor

    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass

    def reset_input_buffer(self):
        pass

    def write(self, data):
        self.sent.append(data)
        if data == b"ping\r\n":
            self.lines = [b"OK pong\r\n"]
        elif data == b"status\r\n":
            self.lines = [
                b"OK status period_ticks=10 min=10 max=10 delay_errors=0 rx_errors=0 tx_errors=0 rx_overflow=0\r\n",
                b"OK safety profile=tb67h450-basic left=M2 right=M1 outputs_low=1 motor_control=1 encoders=1\r\n",
                self.motor.encode("ascii") + b"\r\n",
            ]
        else:
            raise AssertionError("只允许 ping/status")

    def readline(self):
        return self.lines.pop(0)


class CheckBoardTest(unittest.TestCase):
    def test_idle_and_rejections(self):
        idle = ("OK motor ready=1 fault=0 left=M2 right=M1 request_left=0 request_right=0 "
                "applied_left=0 applied_right=0 phase_left=0 phase_right=0 remaining_ms=0 "
                "delta_left=4 total_left=3000000000")
        for response in (idle, idle.replace("fault=0", "fault=1"),
                         idle.replace("request_left=0", "request_left=100")):
            with self.subTest(response=response), tempfile.TemporaryDirectory() as folder:
                port = FakePort(response)
                target = Path(folder) / "result.json"
                with patch.object(board.serial, "Serial", return_value=port), \
                     patch.object(board.list_ports, "comports", return_value=[SimpleNamespace(device="TEST", hwid="fake")]), \
                     patch("sys.argv", ["check_board", "--port", "TEST", "--output", str(target)]):
                    if response == idle:
                        board.main()
                    else:
                        with self.assertRaises(RuntimeError):
                            board.main()
                self.assertEqual(port.sent, [b"ping\r\n", b"status\r\n"])
                result = json.loads(target.read_text(encoding="utf-8"))
                self.assertEqual(result["passed"], response == idle)
                self.assertEqual(result["motor"]["total_left"], 3000000000)


if __name__ == "__main__":
    unittest.main()
