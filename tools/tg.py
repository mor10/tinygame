#!/usr/bin/env python3
"""TinyGame v2 helper.

  python3 tools/tg.py test                 build and run the engine host tests
  python3 tools/tg.py testcard [--out DIR] render every test-card variant to PNG
  python3 tools/tg.py demo [--out DIR]     render the scrolling demo frames to PNG
  python3 tools/tg.py golden [--update]    check (or accept) the golden-image fingerprints
  python3 tools/tg.py check [game]         look for mistakes in a game (or in every game)
  python3 tools/tg.py pack <game> [-o F]   pack a game into one .tgb bundle file
  python3 tools/tg.py new <id> --kind platformer [--name "NAME"]   start a new game
  python3 tools/tg.py run <game> --steps F [--board a|b|both] [--probe] [--shot P] [--json]
                                           play a steps script on a game and check it
  python3 tools/tg.py sim [game] [--port N] [--no-open]   pack a game and play it in the browser
  python3 tools/tg.py verify [game]        the whole gate: check, tests, golden, every steps file

More commands arrive with later milestones (see docs/spec.md, section 12).
"""

import argparse
import hashlib
import json
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
# Vendored C libraries (currently Lua) live in third_party/<name>/src. They are
# built with the C compiler, without -Werror, and linked into every program.
THIRD_PARTY = ROOT / "third_party"
VENDOR_DEFINES = ["-DLUA_32BITS=1"]
CFLAGS = ["-std=c99", "-O1", *VENDOR_DEFINES]
# Engine folders that are programs or tests, not part of the engine library.
NOT_LIBRARY = {"test", "host"}


def compiler():
    return os.environ.get("CXX", "c++")


def c_compiler():
    return os.environ.get("CC", "cc")


def vendor_dirs():
    return sorted(p for p in THIRD_PARTY.glob("*/src") if p.is_dir()) if THIRD_PARTY.exists() else []


def vendor_sources():
    return sorted(source for folder in vendor_dirs() for source in folder.glob("*.c"))


def include_flags():
    return [f"-I{folder}" for folder in vendor_dirs()]


def engine_sources():
    return sorted(p for p in ENGINE.rglob("*.cpp")
                  if not NOT_LIBRARY.intersection(p.relative_to(ENGINE).parts))


def header_digest():
    """One digest of every engine header: any header edit rebuilds everything."""
    digest = hashlib.sha256()
    headers = [*ENGINE.rglob("*.h"), *(h for d in vendor_dirs() for h in d.glob("*.h"))]
    for header in sorted(headers):
        digest.update(str(header.relative_to(ROOT)).encode())
        digest.update(header.read_bytes())
    return digest.hexdigest()


def compile_object(source, headers):
    """Compiles one source to a cached object; returns its path, or None on error."""
    is_c = source.suffix == ".c"
    command = [c_compiler(), *CFLAGS] if is_c else [compiler(), *CXXFLAGS, *VENDOR_DEFINES]
    command += include_flags()
    key = hashlib.sha256()
    key.update(" ".join(command).encode())
    key.update(headers.encode())
    key.update(str(source.relative_to(ROOT)).encode())
    key.update(source.read_bytes())
    obj = BUILD / "obj" / (key.hexdigest()[:24] + ".o")
    if obj.exists():
        return obj
    obj.parent.mkdir(parents=True, exist_ok=True)
    partial = obj.with_suffix(".tmp.o")
    result = subprocess.run([*command, "-c", str(source), "-o", str(partial)])
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
    objects = [compile_object(s, headers)
               for s in [*engine_sources(), *vendor_sources(), main_source]]
    if None in objects:
        return None
    return link(name, objects)


def engine_tests():
    """Builds and runs engine/test/test_*.cpp. Returns (failed names, number of files)."""
    tests = sorted((ENGINE / "test").glob("test_*.cpp"))
    if not tests:
        print("no tests found")
        return ["engine/test (no tests found)"], 0
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
    return failed, len(tests)


def python_tests():
    """The packer, checker and `tg run` tests (Python, tools/tests). True when they pass."""
    print("tools/tests (python unittest)")
    return subprocess.run([sys.executable, "-m", "unittest", "discover", "-s", "tools/tests"],
                          cwd=ROOT).returncode == 0


