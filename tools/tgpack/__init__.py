"""TinyGame packer and checker: turns a game folder into a `.tgb` bundle.

The bundle format is docs/formats/bundle.md. Standard library only.
"""

from .check import check_game
from .pack import PackError, build_bundle, pack_game
from .problem import Problem

__all__ = ["Problem", "PackError", "check_game", "build_bundle", "pack_game"]
