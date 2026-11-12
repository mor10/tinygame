import contextlib
import importlib.util
import io
import json
import signal
from pathlib import Path
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("tg_bridge", ROOT / "tools/tg.py")
tg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tg)


class Port:
    def __init__(self, chunks):
        self.chunks = iter(chunks)
        self.writes = []
        self.closed = False

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.closed = True

    def write(self, data):
        self.writes.append(data)

    def read(self, _size):
        try:
            chunk = next(self.chunks)
        except StopIteration:
            raise KeyboardInterrupt
        if isinstance(chunk, Exception):
            raise chunk
        return chunk


class BridgeTests(unittest.TestCase):
    def test_parser(self):
        self.assertIsNone(tg.parse_input_line("[DEV] ok"))
        self.assertIsNone(tg.parse_input_line(""))
        sample = tg.parse_input_line("IN -2147483648 1 2147483647 0 -0.5 0.25")
        self.assertEqual(sample, {"leftTotal": -2147483648, "leftDown": True,
                                 "rightTotal": 2147483647, "rightDown": False,
                                 "tiltX": -0.5, "tiltY": 0.25})
        for line in ("IN", "IN 1 0 2 1 0 0 extra", "IN 1.5 0 2 1 0 0",
                     "IN 2147483648 0 2 1 0 0", "IN 1 2 2 1 0 0",
                     "IN 1 0 2 true 0 0", "IN 1 0 2 1 nan 0",
                     "IN 1 0 2 1 0 inf", "IN 1 0 2 1 x 0",
                     "IN 1 0 2 1 1.01 0", "IN 1 0 2 1 0 -1.01"):
            with self.subTest(line=line), self.assertRaises(ValueError):
                tg.parse_input_line(line)

    def test_bridge_fragments_and_cleanup(self):
        port = Port([b"[DEV] ok\nIN -4 1 ", b"8 0 -0.25 0.5\r\n"])
        output = io.StringIO()
        with patch.object(tg, "open_serial", return_value=port), \
             contextlib.redirect_stdout(output):
            self.assertEqual(tg.cmd_bridge(SimpleNamespace(port="USB")), 0)
        self.assertEqual(len(output.getvalue().splitlines()), 1)
        self.assertEqual(json.loads(output.getvalue())["leftTotal"], -4)
        self.assertEqual(port.writes, [b"stream on\n", b"stream off\n"])
        self.assertTrue(port.closed)

    def test_bridge_failures_cleanup(self):
        for chunk, error in ((b"IN 0 2 0 0 0 0\n", ValueError),
                             (b"[DEV] error: unknown command\n", RuntimeError),
                             (b"x" * 16385, RuntimeError),
                             (OSError("disconnected"), OSError)):
            port = Port([chunk])
            with self.subTest(chunk=str(chunk)[:50]), \
                 patch.object(tg, "open_serial", return_value=port), \
                 self.assertRaises(error):
                tg.cmd_bridge(SimpleNamespace(port=None))
            self.assertEqual(port.writes[-1], b"stream off\n")
            self.assertTrue(port.closed)

    def test_bridge_heartbeat_and_timeout(self):
        port = Port([b""])
        with patch.object(tg, "open_serial", return_value=port), \
             patch.object(tg.time, "monotonic", side_effect=[0, 1, 3]), \
             self.assertRaisesRegex(RuntimeError, "timed out"):
            tg.cmd_bridge(SimpleNamespace(port=None))
        self.assertEqual(port.writes, [b"stream on\n", b"stream ping\n", b"stream off\n"])

    def test_bridge_sigterm_cleanup(self):
        port = Port([])
        previous = signal.getsignal(signal.SIGTERM)

        def read(_size):
            signal.getsignal(signal.SIGTERM)(signal.SIGTERM, None)
            return b""

        port.read = read
        with patch.object(tg, "open_serial", return_value=port):
            self.assertEqual(tg.cmd_bridge(SimpleNamespace(port=None)), 0)
        self.assertEqual(port.writes, [b"stream on\n", b"stream off\n"])
        self.assertTrue(port.closed)
        self.assertEqual(signal.getsignal(signal.SIGTERM), previous)

    def test_sim_delegation(self):
        server = ModuleType("sim_server")
        args = SimpleNamespace(port=0, build_only=True, no_open=True)
        with patch.object(server, "cmd_sim", create=True, return_value=0) as call, \
             patch.dict("sys.modules", {"sim_server": server}):
            self.assertEqual(tg.cmd_sim(args), 0)
        call.assert_called_once_with(args)

    def test_sim_import_from_module(self):
        original_path = list(tg.sys.path)
        with patch.dict("sys.modules"), patch.object(tg.subprocess, "call", return_value=0):
            tg.sys.modules.pop("sim_server", None)
            self.assertEqual(tg.cmd_sim(SimpleNamespace(port=0, build_only=True,
                                                       no_open=True)), 0)
        self.assertEqual(tg.sys.path, original_path)

    def test_optional_node_tests(self):
        with patch.object(tg.shutil, "which", return_value="/usr/bin/node"), \
             patch.object(tg.subprocess, "call", return_value=1) as call:
            self.assertEqual(tg.test_simulator(), 1)
        call.assert_called_once_with(
            ["/usr/bin/node", "--test", "sim/web/input.test.mjs",
             "sim/web/display_ready.test.mjs", "sim/server.test.mjs"],
            cwd=str(ROOT))
        output = io.StringIO()
        with patch.object(tg.shutil, "which", return_value=None), \
             patch.object(tg.subprocess, "call") as call, contextlib.redirect_stdout(output):
            self.assertEqual(tg.test_simulator(), 0)
        call.assert_not_called()
        self.assertIn("SKIP simulator JavaScript tests", output.getvalue())


if __name__ == "__main__":
    unittest.main()
