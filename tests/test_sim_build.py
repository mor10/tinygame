import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location("sim_build", TOOLS / "sim_build.py")
sim_build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sim_build)

GAME_DIRS = [
    ROOT / "tests/fixtures/games/cave_hop",
    ROOT / "tests/fixtures/games/lua_tick",
]


class SimulatorBuildTests(unittest.TestCase):
    def test_embedded_bundles_are_complete_and_deterministic(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            source = sim_build.embedded_bundle_source(output, GAME_DIRS)
            generated = source.read_text()
            sim_build.embedded_bundle_source(output, list(reversed(GAME_DIRS)))
            repeated = source.read_text()

        self.assertEqual(source.name, "embedded_games.cpp")
        self.assertEqual(generated, repeated)
        self.assertIn("tg_sim_bundles[]", generated)
        self.assertIn("tg_sim_bundle_count", generated)
        self.assertIn('"cave_hop"', generated)
        self.assertIn('"lua_tick"', generated)
        self.assertIn("bundle_0", generated)
        self.assertIn("bundle_1", generated)

    def test_empty_game_set_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(RuntimeError):
                sim_build.embedded_bundle_source(Path(temporary), [])

    def test_manifest_names_are_escaped_as_cpp_strings(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / "cave_hop"
            shutil.copytree(GAME_DIRS[0], game)
            manifest = game / "game.toml"
            manifest.write_text(manifest.read_text().replace(
                'name = "CAVE HOP"', 'name = "CAVE \\"HOP\\""'))
            objects = root / "objects"
            objects.mkdir()
            source = sim_build.embedded_bundle_source(objects, [game])
            generated = source.read_text()

        self.assertIn(json.dumps('CAVE "HOP"'), generated)


if __name__ == "__main__":
    unittest.main()
