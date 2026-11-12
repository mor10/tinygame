import base64
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import shutil
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
import lua_check


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
        return len(data)


class ToolTests(unittest.TestCase):
    def test_doctor_report_checks_prerequisites_without_inferring_board_identity(self):
        versions = {
            tg.sys.executable: "PlatformIO Core, version 6.1.18",
            "c++": "Apple clang version 17.0.0",
            "node": "v22.4.0",
            "git": "git version 2.45.0",
            "curl": "curl 8.7.1",
        }

        def fake_version(command):
            return versions[command[0]], None

        port = {"device": "/dev/ttyUSB0", "vid": tg.ESPRESSIF_USB_VID, "pid": 0x1001}
        with patch.object(tg.sys, "version_info", (3, 12, 1)), \
             patch.object(tg, "_doctor_version", side_effect=fake_version), \
             patch.object(tg.importlib.util, "find_spec", return_value=object()), \
             patch.object(tg, "_doctor_serial_ports", return_value=[port]):
            report = tg.collect_doctor_report()
        self.assertEqual(report["schemaVersion"], 1)
        self.assertEqual(report["profiles"], ["147b", "169"])
        self.assertFalse(report["boardIdentityInferred"])
        self.assertEqual(report["usbPorts"], [port])
        self.assertTrue(all(check["status"] in ("ok", "info")
                            for check in report["checks"]))

    def test_doctor_json_is_machine_readable_and_propagates_required_failures(self):
        report = {
            "schemaVersion": 1,
            "profiles": ["147b", "169"],
            "usbPorts": [],
            "usbPortsTruncated": False,
            "boardIdentityInferred": False,
            "checks": [{"name": "node", "status": "error",
                        "detail": "Node.js 22+ is required"}],
        }
        output = io.StringIO()
        with patch.object(tg, "collect_doctor_report", return_value=report), \
             contextlib.redirect_stdout(output):
            result = tg.cmd_doctor(SimpleNamespace(json=True))
        self.assertEqual(result, 1)
        self.assertEqual(json.loads(output.getvalue()), report)

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

    def test_test_card_build_uses_explicit_profile(self):
        with patch.object(tg, "pio", return_value=0) as pio, \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(tg.cmd_test_card_build(SimpleNamespace(board="169")), 0)
        pio.assert_called_once_with("run", "-e", "test-card-169")

    def test_test_card_flash_uses_explicit_profile_and_port(self):
        with patch.object(tg, "find_port", return_value="/dev/ttyUSB0"), \
             patch.object(tg, "pio", return_value=0) as pio, \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(tg.cmd_test_card_flash(
                SimpleNamespace(board="147b", port=None)), 0)
        pio.assert_called_once_with("run", "-e", "test-card-147b", "-t", "upload",
                                    "--upload-port", "/dev/ttyUSB0")

    def test_build_uses_explicit_board_profile(self):
        with patch.object(tg, "pio", return_value=0) as pio:
            self.assertEqual(tg.cmd_build(SimpleNamespace(board="169")), 0)
        pio.assert_called_once_with("run", "-e", "esp32-s3-lcd-169")

    def test_flash_uses_explicit_board_profile(self):
        with patch.object(tg, "find_port", return_value="/dev/ttyUSB0"), \
             patch.object(tg, "pio", return_value=0) as pio, \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(tg.cmd_flash(SimpleNamespace(
                board="169", port=None)), 0)
        pio.assert_called_once_with("run", "-e", "esp32-s3-lcd-169", "-t", "upload",
                                    "--upload-port", "/dev/ttyUSB0")

    def test_flash_cli_requires_board_before_accessing_a_port(self):
        error = io.StringIO()
        with patch("sys.argv", ["tg.py", "flash"]), \
             contextlib.redirect_stderr(error), self.assertRaises(SystemExit) as raised:
            tg.main()
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("--board", error.getvalue())

    def test_game_bundle_cli_checks_and_packs_within_repo(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            games = root / "games"
            games.mkdir()
            shutil.copytree(ROOT / "tests/fixtures/games/cave_hop", games / "cave_hop")
            with patch.object(tg, "ROOT", str(root)), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(tg.cmd_check_game(SimpleNamespace(id="cave_hop")), 0)
                self.assertEqual(tg.cmd_pack_game(SimpleNamespace(id="cave_hop")), 0)
                bundle = (root / "build/games/cave_hop.tgb").read_bytes()
                self.assertEqual(tg.game_format_module().parse_bundle(bundle)["game.toml"],
                                 (games / "cave_hop/game.toml").read_bytes())

    def test_check_compiles_lua_rules_and_modules_without_running_them(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            games = root / "games"
            games.mkdir()
            game = games / "knob_dodge"
            shutil.copytree(ROOT / "games/knob_dodge", game)
            sentinel = root / "executed"
            (game / "rules.lua").write_text(
                f'os.execute("touch {sentinel}")\nfunction update() end\n')
            modules = game / "modules"
            modules.mkdir(exist_ok=True)
            (modules / "helper.lua").write_text("return {value = 1}\n")
            with patch.object(tg, "ROOT", str(root)), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(tg.cmd_check_game(SimpleNamespace(id="knob_dodge")), 0)
                self.assertFalse(sentinel.exists())
                (modules / "helper.lua").write_text("local broken =\n")
                with self.assertRaisesRegex(RuntimeError, r"modules/helper\.lua"):
                    tg.cmd_check_game(SimpleNamespace(id="knob_dodge"))
                (modules / "helper.lua").write_text("return true\n")
                (game / "rules.lua").write_text("function update(\n")
                with self.assertRaisesRegex(RuntimeError, r"rules\.lua"):
                    tg.cmd_check_game(SimpleNamespace(id="knob_dodge"))
                with self.assertRaisesRegex(RuntimeError, r"rules\.lua"):
                    tg.cmd_pack_game(SimpleNamespace(id="knob_dodge"))
                with patch.object(tg, "open_serial") as serial:
                    with self.assertRaisesRegex(RuntimeError, r"rules\.lua"):
                        tg.cmd_push(SimpleNamespace(id="knob_dodge", port=None))
                    serial.assert_not_called()
            self.assertFalse(sentinel.exists())

    def test_lua_checker_rejects_binary_and_overlarge_sources(self):
        tg.check_lua_sources({"rules.lua": b"--" + b" " * ((64 << 10) - 2)})
        for content in (b"\x1bLua", b"--" + b" " * ((64 << 10) - 1)):
            with self.subTest(length=len(content)), self.assertRaises(RuntimeError):
                tg.check_lua_sources({"rules.lua": content})

    def test_lua_checker_shares_one_deadline_across_all_sources(self):
        sources = {"modules/a.lua": b"return 1", "rules.lua": b"function update() end"}
        with patch.object(lua_check, "_parser", return_value="/parser"), \
             patch.object(lua_check.time, "monotonic", side_effect=[100, 101, 121]), \
             patch.object(lua_check.subprocess, "run",
                          return_value=SimpleNamespace(returncode=0)) as run:
            with self.assertRaisesRegex(RuntimeError, "rules.lua: Lua syntax check timed out"):
                lua_check.check_sources(sources)
        run.assert_called_once()
        self.assertEqual(run.call_args.kwargs["timeout"], 19)

    def test_describe_is_bounded_versioned_and_escapes_game_text(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            games = root / "games"
            games.mkdir()
            game = games / "knob_dodge"
            shutil.copytree(ROOT / "games/knob_dodge", game)
            manifest = game / "game.toml"
            manifest.write_text(
                'api = 1\nid = "knob_dodge"\nname = "KNOB DODGE"\n'
                'starter = "\\u001b[31m' + ("x" * 100) + '"\nview = "fixed"\n'
                '[tuning]\n' +
                "".join(f'key_{i}_with_a_very_long_name = {i}\n' for i in range(8)))
            with patch.object(tg, "ROOT", str(root)):
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(tg.cmd_describe_game(
                        SimpleNamespace(id="knob_dodge", json=True)), 0)
                data = json.loads(output.getvalue())
                self.assertEqual(data["schemaVersion"], 1)
                self.assertEqual(data["id"], "knob_dodge")
                self.assertTrue(data["hasRules"])
                self.assertTrue(data["starterTruncated"])
                self.assertEqual(data["tuningCount"], 8)
                self.assertEqual(len(data["tuning"]), 5)
                self.assertTrue(data["tuningNamesTruncated"])
                output = io.StringIO()
                with contextlib.redirect_stdout(output):
                    self.assertEqual(tg.cmd_describe_game(
                        SimpleNamespace(id="knob_dodge", json=False)), 0)
                self.assertNotIn("\x1b", output.getvalue())
                self.assertIn("Controls and part semantics", output.getvalue())

    def test_push_sends_versioned_crc_checked_chunks_and_finishes(self):
        port = Port()
        def replies(*_):
            if len(port.writes) == 1:
                return iter(["[DEV] install-capabilities 1 profile=147b game-api=1", "[DEV] ok"])
            return iter(["[DEV] ok"])
        with patch.object(tg, "game_directories",
                          return_value=[str(ROOT / "tests/fixtures/games/cave_hop")]), \
             patch.object(tg, "open_serial", return_value=port), \
             patch.object(tg, "read_lines", side_effect=replies), \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(tg.cmd_push(SimpleNamespace(id="cave_hop", board="147b", port=None)), 0)
        self.assertEqual(port.writes[0], b"install 1 capabilities\n")
        self.assertTrue(port.writes[1].startswith(b"install 1 begin cave_hop "))
        self.assertTrue(port.writes[-1] == b"install 1 finish\n")
        chunks = [line.decode().split() for line in port.writes[2:-1]]
        self.assertTrue(chunks)
        self.assertEqual(chunks[0][0:3], ["install", "1", "chunk"])
        decoded = b"".join(base64.b64decode(fields[5]) for fields in chunks)
        source = tg.game_format_module().build_bundle(
            tg.game_format_module().read_game(ROOT / "tests/fixtures/games/cave_hop"))
        self.assertEqual(decoded, source)
        self.assertTrue(all(int(fields[4], 16) == zlib.crc32(base64.b64decode(fields[5]))
                            for fields in chunks))

    def test_push_cancels_after_device_error(self):
        port = Port()
        responses = [iter(["[DEV] install-capabilities 1 profile=147b game-api=1", "[DEV] ok"]),
                     iter(["[DEV] ok"]), iter(["[DEV] error: not enough space"]),
                     iter(["[DEV] ok"])]
        with patch.object(tg, "game_directories",
                          return_value=[str(ROOT / "tests/fixtures/games/cave_hop")]), \
             patch.object(tg, "open_serial", return_value=port), \
             patch.object(tg, "read_lines", side_effect=responses), \
             contextlib.redirect_stdout(io.StringIO()), \
             self.assertRaisesRegex(RuntimeError, "not enough space"):
            tg.cmd_push(SimpleNamespace(id="cave_hop", board="147b", port=None))
        self.assertEqual(port.writes[-1], b"install 1 cancel\n")

    def test_push_requires_an_explicit_board_before_opening_a_port(self):
        error = io.StringIO()
        with patch("sys.argv", ["tg.py", "push", "cave_hop"]), \
             patch.object(tg, "open_serial") as serial, \
             contextlib.redirect_stderr(error), self.assertRaises(SystemExit) as raised:
            tg.main()
        self.assertEqual(raised.exception.code, 2)
        self.assertIn("--board", error.getvalue())
        serial.assert_not_called()

    def test_push_refuses_unconfirmed_or_incompatible_firmware(self):
        for responses, message in (
                (["[DEV] install-capabilities 1 profile=169 game-api=1", "[DEV] ok"],
                 "does not match"),
                (["[DEV] install-capabilities 1 profile=147b game-api=2", "[DEV] ok"],
                 "incompatible"),
                (["[DEV] ok"], "did not report"),
                (["[DEV] install-capabilities 2 profile=147b game-api=1", "[DEV] ok"],
                 "invalid install capabilities"),
                (["[DEV] install-capabilities 1 profile=147b game-api=1",
                  "[DEV] install-capabilities 1 profile=147b game-api=1", "[DEV] ok"],
                 "duplicate"),
                ([], "No acknowledgement")):
            port = Port()
            with self.subTest(message=message), \
                 patch.object(tg, "game_directories",
                              return_value=[str(ROOT / "tests/fixtures/games/cave_hop")]), \
                 patch.object(tg, "open_serial", return_value=port), \
                 patch.object(tg, "read_lines", return_value=iter(responses)), \
                 self.assertRaisesRegex(RuntimeError, message):
                tg.cmd_push(SimpleNamespace(id="cave_hop", board="147b", port=None))
            self.assertEqual(port.writes, [b"install 1 capabilities\n"])

    def test_game_bundle_cli_rejects_symlink_paths(self):
        with tempfile.TemporaryDirectory() as root, tempfile.TemporaryDirectory() as outside:
            root = Path(root)
            (root / "games").symlink_to(outside, target_is_directory=True)
            with patch.object(tg, "ROOT", str(root)), self.assertRaises(RuntimeError):
                tg.game_directories()

            (root / "games").unlink()
            (root / "games").mkdir()
            shutil.copytree(ROOT / "tests/fixtures/games/cave_hop", root / "games/cave_hop")
            (root / "build").symlink_to(outside, target_is_directory=True)
            with patch.object(tg, "ROOT", str(root)), self.assertRaises(RuntimeError):
                tg.safe_build_target("cave_hop.tgb")

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
