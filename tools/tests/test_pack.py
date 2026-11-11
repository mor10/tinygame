"""Packing: cave_hop turns into a bundle that an independent reader agrees with."""

import struct
import sys
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from tgpack import pack_game  # noqa: E402

CAVE_HOP = ROOT / "games" / "cave_hop"


class Reader:
    """A small bundle reader written from docs/formats/bundle.md, sharing no code
    with the packer."""

    def __init__(self, data, at=0):
        self.data, self.at = data, at

    def take(self, fmt):
        values = struct.unpack_from("<" + fmt, self.data, self.at)
        self.at += struct.calcsize("<" + fmt)
        return values if len(values) > 1 else values[0]

    def raw(self, count):
        chunk = self.data[self.at:self.at + count]
        assert len(chunk) == count
        self.at += count
        return chunk

    def string(self):
        return self.raw(self.take("B")).decode("ascii")

    def done(self):
        return self.at == len(self.data)


def read_bundle(data):
    """Returns (header dict, {tag: section bytes}, [tags in table order])."""
    reader = Reader(data)
    header = {"magic": reader.raw(4)}
    header["api"], header["count"], header["size"], header["crc"] = reader.take("HHII")
    sections, order = {}, []
    for _ in range(header["count"]):
        tag = reader.raw(4).decode("ascii")
        offset, size = reader.take("II")
        assert offset + size <= len(data)
        sections[tag] = data[offset:offset + size]
        order.append((tag, offset, size))
    return header, sections, order


