#!/usr/bin/env python3
"""Build the real TinyGame firmware as an ES-module WebAssembly engine.

Run: python3 tools/sim_build.py [--clean] [--jobs N]
The pinned SDK and LVGL downloads live outside the checkout in the user cache.
Only the HAL is replaced; src/main.cpp, every OS source and all games are built.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import ctypes
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tarfile

from lua_source import REVISION as LUA_REVISION
from lua_source import SOURCE_FILES as LUA_SOURCE_FILES
from lua_source import VERSION as LUA_VERSION
from lua_source import source_dir as lua_source_dir
import tgb


ROOT = Path(__file__).resolve().parents[1]
EMSDK_VERSION = "4.0.19"
EMSDK_REVISION = "7b4e60e4bfcba326025e373024369eaa9904af55"
LVGL_VERSION = "9.6.0"
LVGL_SHA256 = "b20ee3acc1bba13c62d854f9ebd62e4c51e0b443b1e0225892e86442defa84df"
CACHE = Path.home() / ".cache" / "tinygame-sim"
OUTPUT = ROOT / "sim" / "web" / "build"
EXPORTS = [
    "_main", "_malloc", "_free", "_tg_sim_turn", "_tg_sim_button",
    "_tg_sim_tilt", "_tg_sim_command", "_tg_sim_serial_connected", "_tg_sim_install_bundle", "_tg_sim_framebuffer",
    "_tg_sim_width", "_tg_sim_height",
    "_tg_sim_sleeping", "_tg_sim_brightness", "_tg_sim_led", "_tg_sim_stop",
    "_tg_sim_pause", "_tg_sim_resume",
]


def run(command: list[str], **kwargs) -> None:
    subprocess.run(command, check=True, **kwargs)


def sdk_environment() -> tuple[Path, dict[str, str]]:
    sdk = CACHE / f"emsdk-{EMSDK_VERSION}"
    compiler = sdk / "upstream" / "emscripten" / "emcc"
    CACHE.mkdir(parents=True, exist_ok=True)
    if not (sdk / "emsdk").exists():
        print(f"Installing pinned Emscripten {EMSDK_VERSION} in {sdk}", flush=True)
        run(["git", "-c", "advice.detachedHead=false", "clone", "--quiet",
             "--depth", "1", "--branch", EMSDK_VERSION,
             "https://github.com/emscripten-core/emsdk.git", str(sdk)])
    revision = subprocess.check_output(["git", "-C", str(sdk), "rev-parse", "HEAD"],
                                       text=True).strip()
    if revision != EMSDK_REVISION:
        raise RuntimeError(f"Cached emsdk revision is not the pinned {EMSDK_VERSION} revision")
    if not compiler.exists() or not (sdk / ".emscripten").exists():
        run([str(sdk / "emsdk"), "install", EMSDK_VERSION], cwd=sdk)
        run([str(sdk / "emsdk"), "activate", EMSDK_VERSION], cwd=sdk)
    # Read emsdk's config via emcc; do not rely on the caller's PATH or SDK.
    env = dict(os.environ, EMSDK=str(sdk), EM_CONFIG=str(sdk / ".emscripten"),
               EM_CACHE=str(sdk / "upstream" / "emscripten" / "cache"))
    env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
    return compiler, env


def lvgl_source() -> Path:
    for candidate in sorted((ROOT / ".pio" / "libdeps").glob("*/lvgl")):
        manifest = candidate / "library.json"
        if manifest.exists() and json.loads(manifest.read_text()).get("version") == LVGL_VERSION:
            return candidate
    cached = CACHE / f"lvgl-{LVGL_VERSION}"
    complete = cached / ".tinygame-complete"
    if (cached / "lvgl.h").exists() and complete.exists() and complete.read_text() == LVGL_SHA256:
        return cached
    archive = CACHE / f"lvgl-{LVGL_VERSION}.tar.gz"
    url = f"https://codeload.github.com/lvgl/lvgl/tar.gz/refs/tags/v{LVGL_VERSION}"
    print(f"Downloading LVGL {LVGL_VERSION} into {CACHE}", flush=True)
    # System curl uses the OS trust store, including on macOS Python installs
    # whose separate OpenSSL certificate bundle has not been initialized.
    run(["curl", "--fail", "--location", "--silent", "--show-error",
         "--retry", "3", "--connect-timeout", "30", url, "-o", str(archive)])
    digest = hashlib.sha256()
    with archive.open("rb") as downloaded:
        for block in iter(lambda: downloaded.read(1024 * 1024), b""):
            digest.update(block)
    checksum = digest.hexdigest()
    if checksum != LVGL_SHA256:
        archive.unlink()
        raise RuntimeError("LVGL archive checksum does not match the pinned 9.6.0 release")
    with tarfile.open(archive) as bundle:
        # Python 3.12+'s extraction filter prevents path traversal/symlinks.
        if hasattr(tarfile, "data_filter"):
            bundle.extractall(CACHE, filter="data")
        else:
            for member in bundle.getmembers():
                destination = (CACHE / member.name).resolve()
                if not destination.is_relative_to(CACHE.resolve()) or member.issym() or member.islnk():
                    raise RuntimeError("Unsafe LVGL archive member")
            bundle.extractall(CACHE)
    if not (cached / "lvgl.h").exists():
        raise RuntimeError("LVGL download did not contain lvgl.h")
    complete.write_text(LVGL_SHA256)
    archive.unlink()
    return cached


def dependencies(depfile: Path) -> list[Path]:
    if not depfile.exists():
        return []
    data = depfile.read_text().replace("\\\n", " ")
    return [Path(item) for item in shlex.split(data.split(": ", 1)[1])]


def publish_directory(staged: Path, destination: Path) -> None:
    """Swap the complete JS/wasm pair in one filesystem operation."""
    if not destination.exists():
        os.replace(staged, destination)
        return
    libc = ctypes.CDLL(None, use_errno=True)
    if sys.platform == "darwin":
        rename = libc.renamex_np
        rename.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint]
        result = rename(os.fsencode(staged), os.fsencode(destination), 2)  # RENAME_SWAP
    elif sys.platform.startswith("linux") and hasattr(libc, "renameat2"):
        rename = libc.renameat2
        rename.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int,
                           ctypes.c_char_p, ctypes.c_uint]
        result = rename(-100, os.fsencode(staged), -100, os.fsencode(destination), 2)
    else:
        raise RuntimeError("Atomic engine directory publishing needs macOS or Linux")
    if result != 0:
        code = ctypes.get_errno()
        raise OSError(code, os.strerror(code))
    # The staging directory now holds the previous good generation. It stays
    # in the persistent cache until the next link, never exposing partial files.


def embedded_bundle_source(objects_dir: Path, game_dirs: list[Path] | None = None) -> Path:
    if game_dirs is None:
        game_dirs = sorted(path for path in (ROOT / "games").iterdir() if path.is_dir())
    bundles = []
    for directory in sorted(game_dirs, key=lambda path: path.name):
        files = tgb.read_game(directory)
        manifest = tgb.parse_manifest(files, expected_id=directory.name)
        bundles.append((manifest["id"], manifest["name"], tgb.build_bundle(files)))
    if not bundles:
        raise RuntimeError("The simulator needs at least one valid text game in games/<id>")

    content = [
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        '#include "lua_bundle_game.h"',
        "",
        "namespace os::lua_game {",
        "namespace {",
    ]
    for index, (_, _, bundle) in enumerate(bundles):
        values = [
            "    " + ", ".join(f"0x{byte:02x}" for byte in bundle[offset:offset + 12]) + ","
            for offset in range(0, len(bundle), 12)
        ]
        content.extend((f"const std::uint8_t bundle_{index}[] = {{", *values, "};"))
    content.extend(("}  // namespace", "", "extern const EmbeddedBundle tg_sim_bundles[] = {"))
    content.extend(
        f"    {{{json.dumps(game_id)}, {json.dumps(name)}, "
        f"bundle_{index}, sizeof(bundle_{index})}},"
        for index, (game_id, name, _) in enumerate(bundles)
    )
    content.extend((
        "};",
        "extern const std::size_t tg_sim_bundle_count =",
        "    sizeof(tg_sim_bundles) / sizeof(tg_sim_bundles[0]);",
        "}  // namespace os::lua_game",
        "",
    ))
    content = "\n".join(content)
    source = objects_dir / "embedded_games.cpp"
    if not source.exists() or source.read_text() != content:
        source.write_text(content)
    return source


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clean", action="store_true", help="Recompile all simulator objects")
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument("--board", choices=("147b", "169"), default="147b")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    compiler, env = sdk_environment()
    lvgl = lvgl_source()
    lua = lua_source_dir()
    cache_key = hashlib.sha256(str(ROOT).encode()).hexdigest()[:16]
    objects_dir = CACHE / "objects" / (cache_key + ("-169" if args.board == "169" else ""))
    output_dir = OUTPUT if args.board == "147b" else OUTPUT.with_name("build-169")
    if args.clean and objects_dir.exists():
        shutil.rmtree(objects_dir)
    objects_dir.mkdir(parents=True, exist_ok=True)
    embedded_games = embedded_bundle_source(objects_dir)
    output_dir.parent.mkdir(parents=True, exist_ok=True)
    staged = objects_dir / "linked-engine"
    staged.mkdir(parents=True, exist_ok=True)

    include_dirs = [ROOT / "sim" / "arduino", ROOT / "sim" / "hal",
                    ROOT / "include", ROOT / "src" / "hal", ROOT / "src" / "os",
                    ROOT / "engine", lvgl, lua]
    common = ["-O2", "-DLV_CONF_INCLUDE_SIMPLE", "-DTINYGAME_SIM=1",
              *(["-DTG_BOARD=169"] if args.board == "169" else []),
              *[f"-I{path}" for path in include_dirs]]
    sources = [ROOT / "src" / "main.cpp", ROOT / "sim" / "main_sim.cpp"]
    for directory in [ROOT / "src" / "os", ROOT / "src" / "games",
                      ROOT / "sim" / "arduino", ROOT / "sim" / "hal"]:
        sources.extend(path for path in directory.rglob("*")
                       if path.suffix in {".c", ".cc", ".cpp", ".cxx"} and
                       not {"test", "tests"}.intersection(path.relative_to(directory).parts))
    sources.extend((ROOT / "engine" / name) for name in
                   ("bundle_script.cpp", "bundle_view.cpp", "draw_frame.cpp", "manifest_view.cpp",
                    "fixed_step_clock.cpp", "fixed_step_runner.cpp", "lua_runtime.cpp"))
    sources.append(embedded_games)
    sources.extend(lua / name for name in LUA_SOURCE_FILES)
    sources.extend((lvgl / "src").rglob("*.c"))
    sources = sorted(set(sources))
    objects: list[Path] = []
    jobs: list[tuple[list[str], Path, Path]] = []
    for source in sources:
        object_path = objects_dir / (hashlib.sha256(str(source).encode()).hexdigest()[:24] + ".o")
        depfile = object_path.with_suffix(".d")
        signature_file = object_path.with_suffix(".json")
        cc = compiler if source.suffix == ".c" else compiler.with_name("em++")
        # LVGL 9.6's snapshot error macro returns enum zero from a pointer
        # function. Clang warns about this vendor code; do not patch the library
        # or suppress diagnostics on TinyGame's own sources.
        vendor_flags = ["-Wno-non-literal-null-conversion"] if source.is_relative_to(lvgl) else []
        command = [str(cc), *common, *vendor_flags,
                   "-std=c11" if source.suffix == ".c" else "-std=c++17",
                   "-MMD", "-MF", str(depfile), "-c", str(source), "-o", str(object_path)]
        signature = json.dumps([EMSDK_VERSION, LVGL_VERSION, LUA_VERSION,
                                LUA_REVISION, command])
        old_signature = signature_file.read_text() if signature_file.exists() else ""
        deps = dependencies(depfile)
        fresh = (object_path.exists() and old_signature == signature and deps and
                 all(path.exists() and path.stat().st_mtime_ns <= object_path.stat().st_mtime_ns
                     for path in [source, *deps]))
        if not fresh:
            jobs.append((command, signature_file, object_path))
        objects.append(object_path)

    def compile_one(job: tuple[list[str], Path, Path]) -> None:
        command, signature_file, object_path = job
        try:
            result = subprocess.run(command, check=True, cwd=ROOT, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if result.stderr:
                sys.stderr.buffer.write(result.stderr)
        except subprocess.CalledProcessError as error:
            object_path.unlink(missing_ok=True)
            sys.stderr.buffer.write(error.stdout or b"")
            sys.stderr.buffer.write(error.stderr or b"")
            raise
        signature_file.write_text(json.dumps([EMSDK_VERSION, LVGL_VERSION,
                                              LUA_VERSION, LUA_REVISION, command]))

    print(f"Compiling {len(jobs)}/{len(sources)} sources ({args.jobs} jobs)", flush=True)
    # Warm the shared SDK sysroot before parallel compilers contend for it.
    if jobs:
        compile_one(jobs.pop(0))
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        list(pool.map(compile_one, jobs))
    output = output_dir / "tinygame.js"
    link = [
        str(compiler.with_name("em++")), *map(str, objects), "-O2",
        "-sMODULARIZE=1", "-sEXPORT_ES6=1", "-sEXPORT_NAME=createTinyGame",
        "-sENVIRONMENT=web,node", "-sALLOW_MEMORY_GROWTH=1", "-sINITIAL_MEMORY=33554432",
        "-sMAXIMUM_MEMORY=268435456", "-sSTACK_SIZE=1048576",
        "-sNO_EXIT_RUNTIME=1", "-sFILESYSTEM=0",
        "-sEXPORTED_FUNCTIONS=" + json.dumps(EXPORTS),
        '-sEXPORTED_RUNTIME_METHODS=["ccall","cwrap","UTF8ToString","stringToUTF8","lengthBytesUTF8","HEAPU8","HEAPU16"]',
        "--post-js", str(ROOT / "sim" / "arduino" / "runtime.js"),
        "-o", str(staged / "tinygame.js"),
    ]
    link_signature = objects_dir / "link.json"
    signature = json.dumps(link)
    needs_link = (not output.exists() or not output.with_suffix(".wasm").exists() or
                  not link_signature.exists() or link_signature.read_text() != signature or
                  any(obj.stat().st_mtime_ns > output.stat().st_mtime_ns for obj in objects) or
                  (ROOT / "sim" / "arduino" / "runtime.js").stat().st_mtime_ns > output.stat().st_mtime_ns)
    if needs_link:
        print(f"Linking {output.relative_to(ROOT)} + tinygame.wasm", flush=True)
        run(link, cwd=ROOT, env=env)
        if not (staged / "tinygame.js").is_file() or not (staged / "tinygame.wasm").is_file():
            raise RuntimeError("Link did not produce both simulator assets")
        publish_directory(staged, output_dir)
        link_signature.write_text(signature)
    else:
        print("Simulator is up to date.", flush=True)
    print(f"Built {output.relative_to(ROOT)} ({output.with_suffix('.wasm').stat().st_size:,} wasm bytes)")
    return 0


def locked_main() -> int:
    # The browser watcher and a manual build can otherwise race on objects and
    # publish different JS/wasm generations. The OS releases this lock on exit.
    import fcntl

    CACHE.mkdir(parents=True, exist_ok=True)
    with (CACHE / "build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        return main()


if __name__ == "__main__":
    try:
        raise SystemExit(locked_main())
    except subprocess.CalledProcessError as error:
        print(f"Simulator build failed: {Path(error.cmd[0]).name} exited {error.returncode}",
              file=sys.stderr)
        raise SystemExit(1)
    except (OSError, ValueError, RuntimeError) as error:
        print(f"Simulator build failed: {error}", file=sys.stderr)
        raise SystemExit(1)