def hardware_tests():
    """The hal/ and os/ host tests. True when they pass."""
    import os_tests
    return os_tests.run_all() == 0


def cmd_test(_):
    failed, count = engine_tests()
    if not python_tests():
        failed.append("tools/tests (python unittest)")
    if not hardware_tests():
        failed.append("hal/ and os/ tests")
    total = count + 2
    if failed:
        print(f"FAILED: {len(failed)} of {total} test groups: " + ", ".join(failed))
        return 1
    print(f"all {total} test groups passed")
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


def render_host(program, main_source):
    """Builds and runs a host renderer. Returns [(name, width, height, rgb, extra)], or None."""
    exe = build_program(program, main_source)
    if exe is None:
        print("build failed")
        return None
    raw_dir = BUILD / program
    raw_dir.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(exe), str(raw_dir)], capture_output=True, text=True)
    if result.returncode != 0:
        print(result.stderr.strip() or f"{program} failed")
        return None
    frames = []
    for line in result.stdout.splitlines():
        name, width, height, *extra = line.split()
        width, height = int(width), int(height)
        data = (raw_dir / f"{name}.rgb565").read_bytes()
        if len(data) != width * height * 2:
            print(f"{name}: expected {width * height * 2} bytes, got {len(data)}")
            return None
        frames.append((name, width, height, rgb565_to_rgb(data), extra))
    return frames


def cmd_testcard(args):
    frames = render_host("testcard_host", ENGINE / "host" / "testcard_main.cpp")
    if frames is None:
        return 1
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    by_board = {}
    for name, width, height, rgb, _ in frames:
        (out / f"{name}.png").write_bytes(png_bytes(width, height, rgb))
        by_board.setdefault(name.split("_")[0], []).append((width, height, rgb))
    for board, images in sorted(by_board.items()):
        # Rows: palette x scale. Columns: flat, bevel. Doubled so it is easy to view.
        sheet = contact_sheet([upscale(*image, 2) for image in images], columns=2)
        (out / f"sheet_board_{board}.png").write_bytes(png_bytes(*sheet))
    print(f"wrote {len(frames)} test cards and {len(by_board)} contact sheets to {out}")
    return 0


GOLDEN_HEADER = ENGINE / "test" / "golden_scroll_demo.h"


def golden_header_text(frames):
    rows = "\n".join(f'    {{"{name}", 0x{extra[0]}ULL}},' for name, _, _, _, extra in frames)
    return ("#pragma once\n\n"
            "// Generated by `python3 tools/tg.py golden --update`. Do not edit by hand.\n"
            "// Fingerprints of the scrolling demo frames listed in engine/demo/scroll_cases.h.\n\n"
            "#include <cstdint>\n\n"
            "namespace tg::golden {\n\n"
            "struct Frame {\n    const char* name;\n    std::uint64_t hash;\n};\n\n"
            "inline constexpr Frame SCROLL_DEMO[] = {\n" + rows + "\n};\n\n"
            "}  // namespace tg::golden\n")


def cmd_demo(args):
    frames = render_host("demo_host", ENGINE / "host" / "demo_main.cpp")
    if frames is None:
        return 1
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    for name, width, height, rgb, _ in frames:
        (out / f"{name}.png").write_bytes(png_bytes(width, height, rgb))
    print(f"wrote {len(frames)} demo frames to {out}")
    game = render_host("game_host", ENGINE / "host" / "game_main.cpp")
    if game is None:
        return 1
    for name, width, height, rgb, _ in game:
        (out / f"{name}.png").write_bytes(png_bytes(width, height, rgb))
    print(f"wrote {len(game)} game frames to {out}")
    return 0


def cmd_golden(args):
    frames = render_host("demo_host", ENGINE / "host" / "demo_main.cpp")
    if frames is None:
        return 1
    text = golden_header_text(frames)
    current = GOLDEN_HEADER.read_text() if GOLDEN_HEADER.exists() else ""
    if text == current:
        print("golden images are up to date")
        return 0
    if not args.update:
        print("golden images differ from the renderer's output.")
        print("Look at `python3 tools/tg.py demo`; if the change is intended, run "
              "`python3 tools/tg.py golden --update`.")
        return 1
    GOLDEN_HEADER.write_text(text)
    print(f"updated {GOLDEN_HEADER.relative_to(ROOT)}")
    return 0


