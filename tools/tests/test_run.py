"""`tg run`: argument handling, failure reporting and screenshots."""

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TG = ROOT / "tools" / "tg.py"


def tg(*args, only_stdout=False):
    done = subprocess.run([sys.executable, str(TG), *map(str, args)], capture_output=True,
                          text=True, cwd=ROOT)
    return done.returncode, done.stdout if only_stdout else done.stdout + done.stderr


class RunTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)

    def steps(self, text, name="t.steps"):
        path = Path(self.dir.name) / name
        path.write_text(text)
        return path

    def test_a_passing_script_exits_zero_on_both_boards(self):
        path = self.steps("wait 10\nexpect tick == 10\nexpect hurts == 0\n")
        code, out = tg("run", "cave_hop", "--steps", path)
        self.assertEqual(code, 0, out)
        self.assertIn("board a (320x172)", out)
        self.assertIn("board b (280x240)", out)

    def test_every_failed_expect_is_reported_with_file_and_line_and_the_run_fails(self):
        path = self.steps("wait 5\nexpect score == 99\nwait 5\nexpect tick > 100\nexpect tick == 10\n")
        code, out = tg("run", "cave_hop", "--steps", path, "--board", "a")
        self.assertNotEqual(code, 0)
        self.assertIn(f"{path}:2: expected score == 99, found 0", out)
        self.assertIn(f"{path}:4: expected tick > 100, found 10", out)
        self.assertNotIn(":5:", out)  # the script kept going; the third expect held

    def test_a_mistake_in_the_script_names_the_line(self):
        path = self.steps("wait 5\njump now\nexpect score ~ 3\n")
        code, out = tg("run", "cave_hop", "--steps", path)
        self.assertNotEqual(code, 0)
        self.assertIn(f"{path}:2: I do not know 'jump'", out)
        self.assertIn(f"{path}:3: expect needs", out)

    def test_missing_steps_file_and_unknown_game(self):
        code, out = tg("run", "cave_hop", "--steps", Path(self.dir.name) / "nope.steps")
        self.assertEqual(code, 1)
        self.assertIn("cannot find the steps file", out)
        code, out = tg("run", "no_such_game", "--steps", self.steps("wait 1\n"))
        self.assertEqual(code, 1)
        self.assertIn("there is no game", out)

    def test_bad_arguments_are_refused(self):
        path = self.steps("wait 1\n")
        self.assertEqual(tg("run", "cave_hop", "--steps", path, "--board", "c")[0], 2)
        self.assertEqual(tg("run", "cave_hop")[0], 2)  # --steps is needed

    def test_json_result(self):
        path = self.steps("wait 3\nexpect tick == 4\n")
        code, out = tg("run", "cave_hop", "--steps", path, "--board", "b", "--json", only_stdout=True)
        self.assertEqual(code, 1)
        data = json.loads(out)
        self.assertFalse(data["ok"])
        run = data["runs"][0]
        self.assertEqual((run["board"], run["width"], run["height"]), ("b", 280, 240))
        self.assertEqual(run["failures"], [{"line": 2, "expected": "expected tick == 4", "found": "3"}])

    def test_probe_and_screenshot(self):
        path = self.steps("tilt 1 0\nwait 20\n")
        shot = Path(self.dir.name) / "shot.png"
        code, out = tg("run", "cave_hop", "--steps", path, "--probe", "--shot", shot)
        self.assertEqual(code, 0, out)
        self.assertIn("tick 20", out)
        self.assertIn("actor", out)
        for board in "ab":
            png = Path(self.dir.name) / f"shot_{board}.png"
            self.assertTrue(png.read_bytes().startswith(b"\x89PNG"), board)

    def test_a_game_that_does_not_pass_check_is_not_run(self):
        game = Path(self.dir.name) / "broken"
        game.mkdir()
        (game / "game.toml").write_text("nonsense\n")
        code, out = tg("run", game, "--steps", self.steps("wait 1\n"))
        self.assertEqual(code, 1)
        self.assertIn("not run", out)


if __name__ == "__main__":
    unittest.main()
