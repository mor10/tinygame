import base64
import contextlib
import importlib.util
import io
from pathlib import Path
import struct
import tempfile
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("tg", ROOT / "tools/tg.py")
tg = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tg)


class Port:
    def __init__(self):
        self.closed = False
        self.writes = []

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.closed = True

    def write(self, data):
        self.writes.append(data)


class ToolTests(unittest.TestCase):
    def test_send_errors_and_timeout_close_port(self):
        for lines in ([], ["[DEV] error: invalid"], ["[DEV] ok-ish"]):
            with self.subTest(lines=lines):
                port = Port()
                with patch.object(tg, "open_serial", return_value=port), \
                     patch.object(tg, "read_lines", return_value=iter(lines)), \
                     contextlib.redirect_stdout(io.StringIO()):
                    with self.assertRaises(RuntimeError):
                        tg.cmd_send(SimpleNamespace(port=None, commands=["click"]))
                self.assertTrue(port.closed)

    def test_send_success(self):
        port = Port()
        with patch.object(tg, "open_serial", return_value=port), \
             patch.object(tg, "read_lines", return_value=iter(["[DEV] ok"])), \
             patch.object(tg.time, "sleep"), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(tg.cmd_send(SimpleNamespace(port=None, commands=["click"])), 0)
        self.assertEqual(port.writes, [b"click\n"])
        self.assertTrue(port.closed)

    def test_durations(self):
        for value in ("nan", "inf", "-1", "1 2"):
            with self.assertRaises(ValueError):
                tg.nonnegative_seconds(value)
        self.assertEqual(tg.nonnegative_seconds("0"), 0)

    def test_port_selection(self):
        devices = [SimpleNamespace(device="one", vid=tg.ESPRESSIF_USB_VID)]
        serial = ModuleType("serial")
        serial.tools = ModuleType("serial.tools")
        serial.tools.list_ports = SimpleNamespace(comports=lambda: devices)
        with patch.dict("sys.modules", {"serial": serial, "serial.tools": serial.tools}):
            self.assertEqual(tg.find_port(), "one")
            devices.append(SimpleNamespace(device="two", vid=tg.ESPRESSIF_USB_VID))
            with self.assertRaises(RuntimeError):
                tg.find_port()
            self.assertEqual(tg.find_port("two"), "two")
            devices.clear()
            with self.assertRaises(RuntimeError):
                tg.find_port()

    def test_screenshot_validation(self):
        pixels = bytes(320 * 172 * 2)
        row = base64.b64encode(bytes(640)).decode()
        valid = ["SHOT 320 172 RGB565LE", *([row] * 172), "END"]
        self.assertEqual(tg.read_screenshot(iter(valid)), pixels)
        for lines in (valid[:-1], ["SHOT 320 172 RGB565LE", row, "END"],
                      ["SHOT 1 1 RGB565LE"], ["SHOT 320 172 RGB565LE", "???"],
                      ["[DEV] error: out of memory"],
                      ["SHOT 320 172 RGB565LE", *([row] * 173), "END"]):
            with self.subTest(lines=lines[:2]):
                with self.assertRaises((ValueError, RuntimeError)):
                    tg.read_screenshot(iter(lines))

    def test_png_pixels(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "colors.png"
            tg.write_png(path, 2, 1, struct.pack("<HH", 0xf800, 0x07e0))
            data = path.read_bytes()
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        offset = 8
        while offset < len(data):
            length = struct.unpack_from(">I", data, offset)[0]
            kind = data[offset + 4:offset + 8]
            payload = data[offset + 8:offset + 8 + length]
            if kind == b"IDAT":
                self.assertEqual(zlib.decompress(payload), b"\0\xff\0\0\0\xff\0")
            offset += length + 12

    def test_scaffold_rejects_bad_inputs(self):
        for game_id, name in (("class", "NAME"), ("std", "NAME"), ("a__b", "NAME"),
                              ("1bad", "NAME"), ("a" * 16, "NAME"), ("ok", ""),
                              ("ok", " "), ("ok", "A\nB"), ("ok", "é"), ("ok", "ß"),
                              ("ok", "A" * 14)):
            with self.subTest(game_id=game_id, name=name), tempfile.TemporaryDirectory() as root:
                with patch.object(tg, "ROOT", root), self.assertRaises(ValueError):
                    tg.cmd_new(SimpleNamespace(id=game_id, name=name))
                self.assertFalse((Path(root) / "src").exists())

    def test_scaffold_escapes_once(self):
        with tempfile.TemporaryDirectory() as root, patch.object(tg, "ROOT", root), \
             contextlib.redirect_stdout(io.StringIO()):
            tg.cmd_new(SimpleNamespace(id="demo_2", name='SAY "HI"\\'))
            source = (Path(root) / "src/games/demo_2/game.cpp").read_text()
            self.assertIn('"SAY \\"HI\\"\\\\"', source)
            self.assertIn("namespace demo_2", source)
            with self.assertRaises(ValueError):
                tg.cmd_new(SimpleNamespace(id="demo_2", name="AGAIN"))
            tg.cmd_new(SimpleNamespace(id="placeholder", name="__CLASS__"))
            source = (Path(root) / "src/games/placeholder/game.cpp").read_text()
            self.assertIn('"__CLASS__"', source)

    def test_nested_header_discovery(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root)
            (path / "nested").mkdir()
            (path / "game.cpp").write_text('#include "nested/view.h"\n')
            (path / "nested/view.h").write_text('#include "board.h"\n')
            (path / "nested/board.h").write_text('#include <Arduino.h>\n')
            self.assertEqual(tg.host_safe_sources(root), [])

    def test_skill_aliases(self):
        for name in ("make-a-game", "waveshare-esp32-s3-lcd-147b"):
            alias = ROOT / ".agents/skills" / name
            self.assertTrue(alias.is_symlink())
            self.assertEqual(alias.resolve(), ROOT / ".claude/skills" / name)
            self.assertTrue((alias / "SKILL.md").is_file())


if __name__ == "__main__":
    unittest.main()