GAME_GOLDEN_HEADER = ENGINE / "test" / "golden_game.h"


def cmd_golden_game(args):
    """The same check for the engine running the built-in cave_hop test bundle."""
    frames = render_host("game_host", ENGINE / "host" / "game_main.cpp")
    if frames is None:
        return 1
    rows = "\n".join(f'    {{"{name}", 0x{extra[0]}ULL}},' for name, _, _, _, extra in frames)
    text = ("#pragma once\n\n"
            "// Generated by `python3 tools/tg.py golden --update`. Do not edit by hand.\n"
            "// Fingerprints of the engine frames listed in engine/test/game_cases.h.\n\n"
            "#include <cstdint>\n\n"
            "namespace tg::golden {\n\n"
            "struct GameFrame {\n    const char* name;\n    std::uint64_t hash;\n};\n\n"
            "inline constexpr GameFrame GAME[] = {\n" + rows + "\n};\n\n"
            "}  // namespace tg::golden\n")
    current = GAME_GOLDEN_HEADER.read_text() if GAME_GOLDEN_HEADER.exists() else ""
    if text == current:
        print("game golden frames are up to date")
        return 0
    if not args.update:
        print("game frames differ from the engine's output.")
        print("Look at `python3 tools/tg.py demo`; if the change is intended, run "
              "`python3 tools/tg.py golden --update`.")
        return 1
    GAME_GOLDEN_HEADER.write_text(text)
    print(f"updated {GAME_GOLDEN_HEADER.relative_to(ROOT)}")
    return 0


def game_folder(text):
    """Turns an id or a folder path given on the command line into a folder."""
    path = Path(text)
    if path.is_dir():
        return path
    for parent in ("games", "starters"):
        if (ROOT / parent / text).is_dir():
            return ROOT / parent / text
    return ROOT / "games" / text


def shown_path(path):
    """A path relative to the repository when it is inside it."""
    try:
        return str(Path(path).resolve().relative_to(ROOT))
    except ValueError:
        return str(path)


def report_problems(folder, problems):
    for problem in problems:
        print(problem.text(shown_path(folder)))


def cmd_check(args):
    from tgpack import check_game
    if args.game:
        folders = [game_folder(args.game)]
    else:
        folders = sorted(p for parent in ("games", "starters") if (ROOT / parent).is_dir()
                         for p in (ROOT / parent).iterdir() if p.is_dir())
    results = [(folder, check_game(folder)) for folder in folders]
    count = sum(len(problems) for _, problems in results)
    if args.json:
        print(json.dumps({"ok": count == 0, "games": [
            {"id": folder.resolve().name, "folder": shown_path(folder), "ok": not problems,
             "problems": [p.to_dict() for p in problems]} for folder, problems in results]},
            indent=2))
    else:
        for folder, problems in results:
            if problems:
                report_problems(folder, problems)
            else:
                print(f"{shown_path(folder)}: ok")
        if count:
            print(f"{count} problem{'s' if count != 1 else ''} found")
    return 1 if count else 0


def cmd_pack(args):
    from tgpack import PackError, pack_game
    folder = game_folder(args.game)
    try:
        data = pack_game(folder)
    except PackError as error:
        if args.json:
            print(json.dumps({"ok": False, "folder": shown_path(folder),
                              "problems": [p.to_dict() for p in error.problems]}, indent=2))
        else:
            report_problems(folder, error.problems)
            print("not packed: fix the problems above first")
        return 1
    out = Path(args.output) if args.output else ROOT / "out" / f"{folder.resolve().name}.tgb"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    if args.json:
        print(json.dumps({"ok": True, "folder": shown_path(folder), "output": shown_path(out),
                          "bytes": len(data), "crc32": f"{zlib.crc32(data[16:]):08x}",
                          "problems": []}, indent=2))
    else:
        print(f"packed {shown_path(folder)} into {shown_path(out)} ({len(data)} bytes)")
    return 0


