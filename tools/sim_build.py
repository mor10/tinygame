#!/usr/bin/env python3
"""Build the TinyGame engine as WebAssembly for the browser simulator.

  python3 tools/sim_build.py                build sim/web/build/tinygame.js + .wasm
  python3 tools/sim_build.py --clean        forget cached objects and rebuild
  python3 tools/sim_build.py --install-only only install the pinned Emscripten SDK

The pinned Emscripten SDK is installed once, outside the repository, in
~/.cache/tinygame-sim (override with TINYGAME_SIM_CACHE). Objects are cached by
content in .build/sim, so a second run with nothing changed compiles nothing.
"""

import argparse
import concurrent.futures
import fcntl
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "engine"
THIRD_PARTY = ROOT / "third_party"
SIM_WASM = ROOT / "sim" / "wasm"
OUTPUT = ROOT / "sim" / "web" / "build"
BUILD = ROOT / ".build" / "sim"

EMSDK_VERSION = "4.0.19"
EMSDK_REVISION = "7b4e60e4bfcba326025e373024369eaa9904af55"
EMSDK_URL = "https://github.com/emscripten-core/emsdk.git"
CACHE = Path(os.environ.get("TINYGAME_SIM_CACHE") or Path.home() / ".cache" / "tinygame-sim")
SDK = CACHE / f"emsdk-{EMSDK_VERSION}"

# Keep these the same as tools/tg.py so the browser build is as strict as the
# host build. (-O2 instead of -O1: the frame fingerprints must not depend on it.)
CXXFLAGS = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror"]
VENDOR_DEFINES = ["-DLUA_32BITS=1"]
CFLAGS = ["-std=c99", "-O2", *VENDOR_DEFINES]
NOT_LIBRARY = {"test", "host"}

# The C API in sim/wasm/sim_main.cpp.
EXPORTS = ["_malloc", "_free", "_tg_sim_init", "_tg_sim_tick", "_tg_sim_draw",
           "_tg_sim_width", "_tg_sim_height", "_tg_sim_load_bundle", "_tg_sim_error",
           "_tg_sim_has_game", "_tg_sim_probe"]
LINK_FLAGS = [
    "-O2", "-sMODULARIZE=1", "-sEXPORT_ES6=1", "-sEXPORT_NAME=createTinyGame",
    "-sENVIRONMENT=web,node", "-sALLOW_MEMORY_GROWTH=1", "-sNO_EXIT_RUNTIME=1",
    "-sFILESYSTEM=0", "--no-entry",
    "-sEXPORTED_FUNCTIONS=" + json.dumps(EXPORTS),
    '-sEXPORTED_RUNTIME_METHODS=["HEAPU8","HEAPU16","UTF8ToString"]',
]


class BuildError(Exception):
    pass


def run(command, what, **kwargs):
    """Runs a setup step; on failure says which step and command failed."""
    try:
        result = subprocess.run(command, **kwargs)
    except OSError as error:
        raise BuildError(f"{what}: cannot run {command[0]}: {error}") from error
    if result.returncode != 0:
        raise BuildError(f"{what} failed (exit {result.returncode}): {' '.join(map(str, command))}")


def install_sdk():
    """Installs the pinned SDK if needed. Returns (emcc path, environment)."""
    emcc = SDK / "upstream" / "emscripten" / "emcc"
    CACHE.mkdir(parents=True, exist_ok=True)
    if not (SDK / "emsdk.py").exists():
        if SDK.exists():
            shutil.rmtree(SDK)  # a clone that was interrupted
        print(f"Installing Emscripten {EMSDK_VERSION} in {SDK} (one time, a few minutes)", flush=True)
        partial = SDK.with_name(SDK.name + ".partial")
        if partial.exists():
            shutil.rmtree(partial)
        run(["git", "-c", "advice.detachedHead=false", "clone", "--quiet", "--depth", "1",
             "--branch", EMSDK_VERSION, EMSDK_URL, str(partial)], f"git clone of {EMSDK_URL}")
        partial.rename(SDK)
    try:
        revision = subprocess.check_output(["git", "-C", str(SDK), "rev-parse", "HEAD"],
                                           text=True).strip()
    except (OSError, subprocess.CalledProcessError) as error:
        raise BuildError(f"cannot read the emsdk revision in {SDK}: {error}") from error
    if revision != EMSDK_REVISION:
        raise BuildError(f"{SDK} is at revision {revision}, not the pinned {EMSDK_REVISION}. "
                         f"Delete that folder and run again.")
    marker = SDK / ".tinygame-installed"
    if not (emcc.exists() and marker.exists()):
        emsdk = [sys.executable, str(SDK / "emsdk.py")]
        run([*emsdk, "install", EMSDK_VERSION], "emsdk install (downloads from storage.googleapis.com)",
            cwd=SDK)
        run([*emsdk, "activate", EMSDK_VERSION], "emsdk activate", cwd=SDK)
        if not emcc.exists():
            raise BuildError(f"emsdk install finished but {emcc} is missing")
        marker.write_text(EMSDK_REVISION + "\n")
    # Use only this SDK's configuration, whatever the caller's PATH holds.
    env = dict(os.environ, EMSDK=str(SDK), EM_CONFIG=str(SDK / ".emscripten"))
    env["PATH"] = str(emcc.parent) + os.pathsep + env.get("PATH", "")
    env.pop("EM_CACHE", None)
    return emcc, env


