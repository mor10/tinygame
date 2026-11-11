"""Writes a game as `.tgb` bundle bytes (docs/formats/bundle.md, version 1)."""

import struct
import zlib

from .source import API_VERSION, MAX_BUNDLE_BYTES, load_game

MAGIC = b"TGB1"
HEADER_SIZE = 16
TABLE_ENTRY_SIZE = 12


class PackError(Exception):
    """The game has problems, so no bundle was made."""

    def __init__(self, problems):
        super().__init__(f"{len(problems)} problem(s) found")
        self.problems = problems


def string(text):
    data = text.encode("ascii")
    return bytes([len(data)]) + data


def meta_section(game):
    return (string(game.id) + string(game.name) + string(game.author) + string(game.starter)
            + bytes([game.view, game.playfield_cols, game.playfield_rows]))


def palette_section(game):
    return struct.pack("<18H", *game.palette, game.background, game.ink)


def sprite_section(game):
    out = bytearray(struct.pack("<H", len(game.sprites)))
    for sprite in game.sprites:
        out += string(sprite.name) + bytes([sprite.width, sprite.height])
        packed = bytearray((len(sprite.pixels) + 1) // 2)
        for i, index in enumerate(sprite.pixels):
            packed[i // 2] |= index << (4 if i % 2 else 0)
        out += packed
    return bytes(out)


def tile_section(game):
    out = bytearray([len(game.tiles)])
    for tile in game.tiles:
        out += struct.pack("<HH", tile.sprite, tile.flags)
    return bytes(out)


def kind_section(game):
    out = bytearray([len(game.kinds)])
    for kind in game.kinds:
        out += string(kind.name)
        out += struct.pack("<HBBBBfi", kind.sprite, kind.movement, kind.behaviour,
                           kind.flags, kind.on_touch_player, kind.speed, kind.points)
    return bytes(out)


def tuning_section(game):
    out = bytearray([len(game.tuning)])
    for key, value in game.tuning:
        out += string(key) + struct.pack("<f", value)
    return bytes(out)


def level_section(game):
    out = bytearray([len(game.levels)])
    for level in game.levels:
        out += string(level.name) + struct.pack("<HH", level.cols, level.rows)
        out += level.cells
        out += struct.pack("<H", len(level.spawns))
        for kind, col, row in level.spawns:
            out += struct.pack("<HHH", kind, col, row)
    return bytes(out)


def build_bundle(game):
    """Bundle bytes for a Game that loaded without problems."""
    sections = [
        (b"META", meta_section(game)),
        (b"PALT", palette_section(game)),
        (b"SPRT", sprite_section(game)),
        (b"TILE", tile_section(game)),
        (b"KIND", kind_section(game)),
        (b"TUNE", tuning_section(game)),
        (b"LEVL", level_section(game)),
    ]
    if game.rules is not None:
        sections.append((b"RULE", game.rules))
    offset = HEADER_SIZE + TABLE_ENTRY_SIZE * len(sections)
    table = bytearray()
    body = bytearray()
    for tag, data in sections:
        table += tag + struct.pack("<II", offset + len(body), len(data))
        body += data
    rest = bytes(table + body)
    header = MAGIC + struct.pack("<HHII", API_VERSION, len(sections),
                                 HEADER_SIZE + len(rest), zlib.crc32(rest))
    return header + rest


def pack_game(folder):
    """Checks and packs the game in `folder`. Raises PackError when it has problems."""
    from .check import check_loaded

    game, problems = load_game(folder)
    problems = check_loaded(game, problems)
    if problems:
        raise PackError(problems)
    return build_bundle(game)


__all__ = ["PackError", "build_bundle", "pack_game", "MAX_BUNDLE_BYTES"]