BOARDS = {"a": (320, 172), "b": (280, 240)}


def pack_for_run(folder):
    """Checks and packs a game into .build/run. Returns (bundle path, problem lines)."""
    from tgpack import PackError, check_game, pack_game
    problems = check_game(folder)
    if problems:
        return None, [p.text(shown_path(folder)) for p in problems]
    try:
        data = pack_game(folder)
    except PackError as error:
        return None, [p.text(shown_path(folder)) for p in error.problems]
    out = BUILD / "run" / f"{folder.resolve().name}.tgb"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    return out, []


def play_steps(bundle, steps, boards, probe=False, shot=None):
    """Plays a steps file on a packed game at each board. Returns a list of result
    dicts: board, width, height, ok, ticks, score, hurts, error, failures, probe,
    syntax (a list of {line, text}), shot (path or None), message (a fatal problem)."""
    exe = build_program("run_host", ENGINE / "host" / "run_main.cpp")
    if exe is None:
        return None
    results = []
    for board in boards:
        width, height = BOARDS[board]
        raw = BUILD / "run" / f"shot_{board}.rgb565"
        command = [str(exe), "--bundle", str(bundle), "--steps", str(steps), "--size",
                   str(width), str(height)]
        if probe:
            command.append("--probe")
        if shot:
            raw.parent.mkdir(parents=True, exist_ok=True)
            command += ["--raw", str(raw)]
        done = subprocess.run(command, capture_output=True, text=True)
        result = {"board": board, "width": width, "height": height, "ok": done.returncode == 0,
                  "ticks": 0, "score": 0, "hurts": 0, "error": "", "failures": [], "probe": "",
                  "syntax": [], "shot": None, "message": ""}
        in_probe = False
        for line in done.stdout.splitlines():
            part = line.split("\t")
            if in_probe:
                if line == "probe-end":
                    in_probe = False
                else:
                    result["probe"] += line + "\n"
            elif line == "probe-begin":
                in_probe = True
            elif part[0] == "syntax":
                result["syntax"].append({"line": int(part[1]), "text": part[2]})
            elif part[0] == "fail":
                result["failures"].append({"line": int(part[1]), "expected": part[2],
                                           "found": part[3]})
            elif part[0] == "result":
                result["ticks"], result["score"], result["hurts"] = map(int, part[1:4])
                result["error"] = part[4] if len(part) > 4 else ""
        if done.returncode not in (0, 1) and not result["syntax"]:
            result["message"] = done.stderr.strip() or f"the runner stopped (exit {done.returncode})"
        if result["error"]:
            result["ok"] = False
        if shot and raw.exists() and done.returncode in (0, 1):
            target = Path(shot)
            if len(boards) > 1:
                target = target.with_name(f"{target.stem}_{board}{target.suffix or '.png'}")
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(png_bytes(width, height, rgb565_to_rgb(raw.read_bytes())))
            result["shot"] = str(target)
        results.append(result)
    return results


def print_run(results, steps_name):
    problems = 0
    for r in results:
        print(f"board {r['board']} ({r['width']}x{r['height']}), {steps_name}")
        for mistake in r["syntax"]:
            print(f"  {steps_name}:{mistake['line']}: {mistake['text']}")
            problems += 1
        if r["message"]:
            print(f"  {r['message']}")
            problems += 1
        for f in r["failures"]:
            print(f"  FAIL {steps_name}:{f['line']}: {f['expected']}, found {f['found']}")
            problems += 1
        if r["error"]:
            print(f"  GAME STOPPED: {r['error']}")
            problems += 1
        if not r["syntax"] and not r["message"]:
            print(f"  {r['ticks']} ticks, score {r['score']}, hurts {r['hurts']}"
                  + (" ok" if r["ok"] else ""))
        if r["probe"]:
            print("  probe:")
            print("".join("    " + line + "\n" for line in r["probe"].splitlines()), end="")
        if r["shot"]:
            print(f"  screenshot: {r['shot']}")
    return problems


