#!/usr/bin/env python3
"""TinyGame v2 helper.

  python3 tools/tg.py test                 build and run the engine host tests
  python3 tools/tg.py testcard [--out DIR] render every test-card variant to PNG

More commands arrive with later milestones (see docs/spec.md, section 12).
"""

import argparse
import hashlib
import os
import struct
import subprocess
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "engine"
BUILD = ROOT / ".build"
CXXFLAGS = ["-std=c++17", "-O1", "-Wall", "-Wextra", "-Werror"]
# Engine folders that are programs or tests, not part of the engine library.
NOT_LIBRARY = {"test", "host"}


def compiler():
    return os.environ.get("CXX", "c++")


def engine_sources():
    return sorted(p for p in ENGINE.rglob("*.cpp")
                  if not NOT_LIBRARY.intersection(p.relative_to(ENGINE).parts))


def header_digest():
    """One digest of every engine header: any header edit rebuilds everything."""
    digest = hashlib.sha256()
    for header in sorted(ENGINE.rglob("*.h")):
        digest.update(str(header.relative_to(ROOT)).encode())
        digest.update(header.read_bytes())
    return digest.hexdigest()


def compile_object(source, headers):
    """Compiles one source to a cached object; returns its path, or None on error."""
    key = hashlib.sha256()
    key.update(" ".join([compiler(), *CXXFLAGS]).encode())
    key.update(headers.encode())
    key.update(str(source.relative_to(ROOT)).encode())
    key.update(source.read_bytes())
    obj = BUILD / "obj" / (key.hexdigest()[:24] + ".o")
    if obj.exists():
        return obj
    obj.parent.mkdir(parents=True, exist_ok=True)
    partial = obj.with_suffix(".tmp.o")
    result = subprocess.run([compiler(), *CXXFLAGS, "-c", str(source), "-o", str(partial)])
    if result.returncode != 0:
        partial.unlink(missing_ok=True)
        return None
    partial.replace(obj)
    return obj


def link(name, objects):
    exe = BUILD / "bin" / name
    exe.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([compiler(), *map(str, objects), "-o", str(exe)])
    return exe if result.returncode == 0 else None


def build_program(name, main_source):
    headers = header_digest()
    objects = [compile_object(s, headers) for s in [*engine_sources(), main_source]]
    if None in objects:
        return None
    return link(name, objects)


def cmd_test(_):
    tests = sorted((ENGINE / "test").glob("test_*.cpp"))
    if not tests:
        print("no tests found")
        return 1
    failed = []
    for test in tests:
        name = str(test.relative_to(ROOT))
        print(name)
        exe = build_program(test.stem, test)
        if exe is None:
            print("  build failed")
            failed.append(name)
        elif subprocess.run([str(exe)]).returncode != 0:
            failed.append(name)
    if failed:
        print(f"FAILED: {len(failed)} of {len(tests)} test files: " + ", ".join(failed))
        return 1
    print(f"all {len(tests)} test files passed")
    return 0


def png_bytes(width, height, rgb):
    """Encodes 8-bit RGB rows as a PNG."""
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    stride = width * 3
    raw = b"".join(b"\x00" + rgb[y * stride:(y + 1) * stride] for y in range(height))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def rgb565_to_rgb(data):
    out = bytearray()
    for (value,) in struct.iter_unpack("<H", data):
        r, g, b = value >> 11, (value >> 5) & 63, value & 31
        out += bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31))
    return bytes(out)


def upscale(width, height, rgb, factor):
    rows = []
    for y in range(height):
        row = rgb[y * width * 3:(y + 1) * width * 3]
        wide = b"".join(row[i:i + 3] * factor for i in range(0, len(row), 3))
        rows.extend([wide] * factor)
    return width * factor, height * factor, b"".join(rows)


def contact_sheet(images, columns, gap=6, background=(40, 40, 48)):
    """Lays equally sized (width, height, rgb) images out in a grid."""
    width, height, _ = images[0]
    rows = (len(images) + columns - 1) // columns
    sheet_w = columns * width + (columns + 1) * gap
    sheet_h = rows * height + (rows + 1) * gap
    sheet = bytearray(bytes(background) * (sheet_w * sheet_h))
    for index, (_, _, rgb) in enumerate(images):
        left = gap + (index % columns) * (width + gap)
        top = gap + (index // columns) * (height + gap)
        for y in range(height):
            start = ((top + y) * sheet_w + left) * 3
            sheet[start:start + width * 3] = rgb[y * width * 3:(y + 1) * width * 3]
    return sheet_w, sheet_h, bytes(sheet)


def cmd_testcard(args):
    exe = build_program("testcard_host", ENGINE / "host" / "testcard_main.cpp")
    if exe is None:
        print("build failed")
        return 1
    out = Path(args.out).resolve()
    raw_dir = BUILD / "testcard"
    raw_dir.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(exe), str(raw_dir)], capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr.strip() or "test card renderer failed")
        return 1
    by_board = {}
    for line in result.stdout.splitlines():
        name, width, height = line.split()
        width, height = int(width), int(height)
        data = (raw_dir / f"{name}.rgb565").read_bytes()
        if len(data) != width * height * 2:
            print(f"{name}: expected {width * height * 2} bytes, got {len(data)}")
            return 1
        rgb = rgb565_to_rgb(data)
        (out / f"{name}.png").write_bytes(png_bytes(width, height, rgb))
        by_board.setdefault(name.split("_")[0], []).append((width, height, rgb))
    for board, images in sorted(by_board.items()):
        # Rows: palette x scale. Columns: flat, bevel. Doubled so it is easy to view.
        sheet = contact_sheet([upscale(*image, 2) for image in images], columns=2)
        (out / f"sheet_board_{board}.png").write_bytes(png_bytes(*sheet))
    count = sum(len(v) for v in by_board.values())
    print(f"wrote {count} test cards and {len(by_board)} contact sheets to {out}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("test", help="build and run the engine host tests").set_defaults(run=cmd_test)
    card = sub.add_parser("testcard", help="render the test card for both boards")
    card.add_argument("--out", default=str(ROOT / "out" / "testcard"))
    card.set_defaults(run=cmd_testcard)
    args = parser.parse_args()
    try:
        sys.exit(args.run(args))
    except OSError as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
