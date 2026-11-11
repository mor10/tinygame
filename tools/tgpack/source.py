"""Reads a game folder into a Game, collecting every problem it finds.

This is where all the rules of the text formats live (docs/formats/bundle.md,
"Text sources"). `check.py` reports the problems; `pack.py` writes the Game
out as bundle bytes. Nothing here looks inside rules.lua.
"""

import re
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

from .problem import Problem

ROOT = Path(__file__).resolve().parents[2]
PARTS_FILE = ROOT / "engine" / "parts.toml"
STARTERS_DIR = ROOT / "starters"

API_VERSION = 1
MAX_BUNDLE_BYTES = 262_144
MAX_SPRITES = 255
MAX_TILES = 255
MAX_KINDS = 64
MAX_LEVELS = 32
MAX_CELLS = 16_384
MAX_SPAWNS = 256
MAX_SPRITE_SIDE = 32
TILE_SIDE = 8
MIN_LEVEL_COLS = 17
MIN_LEVEL_ROWS = 10
MAX_NAME_CHARS = 13
MAX_STRING_BYTES = 255

ID_RE = re.compile(r"[a-z][a-z0-9_]{0,14}")
# Names of sprites, tiles and kinds (see the report: the contract gives no rule).
NAME_RE = re.compile(r"[A-Za-z0-9_]{1,31}")
COLOUR_RE = re.compile(r"#[0-9A-Fa-f]{6}")
SPRITE_CHARS = ".0123456789abcdef"

# Numbers the bundle format gives to part names (bundle.md, KIND and TILE).
MOVEMENT_CODES = {"none": 0, "platformer": 1}
BEHAVIOUR_CODES = {"none": 0, "walker": 1, "pickup": 2}
TOUCH_CODES = {"nothing": 0, "hurt": 1}
TILE_FLAG_BITS = {"solid": 1, "one_way": 2, "hazard": 4}
KIND_FLAG_PLAYER = 1

# The platform default palette: the BRIGHT candidate in
# engine/testcard/testcard.cpp. Entry 0 is unused (index 0 is see-through).
DEFAULT_PALETTE_RGB = (
    0x000000, 0x2B2D42, 0xFFFFFF, 0xC9D1E0, 0xF25C54, 0xF7934C, 0xFFD23F, 0x7BD389,
    0x3E9E6C, 0x5BC0EB, 0x3A6EA5, 0x8E6BBF, 0xF49AC2, 0xA9714B, 0xF2D0A4, 0x5C6378,
)
DEFAULT_BACKGROUND_RGB = 0xBFE6F5
DEFAULT_INK_RGB = 0x2B2D42

TOP_LEVEL_KEYS = ("api", "id", "name", "author", "starter", "view",
                  "tuning", "tile", "kind", "palette")
REQUIRED_KEYS = ("api", "id", "name", "starter")
VIEWS = {"standard": 0, "fill": 1}
KIND_FIXED_KEYS = ("sprite", "movement", "behaviour", "player", "on_touch_player")


