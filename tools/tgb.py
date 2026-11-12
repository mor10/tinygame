#!/usr/bin/env python3
"""Deterministic, bounded TinyGame text-game bundle helpers."""

import math
import os
from pathlib import Path, PurePosixPath
import re
import struct
import tempfile
import zlib

try:
    import tomllib
except ModuleNotFoundError:
    tomllib = None

MAGIC = b"TGB\x00"
FORMAT_VERSION = 1
HEADER = struct.Struct("<4sHHHHIIIIII4s")
ENTRY = struct.Struct("<HHIII")
HEADER_SIZE = HEADER.size
MAX_FILES = 256
MAX_PATH_BYTES = 128
MAX_FILE_BYTES = 1 << 20
MAX_LUA_SOURCE_BYTES = 64 << 10
MAX_MANIFEST_BYTES = 32 << 10
MAX_BUNDLE_BYTES = 4 << 20
REQUIRED_FILES = {"game.toml", "sprites.txt", "tiles.txt", "levels.txt", "README.md"}


class BundleError(ValueError):
    """A game source tree or packed bundle violates the candidate contract."""


def _validate_numbers(value, location="game.toml"):
    if isinstance(value, bool):
        return
    if isinstance(value, (int, float)):
        try:
            finite = math.isfinite(value)
        except OverflowError:
            finite = False
        if not finite:
            raise BundleError(f"{location}: numeric value must be finite and supported.")
    elif isinstance(value, dict):
        for key, child in value.items():
            _validate_numbers(child, f"{location}.{key}")
    elif isinstance(value, list):
        for index, child in enumerate(value):
            _validate_numbers(child, f"{location}[{index}]")


def _align4(value):
    return (value + 3) & ~3


def _member_name(name):
    if not isinstance(name, str) or not name or len(name.encode("utf-8")) > MAX_PATH_BYTES:
        raise BundleError("Bundle path must be a non-empty path of at most 128 bytes.")
    if (not name.isascii() or "\\" in name or name.startswith("/")
            or any(ord(char) < 32 or ord(char) == 127 for char in name)):
        raise BundleError(f"Invalid bundle path: {name!r}.")
    path = PurePosixPath(name)
    if str(path) != name or any(part in ("", ".", "..") for part in name.split("/")):
        raise BundleError(f"Invalid bundle path: {name!r}.")
    parts = path.parts
    valid = (
        name in REQUIRED_FILES
        or name == "rules.lua"
        or (len(parts) == 2 and parts[0] == "modules"
            and re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,115}\.lua", parts[1]) is not None)
        or (len(parts) == 2 and parts[0] == "test" and len(parts[1]) > 6
            and parts[1].endswith(".steps"))
    )
    if not valid:
        raise BundleError(f"Unsupported game file: {name!r}.")
    return name


def _text_bytes(name, value):
    if not isinstance(value, bytes) or not value:
        raise BundleError(f"{name}: file must contain non-empty text.")
    if len(value) > MAX_FILE_BYTES:
        raise BundleError(f"{name}: file exceeds the 1 MiB candidate limit.")
    if name.endswith(".lua") and len(value) > MAX_LUA_SOURCE_BYTES:
        raise BundleError(f"{name}: Lua source exceeds the 64 KiB candidate limit.")
    if name.endswith(".lua") and value.startswith(b"\x1bLua"):
        raise BundleError(f"{name}: Lua bytecode is not accepted.")
    try:
        text = value.decode("utf-8", errors="strict")
    except UnicodeDecodeError as error:
        raise BundleError(f"{name}: file must be valid UTF-8 text.") from error
    if "\x00" in text or "\r" in text:
        raise BundleError(f"{name}: NUL and CR characters are not allowed; use UTF-8 with LF.")
    if name == "game.toml" and len(value) > MAX_MANIFEST_BYTES:
        raise BundleError("game.toml exceeds the 32 KiB candidate limit.")
    return value


