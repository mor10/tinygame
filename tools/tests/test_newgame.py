"""`tg new` makes a game that passes the check and packs."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from tgpack import check_game, pack_game  # noqa: E402
from tgpack.newgame import NewGameError, new_game  # noqa: E402


class NewGame(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.games = Path(self.tmp.name)

    def test_new_game_checks_and_packs(self):
        folder = new_game("space_cat", "platformer", games_dir=self.games)
        self.assertEqual(folder, self.games / "space_cat")
        self.assertEqual(check_game(folder), [])
        data = pack_game(folder)
        self.assertEqual(data[:4], b"TGB1")
        text = (folder / "game.toml").read_text()
        self.assertIn('id = "space_cat"', text)
        self.assertIn('name = "SPACE CAT"', text)

    def test_name_option(self):
        folder = new_game("rocket", "platformer", "Rocket Run", games_dir=self.games)
        self.assertIn('name = "Rocket Run"', (folder / "game.toml").read_text())
        self.assertEqual(check_game(folder), [])

    def test_refuses_existing_folder(self):
        new_game("rocket", "platformer", games_dir=self.games)
        with self.assertRaises(NewGameError):
            new_game("rocket", "platformer", games_dir=self.games)

    def test_refuses_bad_ids(self):
        for bad in ("Rocket", "1rocket", "rock-et", "", "a" * 16, "../x"):
            with self.assertRaises(NewGameError, msg=bad):
                new_game(bad, "platformer", games_dir=self.games)
        self.assertEqual(list(self.games.iterdir()), [])

    def test_refuses_bad_name_and_kind(self):
        with self.assertRaises(NewGameError):
            new_game("rocket", "platformer", "A" * 14, games_dir=self.games)
        with self.assertRaises(NewGameError):
            new_game("rocket", "racing", games_dir=self.games)
        self.assertEqual(list(self.games.iterdir()), [])

    def test_the_starter_itself_is_clean(self):
        self.assertEqual(check_game(ROOT / "starters" / "platformer"), [])


class Command(unittest.TestCase):
    """The tg.py subcommands: --json output and exit codes."""

    def run_tg(self, *args):
        return subprocess.run([sys.executable, str(ROOT / "tools" / "tg.py"), *args],
                              capture_output=True, text=True, cwd=ROOT)

    def test_check_json_ok(self):
        result = self.run_tg("check", "cave_hop", "--json")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue(json.loads(result.stdout)["ok"])

    def test_check_all_games_and_starters(self):
        result = self.run_tg("check", "--json")
        self.assertEqual(result.returncode, 0, result.stdout)
        ids = {g["id"] for g in json.loads(result.stdout)["games"]}
        self.assertTrue({"cave_hop", "platformer"} <= ids)

    def test_check_failure_exits_non_zero(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "broken"
            folder.mkdir()
            result = self.run_tg("check", str(folder), "--json")
        self.assertNotEqual(result.returncode, 0)
        report = json.loads(result.stdout)
        self.assertFalse(report["ok"])
        self.assertTrue(report["games"][0]["problems"])

    def test_pack_writes_a_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp) / "x.tgb"
            result = self.run_tg("pack", "cave_hop", "-o", str(out), "--json")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(out.read_bytes(), pack_game(ROOT / "games" / "cave_hop"))
            self.assertTrue(json.loads(result.stdout)["ok"])

    def test_pack_refuses_broken_game(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp) / "broken"
            folder.mkdir()
            out = Path(tmp) / "x.tgb"
            result = self.run_tg("pack", str(folder), "-o", str(out), "--json")
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(out.exists())
            self.assertFalse(json.loads(result.stdout)["ok"])

    def test_new_command(self):
        with tempfile.TemporaryDirectory() as tmp:
            result = self.run_tg("new", "zed_game", "--kind", "platformer",
                                 "--games-dir", tmp, "--json")
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertEqual(check_game(Path(tmp) / "zed_game"), [])
            again = self.run_tg("new", "zed_game", "--kind", "platformer",
                                "--games-dir", tmp, "--json")
            self.assertNotEqual(again.returncode, 0)


if __name__ == "__main__":
    unittest.main()