def read_sprites(section):
    reader = Reader(section)
    sprites = []
    for _ in range(reader.take("H")):
        name = reader.string()
        width, height = reader.take("BB")
        packed = reader.raw((width * height + 1) // 2)
        pixels = [(packed[i // 2] >> (4 if i % 2 else 0)) & 15 for i in range(width * height)]
        sprites.append((name, width, height, pixels))
    assert reader.done()
    return sprites


def read_kinds(section):
    reader = Reader(section)
    kinds = []
    for _ in range(reader.take("B")):
        name = reader.string()
        sprite, movement, behaviour, flags, touch, speed, points = reader.take("HBBBBfi")
        kinds.append(dict(name=name, sprite=sprite, movement=movement, behaviour=behaviour,
                          flags=flags, touch=touch, speed=speed, points=points))
    assert reader.done()
    return kinds


def read_levels(section):
    reader = Reader(section)
    levels = []
    for _ in range(reader.take("B")):
        name = reader.string()
        cols, rows = reader.take("HH")
        cells = reader.raw(cols * rows)
        spawns = [reader.take("HHH") for _ in range(reader.take("H"))]
        levels.append(dict(name=name, cols=cols, rows=rows, cells=cells, spawns=spawns))
    assert reader.done()
    return levels


def text_sprite(name):
    """Pixels of a sprite read straight from cave_hop's sprites.txt."""
    lines = (CAVE_HOP / "sprites.txt").read_text().splitlines()
    start = next(i for i, line in enumerate(lines) if line.startswith(f"== {name} "))
    height = int(lines[start].split("x")[1])
    rows = lines[start + 1:start + 1 + height]
    return [0 if ch == "." else int(ch, 16) for row in rows for ch in row]


class PackCaveHop(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = pack_game(CAVE_HOP)
        cls.header, cls.sections, cls.order = read_bundle(cls.data)

    def test_header(self):
        self.assertEqual(self.header["magic"], b"TGB1")
        self.assertEqual(self.header["api"], 1)
        self.assertEqual(self.header["count"], 8)
        self.assertEqual(self.header["size"], len(self.data))
        self.assertEqual(self.header["crc"], zlib.crc32(self.data[16:]))
        self.assertLessEqual(len(self.data), 262_144)

    def test_section_table(self):
        tags = [tag for tag, _, _ in self.order]
        self.assertEqual(sorted(tags),
                         sorted(["META", "PALT", "SPRT", "TILE", "KIND", "TUNE", "LEVL", "RULE"]))
        # Sections sit after the table, do not overlap and fill the file exactly.
        expected = 16 + 12 * len(self.order)
        for _, offset, size in sorted(self.order, key=lambda entry: entry[1]):
            self.assertEqual(offset, expected)
            expected += size
        self.assertEqual(expected, len(self.data))

    def test_meta(self):
        reader = Reader(self.sections["META"])
        self.assertEqual([reader.string() for _ in range(4)],
                         ["cave_hop", "CAVE HOP", "TinyGame", "platformer"])
        self.assertEqual(reader.take("BBB"), (0, 0, 0))
        self.assertTrue(reader.done())

    def test_default_palette(self):
        section = self.sections["PALT"]
        self.assertEqual(len(section), 36)
        values = struct.unpack("<18H", section)

        def to565(rgb):
            r, g, b = rgb >> 16, (rgb >> 8) & 255, rgb & 255
            return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

        self.assertEqual(values[0], 0)
        self.assertEqual(values[1], to565(0x2B2D42))
        self.assertEqual(values[2], 0xFFFF)
        self.assertEqual(values[4], to565(0xF25C54))
        self.assertEqual(values[15], to565(0x5C6378))
        self.assertEqual(values[16], to565(0xBFE6F5))   # background
        self.assertEqual(values[17], to565(0x2B2D42))   # ink
        # The whole BRIGHT row must match engine/testcard/testcard.cpp.
        source = (ROOT / "engine" / "testcard" / "testcard.cpp").read_text()
        bright = source[source.index('{"BRIGHT"'):]
        bright = bright[:bright.index("};")]
        import re
        colours = [int(h, 16) for h in re.findall(r"rgb\(0x([0-9A-Fa-f]{6})\)", bright)]
        self.assertEqual(len(colours), 17)
        self.assertEqual(list(values[1:]), [to565(c) for c in colours])

    def test_sprites(self):
        sprites = read_sprites(self.sections["SPRT"])
        self.assertEqual(len(sprites), 8)
        self.assertEqual([s[0] for s in sprites],
                         ["ground", "brick", "ledge", "spikes", "coin", "gem", "slime", "hero"])
        by_name = {s[0]: s for s in sprites}
        self.assertEqual(by_name["hero"][1:3], (16, 16))
        for name, width, height, pixels in sprites:
            if name != "hero":
                self.assertEqual((width, height), (8, 8))
            self.assertEqual(pixels, text_sprite(name), name)
        # Pixel 0 is the low nibble, pixel 1 the high nibble: coin row 1 is ".626665.".
        coin = by_name["coin"][3]
        self.assertEqual(coin[8:16], [0, 6, 2, 6, 6, 6, 5, 0])

    def test_tiles(self):
        reader = Reader(self.sections["TILE"])
        self.assertEqual(reader.take("B"), 4)
        tiles = [reader.take("HH") for _ in range(4)]
        self.assertTrue(reader.done())
        # ground, brick: solid; ledge: one-way; spikes: hazard. Sprite indexes 0-3.
        self.assertEqual(tiles, [(0, 1), (1, 1), (2, 2), (3, 4)])

    def test_kinds(self):
        kinds = read_kinds(self.sections["KIND"])
        self.assertEqual([k["name"] for k in kinds], ["player", "slime", "coin", "gem"])
        player, slime, coin, gem = kinds
        self.assertEqual((player["sprite"], player["movement"], player["behaviour"],
                          player["flags"], player["touch"]), (7, 1, 0, 1, 0))
        self.assertEqual((slime["sprite"], slime["movement"], slime["behaviour"],
                          slime["flags"], slime["touch"], slime["speed"]), (6, 1, 1, 0, 1, 2.0))
        self.assertEqual((coin["sprite"], coin["movement"], coin["behaviour"],
                          coin["flags"], coin["points"]), (4, 0, 2, 0, 10))
        self.assertEqual((gem["sprite"], gem["movement"], gem["behaviour"],
                          gem["flags"], gem["touch"], gem["points"]), (5, 0, 0, 0, 0, 0))

    def test_tuning(self):
        reader = Reader(self.sections["TUNE"])
        entries = {}
        for _ in range(reader.take("B")):
            key = reader.string()
            entries[key] = reader.take("f")
        self.assertTrue(reader.done())
        self.assertEqual(entries, {"gravity": 30.0, "jump_height": 3.0, "run_speed": 6.0})

    def test_level(self):
        levels = read_levels(self.sections["LEVL"])
        self.assertEqual(len(levels), 1)
        level = levels[0]
        self.assertEqual((level["name"], level["cols"], level["rows"]), ("cave 1", 40, 10))
        cells = level["cells"]

        def cell(col, row):
            return cells[row * 40 + col]

        # Tile numbers are 1-based in game.toml order: ground 1, brick 2, ledge 3, spikes 4.
        self.assertEqual(cells[9 * 40:], bytes([1]) * 40)
        self.assertEqual(cells[:6 * 40], bytes(6 * 40))
        self.assertEqual([cell(c, 7) for c in range(8, 12)], [3, 3, 3, 3])
        self.assertEqual((cell(13, 8), cell(19, 8), cell(27, 7), cell(28, 7)), (2, 2, 2, 2))
        self.assertEqual((cell(22, 8), cell(23, 8)), (4, 4))
        # Kinds leave their cell empty.
        self.assertEqual((cell(2, 8), cell(16, 8), cell(8, 6), cell(28, 6)), (0, 0, 0, 0))

    def test_spawns(self):
        spawns = read_levels(self.sections["LEVL"])[0]["spawns"]
        # Kind indexes: player 0, slime 1, coin 2, gem 3. Order: row by row.
        self.assertEqual(spawns, [
            (2, 8, 6), (2, 9, 6), (2, 10, 6), (2, 11, 6), (3, 28, 6),
            (0, 2, 8), (1, 16, 8), (2, 33, 8), (2, 34, 8), (2, 35, 8),
        ])
        # Bottom-left rule: the 16x16 hero's spawn is the cell where `P` is typed,
        # the row just above the ground, not the top-left of its two-block body.
        self.assertIn((0, 2, 8), spawns)

    def test_rule_is_the_source_text(self):
        self.assertEqual(self.sections["RULE"], (CAVE_HOP / "rules.lua").read_bytes())

    def test_deterministic(self):
        self.assertEqual(pack_game(CAVE_HOP), self.data)
        self.assertEqual(pack_game(CAVE_HOP), pack_game(CAVE_HOP))


if __name__ == "__main__":
    unittest.main()