def parse_manifest(files, expected_id=None):
    if tomllib is None:
        raise BundleError("Python 3.11+ is required for TOML validation.")
    try:
        manifest = tomllib.loads(files["game.toml"].decode("utf-8"))
    except (UnicodeDecodeError, tomllib.TOMLDecodeError) as error:
        raise BundleError(f"game.toml: invalid TOML: {error}") from error
    _validate_numbers(manifest)
    game_id = manifest.get("id")
    if not isinstance(game_id, str) or not _valid_id(game_id):
        raise BundleError("game.toml: id must match [a-z][a-z0-9_]{0,14}.")
    if expected_id is not None and game_id != expected_id:
        raise BundleError(f"game.toml: id {game_id!r} does not match folder {expected_id!r}.")
    if type(manifest.get("api")) is not int or manifest["api"] != 1:
        raise BundleError("game.toml: api must be the integer 1.")
    name = manifest.get("name")
    if (not isinstance(name, str) or not 1 <= len(name) <= 13
            or any(ord(char) < 32 or ord(char) > 126 for char in name)):
        raise BundleError("game.toml: name must be 1-13 printable ASCII characters.")
    if not isinstance(manifest.get("starter"), str) or not manifest["starter"]:
        raise BundleError("game.toml: starter must be a non-empty string.")
    if manifest.get("view") not in ("scroll", "fixed"):
        raise BundleError("game.toml: view must be 'scroll' or 'fixed'.")
    tuning = manifest.get("tuning", {})
    if not isinstance(tuning, dict):
        raise BundleError("game.toml: tuning must be a table.")
    for key, value in tuning.items():
        if (not isinstance(key, str) or isinstance(value, bool)
                or not isinstance(value, (int, float))):
            raise BundleError(f"game.toml: tuning.{key} must be a finite number.")
    kinds = manifest.get("kinds", {})
    if not isinstance(kinds, dict):
        raise BundleError("game.toml: kinds must be a table.")
    return manifest


def _valid_id(value):
    return (isinstance(value, str) and 1 <= len(value) <= 15
            and value[0].islower() and value[0].isascii() and value[0].isalpha()
            and all(char.isascii() and (char.islower() or char.isdigit() or char == "_")
                    for char in value))


def validate_files(files, expected_id=None):
    if not isinstance(files, dict) or not 1 <= len(files) <= MAX_FILES:
        raise BundleError(f"Game must contain 1-{MAX_FILES} files.")
    clean = {}
    total_bytes = 0
    for name, value in files.items():
        _member_name(name)
        clean[name] = _text_bytes(name, value)
        total_bytes += len(clean[name])
        if total_bytes > MAX_BUNDLE_BYTES:
            raise BundleError("Game text exceeds the 4 MiB candidate bundle limit.")
    missing = REQUIRED_FILES - clean.keys()
    if missing:
        raise BundleError("Missing required files: " + ", ".join(sorted(missing)) + ".")
    parse_manifest(clean, expected_id)
    return clean


def read_game(directory):
    root = Path(directory)
    if root.is_symlink() or not root.is_dir():
        raise BundleError(f"Game path is not a regular directory: {root}.")
    files = {}
    total_bytes = 0
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            raise BundleError(f"Symlinks are not allowed in game content: {relative}.")
        if path.is_dir():
            continue
        if not path.is_file():
            raise BundleError(f"Unsupported filesystem entry: {relative}.")
        name = _member_name(relative)
        with path.open("rb") as stream:
            content = stream.read(MAX_FILE_BYTES + 1)
        if len(content) > MAX_FILE_BYTES:
            raise BundleError(f"{name}: file exceeds the 1 MiB candidate limit.")
        total_bytes += len(content)
        if total_bytes > MAX_BUNDLE_BYTES:
            raise BundleError("Game text exceeds the 4 MiB candidate bundle limit.")
        files[name] = content
    expected_id = root.name
    if not _valid_id(expected_id):
        raise BundleError(f"Invalid game folder id: {expected_id!r}.")
    return validate_files(files, expected_id)