def cmd_run(args):
    folder = game_folder(args.game)
    steps = Path(args.steps)
    if not folder.is_dir():
        print(f"Error: there is no game '{args.game}' (looked in games/ and starters/)")
        return 1
    if not steps.is_file():
        print(f"Error: cannot find the steps file {args.steps}")
        return 1
    bundle, problems = pack_for_run(folder)
    if bundle is None:
        if args.json:
            print(json.dumps({"ok": False, "game": folder.resolve().name, "problems": problems,
                              "runs": []}, indent=2))
        else:
            print("\n".join(problems))
            print("not run: fix the problems above first (python3 tools/tg.py check)")
        return 1
    boards = ["a", "b"] if args.board == "both" else [args.board]
    results = play_steps(bundle, steps, boards, args.probe, args.shot)
    if results is None:
        print("Error: the runner could not be built")
        return 1
    ok = all(r["ok"] for r in results)
    if args.json:
        print(json.dumps({"ok": ok, "game": folder.resolve().name, "steps": shown_path(steps),
                          "runs": results}, indent=2))
    else:
        print(f"{folder.resolve().name}:")
        count = print_run(results, shown_path(steps))
        print("all steps held" if ok else f"run failed: {count} problem{'s' if count != 1 else ''}")
    return 0 if ok else 1


def steps_stage(folders, report):
    """Plays every test/*.steps of the given games on both boards. Returns the
    number of steps files run and the number that failed."""
    ran = failed = 0
    for folder in folders:
        files = sorted((folder / "test").glob("*.steps"))
        if not files:
            continue
        bundle, problems = pack_for_run(folder)
        if bundle is None:
            report("\n".join(problems))
            ran += len(files)
            failed += len(files)
            continue
        for steps in files:
            ran += 1
            results = play_steps(bundle, steps, ["a", "b"])
            if results is None or not all(r["ok"] for r in results):
                failed += 1
                report(f"{shown_path(steps)} failed")
                if results:
                    import contextlib
                    import io
                    with contextlib.redirect_stdout(io.StringIO()) as text:
                        print_run(results, shown_path(steps))
                    report(text.getvalue().rstrip())
    return ran, failed


def cmd_verify(args):
    """Everything that must hold before a change is done. One line per stage."""
    class Quiet:  # a namespace for the golden check
        update = False
    folders = [game_folder(args.game)] if args.game else sorted(
        p for parent in ("games", "starters") if (ROOT / parent).is_dir()
        for p in (ROOT / parent).iterdir() if p.is_dir())
    if args.game and not folders[0].is_dir():
        print(f"Error: there is no game '{args.game}' (looked in games/ and starters/)")
        return 1
    summary = []
    steps_notes = []

    def stage(name, run):
        print(f"\n== {name}", flush=True)
        ok, detail = run()
        summary.append((name, ok, detail))
        print(f"-- {name}: {'ok' if ok else 'FAILED'}" + (f" ({detail})" if detail else ""),
              flush=True)

    def check_stage():
        from tgpack import check_game
        bad = 0
        for folder in folders:
            problems = check_game(folder)
            report_problems(folder, problems)
            bad += len(problems)
        return bad == 0, f"{len(folders)} game{'s' if len(folders) != 1 else ''}"

    def engine_stage():
        failed, count = engine_tests()
        return not failed, f"{count} test files" if not failed else ", ".join(failed)

    def golden_stage():
        return cmd_golden(Quiet) == 0 and cmd_golden_game(Quiet) == 0, ""

    def steps_run():
        ran, failed = steps_stage(folders, steps_notes.append)
        for note in steps_notes:
            print(note)
        return failed == 0, f"{ran} steps file{'s' if ran != 1 else ''} on both boards"

    stage("check", check_stage)
    stage("engine tests", engine_stage)
    stage("python tests", lambda: (python_tests(), ""))
    stage("hal and os tests", lambda: (hardware_tests(), ""))
    stage("golden frames", golden_stage)
    stage("game steps", steps_run)
    print("\nverify summary")
    for name, ok, detail in summary:
        print(f"  {'ok    ' if ok else 'FAILED'} {name}" + (f" ({detail})" if detail else ""))
    if all(ok for _, ok, _ in summary):
        print("verify: everything passed")
        return 0
    print("verify: FAILED")
    return 1


