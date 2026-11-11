"""Makes a new game folder from a starter template (`tg new`)."""

import re
import shutil
from pathlib import Path

from .source import ID_RE, MAX_NAME_CHARS, ROOT, STARTERS_DIR, is_printable_ascii

GAMES_DIR = ROOT / "games"


class NewGameError(Exception):
    """The new game could not be made; the text says why and how to fix it."""


def default_name(game_id):
    return game_id.replace("_", " ").upper()[:MAX_NAME_CHARS].strip() or game_id.upper()


def toml_string(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def new_game(game_id, kind, name=None, games_dir=None, starters_dir=None):
    """Copies starters/<kind> to <games_dir>/<game_id>. Returns the new folder."""
    games_dir = Path(games_dir) if games_dir else GAMES_DIR
    starters_dir = Path(starters_dir) if starters_dir else STARTERS_DIR
    if not ID_RE.fullmatch(game_id):
        raise NewGameError(
            f'"{game_id}" cannot be a game id. An id starts with a small letter and then has '
            "only small letters, numbers and _, at most 15 characters. Try something like "
            "space_cat.")
    template = starters_dir / kind
    if not (template / "game.toml").is_file():
        known = sorted(p.name for p in starters_dir.iterdir() if p.is_dir()) \
            if starters_dir.is_dir() else []
        raise NewGameError(f'There is no starter called "{kind}". '
                           f"Use one of: {', '.join(known) or '(none found)'}.")
    if name is None:
        name = default_name(game_id)
    if not 1 <= len(name) <= MAX_NAME_CHARS or not is_printable_ascii(name):
        raise NewGameError(
            f'The name "{name}" does not fit on the console. Use 1 to {MAX_NAME_CHARS} plain '
            "characters (letters, numbers, spaces). No emoji or accents.")
    target = games_dir / game_id
    if target.exists():
        raise NewGameError(f"There is already a game at {target}. Pick a different id, or "
                           "delete that folder first if you really want to start again.")
    games_dir.mkdir(parents=True, exist_ok=True)
    shutil.copytree(template, target, ignore=shutil.ignore_patterns("__pycache__"))

    toml_path = target / "game.toml"
    text = toml_path.read_text()
    text, ids = re.subn(r'(?m)^id\s*=.*$', lambda _: f"id = {toml_string(game_id)}",
                        text, count=1)
    text, names = re.subn(r'(?m)^name\s*=.*$', lambda _: f"name = {toml_string(name)}",
                          text, count=1)
    if ids != 1 or names != 1:
        shutil.rmtree(target)
        raise NewGameError(f"The starter at {template} has no id or name line in game.toml, "
                           "so it cannot be copied. This is a problem with the starter.")
    toml_path.write_text(text)

    readme = target / "README.md"
    if readme.is_file():
        lines = readme.read_text().splitlines(keepends=True)
        if lines and lines[0].startswith("# "):
            lines[0] = f"# {name}\n"
            readme.write_text("".join(lines))
    return target