def build_bundle(files):
    clean = validate_files(files)
    names = sorted(clean)
    table_length = sum(ENTRY.size + _align4(len(name.encode("ascii"))) for name in names)
    payload_offset = _align4(HEADER_SIZE + table_length)
    payload = bytearray()
    final_entries = []
    cursor = payload_offset
    for name in names:
        absolute_offset = _align4(cursor)
        payload.extend(bytes(absolute_offset - cursor))
        content = clean[name]
        final_entries.append((name, absolute_offset, content))
        payload.extend(content)
        cursor = absolute_offset + len(content)
    table = bytearray()
    for name, offset, content in final_entries:
        encoded = name.encode("ascii")
        table.extend(ENTRY.pack(len(encoded), 0, offset, len(content),
                                zlib.crc32(content) & 0xFFFFFFFF))
        table.extend(encoded)
        table.extend(bytes((-len(encoded)) % 4))

    if len(table) != table_length:
        raise BundleError("Internal bundle table-length error.")
    total_length = payload_offset + len(payload)
    if total_length > MAX_BUNDLE_BYTES:
        raise BundleError("Bundle exceeds the 4 MiB candidate limit.")
    header = HEADER.pack(
        MAGIC, FORMAT_VERSION, HEADER_SIZE, len(final_entries), 0,
        HEADER_SIZE, len(table), payload_offset, len(payload), total_length,
        zlib.crc32(table) & 0xFFFFFFFF, bytes(4),
    )
    return header + table + bytes(payload_offset - HEADER_SIZE - len(table)) + payload


def parse_bundle(data):
    if not isinstance(data, bytes) or len(data) < HEADER_SIZE:
        raise BundleError("Bundle is truncated before its header.")
    if len(data) > MAX_BUNDLE_BYTES:
        raise BundleError("Bundle exceeds the 4 MiB candidate limit.")
    (magic, version, header_size, count, flags, table_offset, table_length,
     payload_offset, payload_length, total_length, table_crc, reserved) = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise BundleError("Invalid bundle magic.")
    if version != FORMAT_VERSION:
        raise BundleError(f"Unsupported bundle version: {version}.")
    if header_size != HEADER_SIZE or flags != 0 or reserved != bytes(4):
        raise BundleError("Invalid header size, flags, or reserved bytes.")
    if not 1 <= count <= MAX_FILES:
        raise BundleError("Invalid bundle file count.")
    if table_offset != HEADER_SIZE:
        raise BundleError("Invalid bundle table offset.")
    table_end = table_offset + table_length
    if (table_end > len(data) or payload_offset != _align4(table_end)
            or payload_offset + payload_length != total_length or total_length != len(data)):
        raise BundleError("Invalid bundle section lengths or offsets.")
    if any(data[table_end:payload_offset]):
        raise BundleError("Non-zero padding after the file table.")
    table = data[table_offset:table_end]
    if zlib.crc32(table) & 0xFFFFFFFF != table_crc:
        raise BundleError("File table checksum mismatch.")

    entries = []
    position = 0
    for _ in range(count):
        if position + ENTRY.size > len(table):
            raise BundleError("Truncated file table entry.")
        name_length, entry_flags, offset, length, checksum = ENTRY.unpack_from(table, position)
        position += ENTRY.size
        padded_end = position + _align4(name_length)
        if (name_length == 0 or name_length > MAX_PATH_BYTES or entry_flags != 0
                or padded_end > len(table)):
            raise BundleError("Invalid file table entry.")
        try:
            name = table[position:position + name_length].decode("ascii", errors="strict")
        except UnicodeDecodeError as error:
            raise BundleError("Bundle paths must be ASCII.") from error
        _member_name(name)
        if any(table[position + name_length:padded_end]):
            raise BundleError("Non-zero file table padding.")
        position = padded_end
        if not payload_offset <= offset < total_length or not 0 < length <= MAX_FILE_BYTES:
            raise BundleError(f"Invalid file range for {name}.")
        if offset % 4 or offset + length > total_length:
            raise BundleError(f"Misaligned or out-of-range file data for {name}.")
        entries.append((name, offset, length, checksum))
    if position != len(table):
        raise BundleError("Unexpected trailing bytes in the file table.")
    names = [entry[0] for entry in entries]
    if names != sorted(set(names)):
        raise BundleError("File table paths must be unique and sorted.")

    files = {}
    cursor = payload_offset
    for name, offset, length, checksum in entries:
        if offset < cursor:
            raise BundleError(f"Overlapping file data for {name}.")
        if any(data[cursor:offset]):
            raise BundleError("Non-zero payload padding.")
        content = data[offset:offset + length]
        if zlib.crc32(content) & 0xFFFFFFFF != checksum:
            raise BundleError(f"File checksum mismatch for {name}.")
        files[name] = _text_bytes(name, content)
        cursor = offset + length
    if cursor != total_length:
        raise BundleError("Unexpected trailing bytes after file data.")
    return validate_files(files)


def write_bundle(path, data):
    parse_bundle(data)
    target = Path(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=target.parent, prefix=target.name + ".",
                                         delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, target)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()