def cmd_sim(args):
    command = [sys.executable, str(ROOT / "tools" / "sim_serve.py")]
    if args.game:
        pack = argparse.Namespace(game=args.game, output=None, json=False)
        if cmd_pack(pack) != 0:
            return 1
        command.append(str(ROOT / "out" / f"{game_folder(args.game).resolve().name}.tgb"))
    if args.port is not None:
        command += ["--port", str(args.port)]
    if args.no_open:
        command.append("--no-open")
    try:
        return subprocess.run(command).returncode
    except KeyboardInterrupt:
        return 0


def cmd_new(args):
    from tgpack.newgame import NewGameError, new_game
    try:
        folder = new_game(args.id, args.kind, args.name, games_dir=args.games_dir)
    except NewGameError as error:
        if args.json:
            print(json.dumps({"ok": False, "error": str(error)}, indent=2))
        else:
            print(f"Error: {error}")
        return 1
    if args.json:
        print(json.dumps({"ok": True, "id": args.id, "folder": shown_path(folder),
                          "files": sorted(p.name for p in folder.iterdir())}, indent=2))
    else:
        print(f"made a new {args.kind} game in {shown_path(folder)}")
        print(f"next: python3 tools/tg.py check {args.id}")
    return 0


def main():
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("test", help="build and run the engine host tests").set_defaults(run=cmd_test)
    card = sub.add_parser("testcard", help="render the test card for both boards")
    card.add_argument("--out", default=str(ROOT / "out" / "testcard"))
    card.set_defaults(run=cmd_testcard)
    demo = sub.add_parser("demo", help="render the scrolling demo's golden frames to PNG")
    demo.add_argument("--out", default=str(ROOT / "out" / "demo"))
    demo.set_defaults(run=cmd_demo)
    golden = sub.add_parser("golden", help="check the golden-image fingerprints")
    golden.add_argument("--update", action="store_true", help="accept the current output")
    golden.set_defaults(run=lambda a: cmd_golden(a) | cmd_golden_game(a))
    check = sub.add_parser("check", help="look for mistakes in a game")
    check.add_argument("game", nargs="?", help="game id or folder (default: every game)")
    check.add_argument("--json", action="store_true", help="print the result as JSON")
    check.set_defaults(run=cmd_check)
    pack = sub.add_parser("pack", help="pack a game into one .tgb bundle")
    pack.add_argument("game", help="game id or folder")
    pack.add_argument("-o", "--output", help="file to write (default: out/<id>.tgb)")
    pack.add_argument("--json", action="store_true", help="print the result as JSON")
    pack.set_defaults(run=cmd_pack)
    new = sub.add_parser("new", help="start a new game from a starter")
    new.add_argument("id", help="id of the new game, like space_cat")
    new.add_argument("--kind", required=True, help="which starter to copy (platformer)")
    new.add_argument("--name", help="the name shown on the console (1-13 characters)")
    new.add_argument("--games-dir", help="where to make the game (default: games/)")
    new.add_argument("--json", action="store_true", help="print the result as JSON")
    new.set_defaults(run=cmd_new)
    run = sub.add_parser("run", help="play a steps script on a game and check it")
    run.add_argument("game", help="game id or folder")
    run.add_argument("--steps", required=True, help="steps file (docs/formats/steps.md)")
    run.add_argument("--board", choices=["a", "b", "both"], default="both")
    run.add_argument("--probe", action="store_true", help="print the final probe text")
    run.add_argument("--shot", help="write the final frame to this PNG")
    run.add_argument("--json", action="store_true", help="print the result as JSON")
    run.set_defaults(run=cmd_run)
    verify = sub.add_parser("verify", help="check, test and play everything (the gate)")
    verify.add_argument("game", nargs="?", help="only check and play this game's steps")
    verify.set_defaults(run=cmd_verify)
    sim = sub.add_parser("sim", help="pack a game and play it in the browser simulator")
    sim.add_argument("game", nargs="?", help="game id or folder (default: the scrolling demo)")
    sim.add_argument("--port", type=int, help="port to serve on (default: any free port)")
    sim.add_argument("--no-open", action="store_true", help="do not open a browser")
    sim.set_defaults(run=cmd_sim)
    args = parser.parse_args()
    try:
        sys.exit(args.run(args))
    except OSError as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
