"""Checks a game folder and lists what is wrong with it.

The rules for the text files are in source.py; this adds the check that the
finished bundle fits on the console.
"""

from .problem import Problem
from .source import MAX_BUNDLE_BYTES, load_game


def check_loaded(game, problems):
    """Adds whole-bundle checks to the problems found while loading."""
    if problems:
        return problems
    from .pack import build_bundle

    size = len(build_bundle(game))
    if size > MAX_BUNDLE_BYTES:
        return [Problem("bundle-too-big", "game.toml", None,
                        f"The packed game is {size:,} bytes. A game can be at most "
                        f"{MAX_BUNDLE_BYTES:,} bytes.",
                        "Make it smaller: remove pictures or levels the game does not use, "
                        "or shorten rules.lua.")]
    return problems


def check_game(folder):
    """Returns the list of Problems in the game at `folder` (empty when it is fine)."""
    game, problems = load_game(folder)
    return check_loaded(game, problems)
