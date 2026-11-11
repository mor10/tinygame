"""Checking: each broken copy of cave_hop gives the expected problem, in the right place."""

import shutil
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from tgpack import PackError, check_game, pack_game  # noqa: E402

CAVE_HOP = ROOT / "games" / "cave_hop"


class BrokenGame(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name) / "cave_hop"
        shutil.copytree(CAVE_HOP, self.folder)

    def edit(self, file, old, new, count=1):
        path = self.folder / file
        text = path.read_text()
        self.assertGreaterEqual(text.count(old), 1, f"{old!r} not in {file}")
        path.write_text(text.replace(old, new, count))

    def line_of(self, file, marker):
        for number, line in enumerate((self.folder / file).read_text().splitlines(), 1):
            if marker in line:
                return number
        self.fail(f"{marker!r} not in {file}")

    def expect(self, code, file, line=None, marker=None, words=None):
        """The check must report `code` in `file` at the line holding `marker`."""
        if marker is not None:
            line = self.line_of(file, marker)
        problems = check_game(self.folder)
        found = [p for p in problems if p.code == code]
        self.assertTrue(found, f"no {code!r} among {[p.code for p in problems]}")
        problem = found[0]
        self.assertEqual(problem.file, file)
        self.assertEqual(problem.line, line)
        self.assertTrue(problem.message and problem.hint)
        if words:
            self.assertIn(words, problem.message)
        with self.assertRaises(PackError):
            pack_game(self.folder)
        return problems

    # ---- game.toml -------------------------------------------------------

    def test_clean_copy_has_no_problems(self):
        self.assertEqual(check_game(self.folder), [])

    def test_toml_syntax(self):
        self.edit("game.toml", 'name = "CAVE HOP"', 'name = "CAVE HOP')
        self.expect("toml-syntax", "game.toml", 3)

    def test_missing_required_key(self):
        self.edit("game.toml", 'starter = "platformer"\n', "")
        self.expect("missing-key", "game.toml", None, words="starter")

    def test_wrong_api(self):
        self.edit("game.toml", "api = 1", "api = 2")
        self.expect("bad-api", "game.toml", 1)

    def test_bad_id(self):
        self.edit("game.toml", 'id = "cave_hop"', 'id = "Cave-Hop"')
        self.expect("bad-id", "game.toml", 2)

    def test_id_must_match_folder(self):
        self.edit("game.toml", 'id = "cave_hop"', 'id = "other_game"')
        self.expect("id-folder", "game.toml", 2)

    def test_name_too_long(self):
        self.edit("game.toml", 'name = "CAVE HOP"', 'name = "CAVE HOP ADVENTURE"')
        self.expect("bad-name", "game.toml", 3)

    def test_name_not_ascii(self):
        self.edit("game.toml", 'name = "CAVE HOP"', 'name = "CAVÉ HOP"')
        self.expect("bad-name", "game.toml", 3)

    def test_bad_view(self):
        self.edit("game.toml", 'view = "standard"', 'view = "wide"')
        self.expect("bad-view", "game.toml", 6)

    def test_unknown_top_level_key(self):
        self.edit("game.toml", 'view = "standard"', 'view = "standard"\ncolour = "red"')
        self.expect("unknown-key", "game.toml", 7)

    def test_unknown_tile_key(self):
        self.edit("game.toml", "[tile.brick]\nsprite", "[tile.brick]\nsticky = true\nsprite")
        self.expect("unknown-tile-key", "game.toml", marker="sticky")

    def test_unknown_kind_key(self):
        self.edit("game.toml", "[kind.coin]\n", "[kind.coin]\nwings = true\n")
        self.expect("unknown-kind-key", "game.toml", marker="wings")

    def test_unknown_tuning_key(self):
        self.edit("game.toml", "run_speed = 6", "run_speed = 6\nwind = 2")
        self.expect("unknown-tuning", "game.toml", marker="wind")

    def test_tuning_out_of_range(self):
        self.edit("game.toml", "gravity = 30", "gravity = 500")
        self.expect("out-of-range", "game.toml", marker="gravity")

    def test_tuning_not_a_number(self):
        self.edit("game.toml", "jump_height = 3", 'jump_height = "high"')
        self.expect("not-a-number", "game.toml", marker="jump_height")

    def test_kind_speed_out_of_range(self):
        self.edit("game.toml", "speed = 2", "speed = 50")
        self.expect("out-of-range", "game.toml", marker="speed = 50")

    def test_points_must_be_whole(self):
        self.edit("game.toml", "points = 10", "points = 2.5")
        self.expect("not-a-number", "game.toml", marker="points")

    def test_unknown_movement(self):
        self.edit("game.toml", 'movement = "platformer"', 'movement = "flying"', count=1)
        self.expect("unknown-part", "game.toml", marker='movement = "flying"')

    def test_unknown_behaviour(self):
        self.edit("game.toml", 'behaviour = "walker"', 'behaviour = "chaser"')
        self.expect("unknown-part", "game.toml", marker="chaser")

    def test_unknown_on_touch_player(self):
        self.edit("game.toml", 'on_touch_player = "hurt"', 'on_touch_player = "explode"')
        self.expect("unknown-part", "game.toml", marker="explode")

    def test_tile_sprite_missing(self):
        self.edit("game.toml", 'sprite = "brick"', 'sprite = "bricks"')
        self.expect("unknown-sprite", "game.toml", marker="bricks")

    def test_kind_sprite_missing(self):
        self.edit("game.toml", 'sprite = "slime"', 'sprite = "slimey"')
        self.expect("unknown-sprite", "game.toml", marker="slimey")

    def test_tile_sprite_wrong_size(self):
        self.edit("game.toml", '[tile.brick]\nsprite = "brick"', '[tile.brick]\nsprite = "hero"')
        problems = check_game(self.folder)
        hit = [p for p in problems if p.code == "tile-sprite-size"]
        self.assertEqual(len(hit), 1)
        # The first `sprite = "hero"` line is the tile's (the player kind comes later).
        self.assertEqual((hit[0].file, hit[0].line),
                         ("game.toml", self.line_of("game.toml", 'sprite = "hero"')))

    def test_bad_palette_colour(self):
        (self.folder / "game.toml").write_text(
            (self.folder / "game.toml").read_text() + '\n[palette]\nbackground = "sky blue"\n')
        self.expect("bad-colour", "game.toml", marker="sky blue")

    def test_no_player_kind(self):
        self.edit("game.toml", "player = true\n", "")
        problems = [p for p in check_game(self.folder) if p.code == "no-player-kind"]
        self.assertEqual(len(problems), 1)
        self.assertEqual(problems[0].file, "game.toml")

    # ---- sprites.txt -----------------------------------------------------

    def test_sprite_row_too_short(self):
        self.edit("sprites.txt", "77877787", "7787778")
        self.expect("sprite-row-width", "sprites.txt", 5)

    def test_sprite_missing_row(self):
        self.edit("sprites.txt", "77777777\n77877787\n", "77777777\n")
        self.expect("sprite-rows", "sprites.txt", 3)

    def test_sprite_bad_character(self):
        self.edit("sprites.txt", "77877787", "77x77787")
        self.expect("sprite-char", "sprites.txt", 5)

    def test_sprite_too_big(self):
        self.edit("sprites.txt", "== ground 8x8", "== ground 40x8")
        self.expect("sprite-too-big", "sprites.txt", 3)

    def test_sprite_duplicate(self):
        self.edit("sprites.txt", "== brick 8x8", "== ground 8x8")
        self.expect("sprite-duplicate", "sprites.txt", 13)

    # ---- levels.txt ------------------------------------------------------

    def test_level_row_width(self):
        self.edit("levels.txt", "..P..........B", "..P.........B")
        self.expect("level-row-width", "levels.txt", marker="..P.........B")

    def test_level_too_narrow(self):
        lines = (self.folder / "levels.txt").read_text().splitlines()
        head = [l for l in lines if l.startswith(("==", "legend"))]
        rows = [l[:10] for l in lines if not l.startswith(("==", "legend"))]
        (self.folder / "levels.txt").write_text("\n".join(head + rows) + "\n")
        self.expect("level-too-narrow", "levels.txt", 4)

    def test_level_too_short(self):
        text = (self.folder / "levels.txt").read_text().replace("." * 40 + "\n", "", 2)
        (self.folder / "levels.txt").write_text(text)
        self.expect("level-too-short", "levels.txt", 1)

    def test_unknown_legend_name(self):
        self.edit("levels.txt", "o = coin", "o = coinn")
        self.expect("legend-unknown-name", "levels.txt", marker="coinn")

    def test_character_not_in_legend(self):
        self.edit("levels.txt", "oooo", "ooxo")
        self.expect("level-unknown-char", "levels.txt", marker="ooxo")

    def test_no_player_in_level(self):
        self.edit("levels.txt", "..P....", ".......")
        self.expect("no-player-spawn", "levels.txt", 1)

    def test_two_players_in_level(self):
        self.edit("levels.txt", "ooo....\n", "ooo..P.\n")
        self.expect("many-player-spawns", "levels.txt", marker="ooo..P.")

    # ---- limits and files ------------------------------------------------

    def test_too_many_levels(self):
        text = (self.folder / "levels.txt").read_text()
        (self.folder / "levels.txt").write_text(text + ("\n" + text) * 32)
        problems = check_game(self.folder)
        self.assertIn("too-many-levels", [p.code for p in problems])

    def test_bundle_too_big(self):
        # A very long rules.lua is what fills the bundle.
        (self.folder / "rules.lua").write_text("-- " + "x" * 262_144 + "\n")
        self.expect("bundle-too-big", "game.toml", None)

    def test_missing_sprites_file(self):
        (self.folder / "sprites.txt").unlink()
        self.expect("missing-file", "sprites.txt", None)

    def test_rules_lua_is_optional(self):
        (self.folder / "rules.lua").unlink()
        self.assertEqual(check_game(self.folder), [])
        self.assertTrue(pack_game(self.folder))

    def test_problem_text_names_file_and_line(self):
        self.edit("game.toml", "api = 1", "api = 2")
        problem = check_game(self.folder)[0]
        self.assertIn("game.toml:1", problem.text("games/cave_hop"))
        self.assertIn("Fix:", problem.text())


class ParamsComeFromPartsFile(unittest.TestCase):
    def test_ranges_are_read_from_parts_toml(self):
        source = (ROOT / "tools" / "tgpack" / "source.py").read_text()
        for needle in ("= 120", "= 8)", "== 100000", "max=12"):
            self.assertNotIn(needle, source)
        self.assertIn("parts.toml", source)


if __name__ == "__main__":
    unittest.main()