def rgb565(rgb):
    r, g, b = (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


@dataclass
class Sprite:
    name: str
    width: int
    height: int
    pixels: list          # width * height palette indexes, row by row
    line: int = 0


@dataclass
class Tile:
    name: str
    sprite: int
    flags: int


@dataclass
class Kind:
    name: str
    sprite: int
    movement: int
    behaviour: int
    flags: int
    on_touch_player: int
    speed: float
    points: int


@dataclass
class Level:
    name: str
    cols: int
    rows: int
    cells: bytes          # cols * rows, row by row, 0 = empty, n = tile n - 1
    spawns: list          # (kind index, col, row)


@dataclass
class Game:
    id: str = ""
    name: str = ""
    author: str = ""
    starter: str = ""
    view: int = 0
    playfield_cols: int = 0
    playfield_rows: int = 0
    palette: list = field(default_factory=lambda: [rgb565(c) for c in DEFAULT_PALETTE_RGB])
    background: int = rgb565(DEFAULT_BACKGROUND_RGB)
    ink: int = rgb565(DEFAULT_INK_RGB)
    sprites: list = field(default_factory=list)
    tiles: list = field(default_factory=list)
    kinds: list = field(default_factory=list)
    tuning: list = field(default_factory=list)     # (key, value)
    levels: list = field(default_factory=list)
    rules: bytes | None = None


class TomlLines:
    """Finds the line of a table or key in a TOML file, for error messages."""

    HEADER = re.compile(r"\s*\[\s*([^\]]+?)\s*\]")
    KEY = re.compile(r"\s*([A-Za-z0-9_.-]+|\"[^\"]*\")\s*=")

    def __init__(self, text):
        self.lines = {}
        table = ""
        for number, line in enumerate(text.splitlines(), 1):
            header = self.HEADER.match(line)
            if header:
                table = self._clean(header.group(1))
                self.lines.setdefault(table, number)
                continue
            key = self.KEY.match(line)
            if key:
                path = self._clean(key.group(1))
                if table:
                    path = f"{table}.{path}"
                self.lines.setdefault(path, number)
                # `tile.ground.sprite = ...` also marks where `tile.ground` is.
                parts = path.split(".")
                for size in range(len(parts) - 1, 0, -1):
                    self.lines.setdefault(".".join(parts[:size]), number)

    @staticmethod
    def _clean(path):
        return ".".join(part.strip().strip('"') for part in path.split("."))

    def of(self, *path):
        """Line of the longest known prefix of `path`, or None."""
        for size in range(len(path), 0, -1):
            found = self.lines.get(".".join(path[:size]))
            if found:
                return found
        return None


def is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def is_printable_ascii(text):
    return all(32 <= ord(ch) <= 126 for ch in text)


def show(value):
    """A value the way it was most likely typed in game.toml."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, str):
        return f'"{value}"'
    if isinstance(value, float) and value == int(value):
        return str(int(value))
    return str(value)


def name_list(names):
    return ", ".join(sorted(names)) if names else "(none)"


def load_parts():
    """Returns (parts, error text). parts is the parsed engine/parts.toml."""
    try:
        with open(PARTS_FILE, "rb") as handle:
            return tomllib.load(handle), None
    except (OSError, tomllib.TOMLDecodeError) as error:
        return None, str(error)


class Loader:
    def __init__(self, folder, parts=None):
        self.folder = Path(folder)
        self.problems = []
        self.game = Game()
        self.parts = parts
        self.sprite_index = {}
        self.tile_index = {}
        self.kind_index = {}
        self.player_kinds = set()

    def problem(self, code, file, line, message, hint):
        self.problems.append(Problem(code, file, line, message, hint))

    # ---- files -----------------------------------------------------------

    def read_text(self, name, what):
        path = self.folder / name
        if not path.is_file():
            self.problem("missing-file", name, None,
                         f"The game has no {name} file. That is where {what} go.",
                         f"Create {name} in the game folder. `tg new` makes a game "
                         "with every file in place, to copy from.")
            return None
        try:
            return path.read_bytes().decode("utf-8")
        except UnicodeDecodeError:
            self.problem("not-text", name, None,
                         f"{name} is not plain text, so it cannot be read.",
                         "Save the file as plain text (UTF-8) from your editor.")
            return None

    def load(self):
        if not self.folder.is_dir():
            self.problem("missing-folder", ".", None,
                         f"There is no game folder at {self.folder}.",
                         "Check the spelling, or make a new game with "
                         "`tg new <id> --kind platformer`.")
            return self.game, self.problems
        if self.parts is None:
            self.parts, error = load_parts()
            if self.parts is None:
                self.problem("parts-file", "engine/parts.toml", None,
                             f"The list of game parts could not be read ({error}).",
                             "This is a problem with the TinyGame tools, not your game. "
                             "Restore engine/parts.toml.")
                return self.game, self.problems
        self.load_sprites()
        self.load_toml()
        self.load_levels()
        self.load_rules()
        return self.game, self.problems

    # ---- sprites.txt -----------------------------------------------------

    def load_sprites(self):
        text = self.read_text("sprites.txt", "the pictures")
        if text is None:
            return
        file = "sprites.txt"
        lines = text.splitlines()
        index = 0
        while index < len(lines):
            line = lines[index].rstrip()
            number = index + 1
            index += 1
            if not line.strip() or line.startswith("#"):
                continue
            if not line.startswith("=="):
                self.problem("sprite-stray-line", file, number,
                             "This line is not inside a picture.",
                             "Every picture starts with a line like `== coin 8x8`. Put this "
                             "line under one, or start it with `#` to make it a note.")
                continue
            header = re.fullmatch(r"==\s+(\S+)\s+(\d+)x(\d+)", line)
            rows = []
            while index < len(lines):
                row = lines[index].rstrip()
                if not row or row.startswith("==") or row.startswith("#"):
                    break
                rows.append((index + 1, row))
                index += 1
            if not header:
                self.problem("sprite-header", file, number,
                             f"The picture heading `{line}` is not written the right way.",
                             "Write it like `== coin 8x8`: two equals signs, the name, then "
                             "width x height.")
                continue
            name, width, height = header.group(1), int(header.group(2)), int(header.group(3))
            good = True
            if not NAME_RE.fullmatch(name):
                self.problem("sprite-name", file, number,
                             f"The picture name `{name}` has characters that are not allowed.",
                             "Use only letters, numbers and _ in a name, at most 31 of them.")
                good = False
            elif name in self.sprite_index:
                self.problem("sprite-duplicate", file, number,
                             f"There are two pictures called `{name}`.",
                             "Give one of them a different name.")
                good = False
            if width < 1 or height < 1 or width > MAX_SPRITE_SIDE or height > MAX_SPRITE_SIDE:
                self.problem("sprite-too-big", file, number,
                             f"The picture `{name}` is {width}x{height}. A picture can be at "
                             f"most {MAX_SPRITE_SIDE}x{MAX_SPRITE_SIDE} (and at least 1x1).",
                             f"Make it {MAX_SPRITE_SIDE}x{MAX_SPRITE_SIDE} or smaller.")
                continue
            if len(rows) != height:
                self.problem("sprite-rows", file, number,
                             f"The picture `{name}` says it is {height} rows tall, but it has "
                             f"{len(rows)} rows.",
                             f"Add or remove rows so there are exactly {height}, or change the "
                             f"size in the heading to {width}x{len(rows)}.")
                good = False
            pixels = []
            for row_number, row in rows:
                if len(row) != width:
                    self.problem("sprite-row-width", file, row_number,
                                 f"This row of `{name}` is {len(row)} characters wide, but the "
                                 f"picture is {width} wide.",
                                 f"Make the row exactly {width} characters. Use `.` for "
                                 "see-through pixels.")
                    good = False
                bad = sorted({ch for ch in row if ch not in SPRITE_CHARS})
                if bad:
                    shown = " ".join(f"`{ch}`" for ch in bad)
                    self.problem("sprite-char", file, row_number,
                                 f"This row of `{name}` uses {shown}, which is not a colour.",
                                 "Use `.` for see-through, `1` to `9` and small letters `a` to "
                                 "`f` for the 15 colours.")
                    good = False
                if good:
                    pixels.extend(0 if ch == "." else int(ch, 16) for ch in row)
            if good:
                self.sprite_index[name] = len(self.game.sprites)
                self.game.sprites.append(Sprite(name, width, height, pixels, number))
        if len(self.game.sprites) > MAX_SPRITES:
            self.problem("too-many-sprites", file, self.game.sprites[MAX_SPRITES].line,
                         f"There are {len(self.game.sprites)} pictures. A game can have at "
                         f"most {MAX_SPRITES}.",
                         "Remove pictures the game does not use.")

    # ---- game.toml -------------------------------------------------------

    def load_toml(self):
        file = "game.toml"
        text = self.read_text(file, "the game's settings")
        if text is None:
            return
        try:
            data = tomllib.loads(text)
        except tomllib.TOMLDecodeError as error:
            where = re.search(r"\(at line (\d+), column \d+\)", str(error))
            detail = re.sub(r"\s*\(at line \d+, column \d+\)", "", str(error))
            self.problem("toml-syntax", file, int(where.group(1)) if where else None,
                         f"game.toml has a typing mistake the computer cannot read ({detail}).",
                         'Look at this line. Text needs "quotes" on both sides, each setting '
                         "is `name = value`, and a name can only be set once.")
            return
        self.lines = TomlLines(text)
        line = self.lines.of

        for key in data:
            if key not in TOP_LEVEL_KEYS:
                self.problem("unknown-key", file, line(key),
                             f"game.toml has a setting called `{key}` that TinyGame does not "
                             "know.",
                             "Check the spelling. The settings are: "
                             + ", ".join(TOP_LEVEL_KEYS) + ".")
        for key in REQUIRED_KEYS:
            if key not in data:
                example = {"api": "api = 1", "id": f'id = "{self.folder.resolve().name}"',
                           "name": 'name = "MY GAME"', "starter": 'starter = "platformer"'}[key]
                self.problem("missing-key", file, None,
                             f"game.toml is missing the `{key}` setting.",
                             f"Add a line near the top like: {example}")

        game = self.game
        if "api" in data and (data["api"] != API_VERSION or isinstance(data["api"], bool)
                              or not isinstance(data["api"], int)):
            self.problem("bad-api", file, line("api"),
                         f"`api` is {show(data['api'])}, but this TinyGame only knows api "
                         f"{API_VERSION}.",
                         f"Change the line to: api = {API_VERSION}")

        if "id" in data:
            folder_name = self.folder.resolve().name
            value = data["id"]
            if not isinstance(value, str) or not ID_RE.fullmatch(value):
                self.problem("bad-id", file, line("id"),
                             f"The id {show(value)} is not allowed.",
                             "An id starts with a small letter and then has only small "
                             "letters, numbers and _, at most 15 characters. Example: "
                             'id = "cave_hop"')
            else:
                game.id = value
                if value != folder_name:
                    self.problem("id-folder", file, line("id"),
                                 f'The id is "{value}" but the game\'s folder is called '
                                 f'"{folder_name}". They must be the same.',
                                 f'Change the line to: id = "{folder_name}" (or rename the '
                                 "folder).")

        if "name" in data:
            value = data["name"]
            if (not isinstance(value, str) or not 1 <= len(value) <= MAX_NAME_CHARS
                    or not is_printable_ascii(value)):
                self.problem("bad-name", file, line("name"),
                             f"The name {show(value)} does not fit on the console.",
                             f"Use 1 to {MAX_NAME_CHARS} plain characters (letters, numbers, "
                             "spaces, simple signs). No emoji or accents.")
            else:
                game.name = value

        for key in ("author", "starter"):
            if key not in data:
                continue
            value = data[key]
            if (not isinstance(value, str) or len(value) > MAX_STRING_BYTES
                    or not is_printable_ascii(value)):
                self.problem(f"bad-{key}", file, line(key),
                             f"`{key}` must be plain text in quotes, at most "
                             f"{MAX_STRING_BYTES} characters, with no emoji or accents.",
                             f'Write it like: {key} = "{"Sam" if key == "author" else "platformer"}"')
            else:
                setattr(game, key, value)
        if game.starter and STARTERS_DIR.is_dir():
            known = sorted(p.name for p in STARTERS_DIR.iterdir() if p.is_dir())
            if known and game.starter not in known:
                self.problem("unknown-starter", file, line("starter"),
                             f'There is no starter called "{game.starter}".',
                             "Use one of: " + ", ".join(known) + ".")

        if "view" in data:
            if not isinstance(data["view"], str) or data["view"] not in VIEWS:
                self.problem("bad-view", file, line("view"),
                             f"`view` is {show(data['view'])}, but it can only be "
                             '"standard" or "fill".',
                             'Change the line to: view = "standard"')
            else:
                game.view = VIEWS[data["view"]]

        self.load_tuning(data.get("tuning", {}))
        self.load_palette(data.get("palette"))
        self.load_tiles(data.get("tile", {}))
        self.load_kinds(data.get("kind", {}))

        both = sorted(set(self.tile_index) & set(self.kind_index))
        for name in both:
            self.problem("tile-and-kind", file, line("kind", name),
                         f"`{name}` is the name of a tile and also of a kind, so a level "
                         "cannot tell which one you mean.",
                         "Rename one of them.")

    def table(self, value, path, what):
        """Checks that a TOML value is a table; returns it or None."""
        if isinstance(value, dict):
            return value
        self.problem("not-a-table", "game.toml", self.lines.of(*path),
                     f"`{'.'.join(path)}` should be a group of settings, not a single value.",
                     f"Write it as a heading in square brackets, like {what}, with its "
                     "settings on the lines below.")
        return None

    def check_range(self, value, spec, path, label):
        """Checks a number against a parts.toml param. Returns True when it is fine."""
        file, line = "game.toml", self.lines.of(*path)
        unit = f" ({spec['unit']})" if spec.get("unit") else ""
        wants_integer = spec.get("type") == "integer"
        if not is_number(value) or (wants_integer and not isinstance(value, int)):
            kind = "a whole number" if wants_integer else "a number"
            self.problem("not-a-number", file, line,
                         f"`{label}` is {show(value)}, but it must be {kind}{unit}.",
                         f"Write {kind} with no quotes, like: {path[-1]} = "
                         f"{show(spec.get('default', 1))}")
            return False
        low, high = spec.get("min"), spec.get("max")
        if (low is not None and value < low) or (high is not None and value > high):
            self.problem("out-of-range", file, line,
                         f"`{label}` is {show(value)}, but it has to be between {show(low)} "
                         f"and {show(high)}{unit}.",
                         f"Pick a number from {show(low)} to {show(high)}. The usual value is "
                         f"{show(spec.get('default'))}.")
            return False
        return True

    def load_tuning(self, tuning):
        tuning = self.table(tuning, ("tuning",), "[tuning]")
        if tuning is None:
            return
        specs = {name: part.get("params", {}).get("value", {})
                 for name, part in self.parts.get("tuning", {}).items()}
        for key in tuning:
            if key not in specs:
                self.problem("unknown-tuning", "game.toml", self.lines.of("tuning", key),
                             f"`{key}` is not a tuning setting TinyGame knows.",
                             "Check the spelling. The tuning settings are: "
                             + name_list(specs) + ".")
        for key, spec in specs.items():
            value = spec.get("default", 0)
            if key in tuning and self.check_range(tuning[key], spec, ("tuning", key), key):
                value = tuning[key]
            self.game.tuning.append((key, float(value)))

    def load_palette(self, palette):
        if palette is None:
            return
        palette = self.table(palette, ("palette",), "[palette]")
        if palette is None:
            return
        allowed = [str(n) for n in range(1, 16)] + ["background", "ink"]
        for key, value in palette.items():
            line = self.lines.of("palette", key)
            if key not in allowed:
                self.problem("unknown-palette-key", "game.toml", line,
                             f"`{key}` is not something a palette can set.",
                             "A palette can set the colours 1 to 15, `background` and `ink`.")
                continue
            if not isinstance(value, str) or not COLOUR_RE.fullmatch(value):
                self.problem("bad-colour", "game.toml", line,
                             f"The colour {show(value)} is not written the right way.",
                             'Write a colour as # and six characters from 0-9 and A-F, in '
                             'quotes, like "#F25C54".')
                continue
            colour = rgb565(int(value[1:], 16))
            if key == "background":
                self.game.background = colour
            elif key == "ink":
                self.game.ink = colour
            else:
                self.game.palette[int(key)] = colour

    def sprite_for(self, entry, path, what):
        """Looks up the sprite an entry names. Returns its index or None."""
        file, name = "game.toml", path[-1]
        if "sprite" not in entry:
            self.problem("missing-sprite-key", file, self.lines.of(*path),
                         f"The {what} `{name}` does not say which picture to use.",
                         f'Add a line under [{".".join(path)}] like: sprite = "{name}"')
            return None
        sprite = entry["sprite"]
        if not isinstance(sprite, str) or sprite not in self.sprite_index:
            self.problem("unknown-sprite", file, self.lines.of(*path, "sprite"),
                         f"The {what} `{name}` uses a picture called {show(sprite)}, but "
                         "sprites.txt has no picture with that name.",
                         "Check the spelling, or draw it in sprites.txt. The pictures are: "
                         + name_list(self.sprite_index) + ".")
            return None
        return self.sprite_index[sprite]

    def check_entry_name(self, name, path, what):
        if NAME_RE.fullmatch(name):
            return True
        self.problem("bad-entry-name", "game.toml", self.lines.of(*path),
                     f"The {what} name `{name}` has characters that are not allowed.",
                     "Use only letters, numbers and _ in a name, at most 31 of them.")
        return False

    def load_tiles(self, tiles):
        tiles = self.table(tiles, ("tile",), "[tile.ground]")
        if tiles is None:
            return
        file = "game.toml"
        properties = self.parts.get("tile_property", {})
        for name, entry in tiles.items():
            path = ("tile", name)
            entry = self.table(entry, path, f"[tile.{name}]")
            if entry is None or not self.check_entry_name(name, path, "tile"):
                continue
            flags = 0
            for key, value in entry.items():
                if key == "sprite":
                    continue
                if key not in properties:
                    self.problem("unknown-tile-key", file, self.lines.of(*path, key),
                                 f"The tile `{name}` has a setting called `{key}` that "
                                 "TinyGame does not know.",
                                 "Check the spelling. A tile can have: sprite, "
                                 + name_list(properties) + ".")
                elif not isinstance(value, bool):
                    self.problem("not-true-false", file, self.lines.of(*path, key),
                                 f"`{key}` on the tile `{name}` is {show(value)}, but it can "
                                 "only be true or false.",
                                 f"Write: {key} = true")
                elif value:
                    flags |= TILE_FLAG_BITS.get(key, 0)
            sprite = self.sprite_for(entry, path, "tile")
            if sprite is not None:
                art = self.game.sprites[sprite]
                if (art.width, art.height) != (TILE_SIDE, TILE_SIDE):
                    self.problem("tile-sprite-size", file, self.lines.of(*path, "sprite"),
                                 f"The tile `{name}` uses the picture `{art.name}`, which is "
                                 f"{art.width}x{art.height}. A tile picture must be exactly "
                                 f"{TILE_SIDE}x{TILE_SIDE}.",
                                 f"Redraw `{art.name}` as {TILE_SIDE}x{TILE_SIDE} in "
                                 "sprites.txt, or pick a different picture.")
            self.tile_index[name] = len(self.game.tiles)
            self.game.tiles.append(Tile(name, sprite or 0, flags))
            if len(self.game.tiles) == MAX_TILES + 1:
                self.problem("too-many-tiles", file, self.lines.of(*path),
                             f"There are more than {MAX_TILES} tiles. That is the most a "
                             "game can have.",
                             "Remove tiles the levels do not use.")

    def part_choice(self, entry, key, group, codes, path, default):
        """Reads a part name (movement, behaviour, ...). Returns the name."""
        if key not in entry:
            return default
        name = path[-1]
        value = entry[key]
        known = self.parts.get(group, {})
        line = self.lines.of(*path, key)
        if not isinstance(value, str) or value not in known:
            self.problem("unknown-part", "game.toml", line,
                         f"`{key}` on the kind `{name}` is {show(value)}, which is not one of "
                         "the choices.",
                         f"Use one of: {name_list(known)}.")
            return default
        if known[value].get("since", 1) > API_VERSION or value not in codes:
            self.problem("part-too-new", "game.toml", line,
                         f'`{key} = "{value}"` needs a newer TinyGame than this one.',
                         f"Use one of: {name_list(set(known) & set(codes))}.")
            return default
        return value

    def load_kinds(self, kinds):
        kinds = self.table(kinds, ("kind",), "[kind.player]")
        if kinds is None:
            return
        file = "game.toml"
        # Which numeric settings each movement or behaviour brings (speed, points...).
        param_specs = {}
        owners = {}
        for group in ("movement", "behaviour", "touch_player"):
            for part_name, part in self.parts.get(group, {}).items():
                for param, spec in part.get("params", {}).items():
                    param_specs[param] = spec
                    owners.setdefault(param, set()).add((group, part_name))
        allowed = list(KIND_FIXED_KEYS) + sorted(param_specs)
        for name, entry in kinds.items():
            path = ("kind", name)
            entry = self.table(entry, path, f"[kind.{name}]")
            if entry is None or not self.check_entry_name(name, path, "kind"):
                continue
            for key in entry:
                if key not in allowed:
                    self.problem("unknown-kind-key", file, self.lines.of(*path, key),
                                 f"The kind `{name}` has a setting called `{key}` that "
                                 "TinyGame does not know.",
                                 "Check the spelling. A kind can have: "
                                 + ", ".join(allowed) + ".")
            chosen = {
                "movement": self.part_choice(entry, "movement", "movement",
                                             MOVEMENT_CODES, path, "none"),
                "behaviour": self.part_choice(entry, "behaviour", "behaviour",
                                              BEHAVIOUR_CODES, path, "none"),
                "touch_player": self.part_choice(entry, "on_touch_player", "touch_player",
                                                 TOUCH_CODES, path, "nothing"),
            }
            flags = 0
            if "player" in entry:
                if not isinstance(entry["player"], bool):
                    self.problem("not-true-false", file, self.lines.of(*path, "player"),
                                 f"`player` on the kind `{name}` is {show(entry['player'])}, "
                                 "but it can only be true or false.",
                                 "Write: player = true")
                elif entry["player"]:
                    flags |= KIND_FLAG_PLAYER
            values = {}
            for param, spec in param_specs.items():
                uses_it = any(chosen[group] == part for group, part in owners[param])
                value = spec.get("default", 0) if uses_it else 0
                if param in entry and self.check_range(entry[param], spec, (*path, param),
                                                       f"{param}` on the kind `{name}"):
                    value = entry[param]
                values[param] = value
            sprite = self.sprite_for(entry, path, "kind")
            self.kind_index[name] = len(self.game.kinds)
            if flags & KIND_FLAG_PLAYER:
                self.player_kinds.add(name)
            self.game.kinds.append(Kind(
                name, sprite or 0, MOVEMENT_CODES[chosen["movement"]],
                BEHAVIOUR_CODES[chosen["behaviour"]], flags,
                TOUCH_CODES[chosen["touch_player"]],
                float(values.get("speed", 0)), int(values.get("points", 0))))
            if len(self.game.kinds) == MAX_KINDS + 1:
                self.problem("too-many-kinds", file, self.lines.of(*path),
                             f"There are more than {MAX_KINDS} kinds. That is the most a "
                             "game can have.",
                             "Remove kinds the levels do not use.")
        if not self.player_kinds:
            self.problem("no-player-kind", file, self.lines.of("kind"),
                         "No kind is the player, so nobody could play this game.",
                         "Add `player = true` to the kind you control, for example under "
                         "[kind.player].")

    # ---- levels.txt ------------------------------------------------------

    LEGEND_ITEM = re.compile(r"\s*(\S)\s*=\s*([^\s,=]+)\s*(?:,|$)")

    def load_levels(self):
        file = "levels.txt"
        text = self.read_text(file, "the levels")
        if text is None:
            return
        blocks = []
        current = None
        for number, raw in enumerate(text.splitlines(), 1):
            line = raw.rstrip()
            header = re.fullmatch(r"==\s+(.*\S)", line)
            if header:
                current = {"name": header.group(1), "line": number, "legend": [], "rows": []}
                blocks.append(current)
            elif not line.strip():
                continue
            elif current is None:
                if not line.startswith("#"):
                    self.problem("level-stray-line", file, number,
                                 "This line comes before the first level starts.",
                                 "Start a level with a line like `== level 1`.")
            elif line.startswith("legend:"):
                if current["rows"]:
                    self.problem("legend-after-rows", file, number,
                                 "This legend line comes after the level's rows have started.",
                                 "Move every `legend:` line up, straight under the `==` line.")
                current["legend"].append((number, line[len("legend:"):]))
            else:
                current["rows"].append((number, line))
        if not blocks:
            self.problem("no-levels", file, None,
                         "levels.txt has no levels in it.",
                         "Start a level with a line like `== level 1`, then a `legend:` "
                         "line, then the rows.")
            return
        # Without a readable game.toml the legend names cannot be judged.
        self.names_known = hasattr(self, "lines")
        for block in blocks:
            if len(self.game.levels) == MAX_LEVELS:
                self.problem("too-many-levels", file, block["line"],
                             f"There are more than {MAX_LEVELS} levels. That is the most a "
                             "game can have.",
                             "Remove a level, or join two short ones.")
                break
            self.load_level(block)

    def load_level(self, block):
        file = "levels.txt"
        name, head = block["name"], block["line"]
        good = True
        if len(name) > MAX_STRING_BYTES or not is_printable_ascii(name):
            self.problem("level-name", file, head,
                         "The level name has characters the console cannot show, or is "
                         "too long.",
                         "Use plain letters, numbers and spaces, like `== cave 1`.")
            good = False
        legend = {}
        for number, text in block["legend"]:
            position = 0
            while position < len(text) and text[position:].strip():
                item = self.LEGEND_ITEM.match(text, position)
                if not item:
                    self.problem("legend-syntax", file, number,
                                 "This legend line is not written the right way.",
                                 "Write it like `legend: # = ground, P = player`: one "
                                 "character, an equals sign, a name, with commas between.")
                    good = False
                    break
                position = item.end()
                char, target = item.group(1), item.group(2)
                if char == ".":
                    self.problem("legend-dot", file, number,
                                 "The legend gives `.` a meaning, but `.` always means "
                                 "empty.",
                                 "Pick a different character for "
                                 f"`{target}`.")
                    good = False
                elif char in legend:
                    self.problem("legend-duplicate", file, number,
                                 f"The legend says what `{char}` means twice.",
                                 "Each character can mean only one thing. Pick a different "
                                 f"character for `{target}`.")
                    good = False
                elif not self.names_known:
                    legend[char] = None
                elif target in self.tile_index:
                    legend[char] = ("tile", self.tile_index[target])
                elif target in self.kind_index:
                    legend[char] = ("kind", self.kind_index[target])
                else:
                    self.problem("legend-unknown-name", file, number,
                                 f"The legend says `{char}` means `{target}`, but game.toml "
                                 "has no tile or kind with that name.",
                                 "Check the spelling. Tiles: " + name_list(self.tile_index)
                                 + ". Kinds: " + name_list(self.kind_index) + ".")
                    legend[char] = None
                    good = False
        rows = block["rows"]
        if not rows:
            self.problem("level-empty", file, head,
                         f"The level `{name}` has no rows.",
                         f"Draw the level under its legend: at least {MIN_LEVEL_ROWS} rows "
                         f"of at least {MIN_LEVEL_COLS} characters.")
            return
        cols = len(rows[0][1])
        for number, row in rows[1:]:
            if len(row) != cols:
                self.problem("level-row-width", file, number,
                             f"This row is {len(row)} characters wide, but the first row of "
                             f"the level is {cols} wide. Every row must be the same width.",
                             f"Make this row exactly {cols} characters. Use `.` for empty "
                             "space.")
                good = False
        if cols < MIN_LEVEL_COLS:
            self.problem("level-too-narrow", file, rows[0][0],
                         f"The level `{name}` is {cols} columns wide. A level must be at "
                         f"least {MIN_LEVEL_COLS} wide to fill the screen.",
                         f"Make every row at least {MIN_LEVEL_COLS} characters. Use `.` for "
                         "empty space.")
            good = False
        if len(rows) < MIN_LEVEL_ROWS:
            self.problem("level-too-short", file, head,
                         f"The level `{name}` has {len(rows)} rows. A level must have at "
                         f"least {MIN_LEVEL_ROWS} rows to fill the screen.",
                         f"Add rows of dots at the top until there are {MIN_LEVEL_ROWS}.")
            good = False
        if cols * len(rows) > MAX_CELLS or cols > 0xFFFF or len(rows) > 0xFFFF:
            self.problem("level-too-big", file, head,
                         f"The level `{name}` has {cols * len(rows)} cells. A level can "
                         f"have at most {MAX_CELLS} (columns times rows).",
                         "Make the level shorter or less wide.")
            good = False
        cells = bytearray()
        spawns = []
        players = []
        reported = set()
        for row_index, (number, row) in enumerate(rows):
            for col, char in enumerate(row):
                meaning = None if char == "." else legend.get(char, "missing")
                if meaning == "missing":
                    if char not in reported:
                        reported.add(char)
                        shown = "a space" if char == " " else "a tab" if char == "\t" \
                            else f"`{char}`"
                        self.problem("level-unknown-char", file, number,
                                     f"The level uses {shown} (column {col + 1}), but the "
                                     "legend does not say what it means.",
                                     "Add it to a `legend:` line, or change it to `.` for "
                                     "empty.")
                    good = False
                    meaning = None
                if meaning and meaning[0] == "tile":
                    cells.append(min(meaning[1] + 1, 255))
                    continue
                cells.append(0)
                if meaning and meaning[0] == "kind":
                    spawns.append((meaning[1], col, row_index))
                    if self.game.kinds[meaning[1]].flags & KIND_FLAG_PLAYER:
                        players.append(number)
        if len(spawns) > MAX_SPAWNS:
            self.problem("too-many-spawns", file, head,
                         f"The level `{name}` places {len(spawns)} things. A level can "
                         f"place at most {MAX_SPAWNS}.",
                         "Remove some, or split the level in two.")
            good = False
        if self.names_known and self.player_kinds:
            who = name_list(self.player_kinds)
            if not players:
                self.problem("no-player-spawn", file, head,
                             f"The level `{name}` has no player in it.",
                             f"Put the player ({who}) in the level: add its character to "
                             "the legend and type it once where the player starts.")
                good = False
            elif len(players) > 1:
                self.problem("many-player-spawns", file, players[1],
                             f"The level `{name}` has {len(players)} players. There must "
                             "be exactly one.",
                             "Keep one player character and change the others to `.`.")
                good = False
        if good:
            self.game.levels.append(Level(name, cols, len(rows), bytes(cells), spawns))

    # ---- rules.lua -------------------------------------------------------

    def load_rules(self):
        path = self.folder / "rules.lua"
        if not path.is_file():
            return
        data = path.read_bytes()
        if data.startswith(b"\x1bLua"):
            self.problem("rules-bytecode", "rules.lua", None,
                         "rules.lua is compiled Lua, not Lua you can read.",
                         "Put the Lua text itself in rules.lua.")
            return
        self.game.rules = data


def load_game(folder, parts=None):
    """Returns (Game, [Problem]). The Game is only complete when there are no problems."""
    return Loader(folder, parts).load()