def vendor_dirs():
    return sorted(p for p in THIRD_PARTY.glob("*/src") if p.is_dir()) if THIRD_PARTY.exists() else []


def sources():
    """The same engine and vendor sources as tools/tg.py, plus the simulator's own."""
    engine = sorted(p for p in ENGINE.rglob("*.cpp")
                    if not NOT_LIBRARY.intersection(p.relative_to(ENGINE).parts))
    vendor = sorted(source for folder in vendor_dirs() for source in folder.glob("*.c"))
    return [*engine, *vendor, *sorted(SIM_WASM.glob("*.cpp"))]


def header_digest():
    """One digest of every header: any header edit rebuilds everything."""
    digest = hashlib.sha256()
    headers = [*ENGINE.rglob("*.h"), *SIM_WASM.glob("*.h"),
               *(h for d in vendor_dirs() for h in d.glob("*.h"))]
    for header in sorted(headers):
        digest.update(str(header.relative_to(ROOT)).encode())
        digest.update(header.read_bytes())
    return digest.hexdigest()


def object_job(emcc, source, headers):
    """Returns (object path, compile command) for one source."""
    if source.suffix == ".c":
        command = [str(emcc), *CFLAGS]
    else:
        command = [str(emcc.with_name("em++")), *CXXFLAGS, *VENDOR_DEFINES]
    command += [f"-I{folder}" for folder in vendor_dirs()]
    key = hashlib.sha256()
    for part in (EMSDK_REVISION, " ".join(command), headers, str(source.relative_to(ROOT))):
        key.update(part.encode())
    key.update(source.read_bytes())
    return BUILD / "obj" / (key.hexdigest()[:24] + ".o"), command


def compile_one(job, env):
    source, obj, command = job
    partial = obj.with_suffix(f".{os.getpid()}.tmp.o")
    result = subprocess.run([*command, "-c", str(source), "-o", str(partial)], cwd=ROOT, env=env,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if result.returncode != 0:
        partial.unlink(missing_ok=True)
        return f"{source.relative_to(ROOT)}: compile failed\n{result.stdout}"
    if result.stdout:
        sys.stderr.write(result.stdout)
    partial.replace(obj)
    return None


def build(args):
    started = time.monotonic()
    emcc, env = install_sdk()
    if args.install_only:
        print(f"Emscripten {EMSDK_VERSION} is installed in {SDK}")
        return
    if args.clean and BUILD.exists():
        shutil.rmtree(BUILD)
    (BUILD / "obj").mkdir(parents=True, exist_ok=True)

    headers = header_digest()
    all_sources = sources()
    jobs, objects = [], []
    for source in all_sources:
        obj, command = object_job(emcc, source, headers)
        objects.append(obj)
        if not obj.exists():
            jobs.append((source, obj, command))
    print(f"Compiling {len(jobs)} of {len(all_sources)} sources", flush=True)
    errors = []
    if jobs:
        # The first compile fills the SDK's shared library cache; do it alone.
        errors.append(compile_one(jobs[0], env))
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
            errors += pool.map(lambda job: compile_one(job, env), jobs[1:])
    errors = [e for e in errors if e]
    if errors:
        raise BuildError("\n".join(errors).rstrip())

    js, wasm = OUTPUT / "tinygame.js", OUTPUT / "tinygame.wasm"
    signature = json.dumps([EMSDK_REVISION, LINK_FLAGS, [o.name for o in objects]])
    stamp = BUILD / "link.json"
    if js.exists() and wasm.exists() and stamp.exists() and stamp.read_text() == signature:
        print("Linking: up to date")
    else:
        print("Linking sim/web/build/tinygame.js + tinygame.wasm", flush=True)
        staged = BUILD / "link"
        if staged.exists():
            shutil.rmtree(staged)
        staged.mkdir(parents=True)
        result = subprocess.run([str(emcc.with_name("em++")), *map(str, objects), *LINK_FLAGS,
                                 "-o", str(staged / "tinygame.js")], cwd=ROOT, env=env)
        if result.returncode != 0:
            raise BuildError(f"link failed (exit {result.returncode})")
        if not ((staged / "tinygame.js").is_file() and (staged / "tinygame.wasm").is_file()):
            raise BuildError("link did not produce both tinygame.js and tinygame.wasm")
        OUTPUT.mkdir(parents=True, exist_ok=True)
        stamp.unlink(missing_ok=True)
        (staged / "tinygame.wasm").replace(wasm)
        (staged / "tinygame.js").replace(js)
        stamp.write_text(signature)
    print(f"Built {js.relative_to(ROOT)} and {wasm.name} ({wasm.stat().st_size:,} bytes of wasm) "
          f"in {time.monotonic() - started:.1f} s")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--clean", action="store_true", help="recompile every source")
    parser.add_argument("--install-only", action="store_true",
                        help="install the Emscripten SDK and stop")
    parser.add_argument("--jobs", type=int, default=min(8, os.cpu_count() or 1))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be 1 or more")
    try:
        CACHE.mkdir(parents=True, exist_ok=True)
        # Two builds at once would fight over the SDK install and the objects.
        with (CACHE / "build.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            build(args)
    except (BuildError, OSError) as error:
        print(f"Simulator build failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
