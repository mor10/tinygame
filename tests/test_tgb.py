import importlib.util
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("tgb", ROOT / "tools/tgb.py")
tgb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tgb)
SOURCE_GAME = ROOT / "tests/fixtures/games/cave_hop"
LUA_TICK_GAME = ROOT / "tests/fixtures/games/lua_tick"
LUA_DODGE_GAME = ROOT / "games/knob_dodge"
LUA_MODULES_GAME = ROOT / "tests/fixtures/games/lua_modules"


class BundleTests(unittest.TestCase):
    def setUp(self):
        self.files = tgb.read_game(SOURCE_GAME)
        self.bundle = tgb.build_bundle(self.files)

    def test_round_trip_is_deterministic(self):
        self.assertEqual(self.bundle, tgb.build_bundle(dict(reversed(list(self.files.items())))))
        self.assertEqual(tgb.parse_bundle(self.bundle), self.files)

    def test_golden_vectors(self):
        valid = bytes.fromhex((ROOT / "tests/fixtures/tgb_v1_valid.hex").read_text())
        invalid = bytes.fromhex((ROOT / "tests/fixtures/tgb_v1_invalid.hex").read_text())
        self.assertEqual(tgb.parse_bundle(valid), self.files)
        with self.assertRaises(tgb.BundleError):
            tgb.parse_bundle(invalid)

    def test_lua_tick_fixture_matches_its_golden_bundle(self):
        files = tgb.read_game(LUA_TICK_GAME)
        bundle = tgb.build_bundle(files)
        golden = bytes.fromhex((ROOT / "tests/fixtures/tgb_v1_lua_tick.hex").read_text())
        self.assertEqual(bundle, golden)
        self.assertEqual(tgb.parse_bundle(golden), files)

    def test_lua_modules_fixture_matches_its_golden_bundle(self):
        files = tgb.read_game(LUA_MODULES_GAME)
        bundle = tgb.build_bundle(files)
        golden = bytes.fromhex(
            (ROOT / "tests/fixtures/tgb_v1_lua_modules.hex").read_text())
        self.assertEqual(bundle, golden)
        self.assertEqual(tgb.parse_bundle(golden), files)

    def test_lua_source_limit_applies_to_packing_and_bundle_loading(self):
        files = tgb.read_game(LUA_MODULES_GAME)
        files["modules/palette.lua"] = b"--" + b" " * (tgb.MAX_LUA_SOURCE_BYTES - 2)
        tgb.build_bundle(files)
        files["modules/palette.lua"] += b" "
        with self.assertRaisesRegex(tgb.BundleError, "64 KiB"):
            tgb.build_bundle(files)
        with patch.object(tgb, "MAX_LUA_SOURCE_BYTES", tgb.MAX_LUA_SOURCE_BYTES + 1):
            oversized = tgb.build_bundle(files)
        with self.assertRaisesRegex(tgb.BundleError, "64 KiB"):
            tgb.parse_bundle(oversized)

    def test_literal_toml_and_integer_spellings_are_valid(self):
        files = tgb.read_game(LUA_TICK_GAME)
        files["game.toml"] = (
            b'"api"=+1\n'
            b"'id'='lua_tick'\n'name'='LUA TICK'\n"
            b"'starter'='platformer'\n'view'='fixed'\n")
        self.assertEqual(tgb.parse_manifest(tgb.parse_bundle(tgb.build_bundle(files)))["id"],
                         "lua_tick")
        files["game.toml"] = (
            b"api=1\nid='lua_tick'\nname='   '\nstarter='\t'\nview='fixed'\n")
        manifest = tgb.parse_manifest(tgb.parse_bundle(tgb.build_bundle(files)))
        self.assertEqual((manifest["name"], manifest["starter"]), ("   ", "\t"))

    def test_knob_dodge_game_matches_its_golden_bundle(self):
        files = tgb.read_game(LUA_DODGE_GAME)
        bundle = tgb.build_bundle(files)
        golden = bytes.fromhex((ROOT / "tests/fixtures/tgb_v1_knob_dodge.hex").read_text())
        self.assertEqual(bundle, golden)
        self.assertEqual(tgb.parse_bundle(golden), files)

    def test_rejects_bad_headers_and_sections(self):
        with self.assertRaisesRegex(tgb.BundleError, "truncated"):
            tgb.parse_bundle(self.bundle[:20])
        for offset, value in ((0, ord("X")), (4, 99), (10, 1)):
            changed = bytearray(self.bundle)
            changed[offset] = value
            with self.subTest(offset=offset), self.assertRaises(tgb.BundleError):
                tgb.parse_bundle(bytes(changed))

    def test_rejects_table_checksum_and_file_checksum_errors(self):
        changed = bytearray(self.bundle)
        changed[32] ^= 1
        with self.assertRaisesRegex(tgb.BundleError, "table checksum"):
            tgb.parse_bundle(bytes(changed))

        changed = bytearray(self.bundle)
        entry = tgb.HEADER_SIZE
        name_length, _, offset, length, _ = tgb.ENTRY.unpack_from(
            changed, entry)
        offset += tgb.ENTRY.size + tgb._align4(name_length)
        changed[offset] ^= 1
        with self.assertRaisesRegex(tgb.BundleError, "checksum"):
            tgb.parse_bundle(bytes(changed))

    def test_rejects_overlapping_and_out_of_range_files(self):
        header = tgb.HEADER.unpack_from(self.bundle)
        table_offset, table_length = header[5], header[6]
        table = bytearray(self.bundle[table_offset:table_offset + table_length])
        first_path_length = tgb.ENTRY.unpack_from(table)[0]
        second_entry = tgb.ENTRY.size + tgb._align4(first_path_length)
        first_offset = tgb.ENTRY.unpack_from(table)[2]

        struct.pack_into("<I", table, second_entry + 4, first_offset)
        self.assert_invalid_table(table_offset, table)

        table = bytearray(self.bundle[table_offset:table_offset + table_length])
        struct.pack_into("<I", table, 4, len(self.bundle) - 1)
        self.assert_invalid_table(table_offset, table)

    def assert_invalid_table(self, table_offset, table):
        changed = bytearray(self.bundle)
        changed[table_offset:table_offset + len(table)] = table
        struct.pack_into("<I", changed, 32, zlib.crc32(table) & 0xFFFFFFFF)
        with self.assertRaises(tgb.BundleError):
            tgb.parse_bundle(bytes(changed))

    def test_rejects_invalid_utf8_before_returning_contents(self):
        changed = bytearray(self.bundle)
        header = tgb.HEADER.unpack_from(changed)
        table_offset, table_length = header[5], header[6]
        table = bytearray(changed[table_offset:table_offset + table_length])
        name_length, _, offset, length, _ = tgb.ENTRY.unpack_from(table)
        changed[offset] = 0xFF
        struct.pack_into("<I", table, 12, zlib.crc32(changed[offset:offset + length])
                         & 0xFFFFFFFF)
        changed[table_offset:table_offset + table_length] = table
        struct.pack_into("<I", changed, 32, zlib.crc32(table) & 0xFFFFFFFF)
        with self.assertRaisesRegex(tgb.BundleError, "UTF-8"):
            tgb.parse_bundle(bytes(changed))

    def test_validates_manifest_identity_and_finite_tuning(self):
        manifest = self.files["game.toml"]
        self.assertTrue(tgb._valid_id("a__b"))
        with self.assertRaisesRegex(tgb.BundleError, "does not match folder"):
            tgb.validate_files(self.files, "other")
        bad = dict(self.files)
        bad["game.toml"] = manifest.replace(b"gravity = 30", b"gravity = nan")
        with self.assertRaisesRegex(tgb.BundleError, "finite"):
            tgb.validate_files(bad)
        bad["game.toml"] = manifest.replace(b"speed = 2", b"speed = inf")
        with self.assertRaisesRegex(tgb.BundleError, "finite"):
            tgb.validate_files(bad)
        bad["game.toml"] = manifest.replace(b"api = 1", b"api = 1.0")
        with self.assertRaisesRegex(tgb.BundleError, "api must be the integer"):
            tgb.validate_files(bad)

    def test_rejects_unsafe_paths_and_limits(self):
        for name in ("../escape.lua", "/absolute.lua", "modules/../escape.lua",
                     "modules\\escape.lua", "modules/\x00escape.lua", "other.txt",
                     "modules/1helper.lua", "modules/hel-per.lua",
                     "modules/" + "a" * 117 + ".lua"):
            with self.subTest(name=name), self.assertRaises(tgb.BundleError):
                tgb._member_name(name)
        for name in ("modules/_helper.lua", "modules/A123.lua",
                     "modules/" + "a" * 116 + ".lua"):
            self.assertEqual(tgb._member_name(name), name)
        with patch.object(tgb, "MAX_FILE_BYTES", 8), self.assertRaisesRegex(
                tgb.BundleError, "1 MiB"):
            tgb.validate_files(self.files)

    def test_rejects_source_symlinks(self):
        with tempfile.TemporaryDirectory() as temp:
            game = Path(temp) / "cave_hop"
            shutil.copytree(SOURCE_GAME, game)
            (game / "modules").mkdir()
            (game / "modules/escape.lua").symlink_to(Path(temp) / "outside.lua")
            with self.assertRaisesRegex(tgb.BundleError, "Symlinks"):
                tgb.read_game(game)


if __name__ == "__main__":
    unittest.main()
