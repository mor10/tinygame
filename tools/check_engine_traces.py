#!/usr/bin/env python3
"""Compare a fixed-step engine trace on native C++ and pinned WebAssembly."""

import os
from itertools import zip_longest
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

import lua_source
import sim_build


def main():
    compiler = shutil.which(os.environ.get("CXX", "c++"))
    node = shutil.which("node")
    sdk = sim_build.CACHE / f"emsdk-{sim_build.EMSDK_VERSION}"
    if not compiler or not node:
        raise RuntimeError("A native C++ compiler and Node.js are required.")
    if not (sdk / "upstream/emscripten/emcc").is_file() or not (sdk / ".emscripten").is_file():
        raise RuntimeError("Build the simulator first to install the pinned Emscripten SDK.")
    if not (lua_source.CACHE / f"lua-{lua_source.VERSION}").is_dir():
        raise RuntimeError("Build the simulator first to install the pinned Lua source.")
    emcc, env = sim_build.sdk_environment()
    lua = lua_source.source_dir()
    cc = shutil.which(os.environ.get("CC", "cc"))
    if not cc:
        raise RuntimeError("A native C compiler is required.")
    root = sim_build.ROOT
    sources = [
        root / "engine/test/trace_fixed_step.cpp",
        root / "engine/fixed_step_clock.cpp",
        root / "engine/fixed_step_runner.cpp",
    ]
    flags = ["-std=c++17", "-O2", "-ffp-contract=off",
             f"-I{root / 'engine'}", f"-I{root / 'include'}"]
    with tempfile.TemporaryDirectory(prefix="tinygame-trace-") as directory:
        native = Path(directory) / "trace"
        wasm = Path(directory) / "trace.js"
        subprocess.run([compiler, *flags, *map(str, sources), "-o", str(native)], check=True)
        subprocess.run([str(emcc.with_name("em++")), *flags, *map(str, sources),
                        "-sENVIRONMENT=node", "-sEXIT_RUNTIME=1", "-o", str(wasm)],
                       env=env, check=True)
        def compare(native_output, wasm_output):
            if native_output == wasm_output:
                return
            for number, (native_line, wasm_line) in enumerate(
                    zip_longest(native_output.splitlines(), wasm_output.splitlines()), 1):
                if native_line != wasm_line:
                    raise RuntimeError(
                        f"Traces differ at line {number}: "
                        f"native={native_line!r}, WASM={wasm_line!r}.")
            raise RuntimeError("Traces differ in their final line ending.")

        compare(subprocess.check_output([str(native)], text=True),
                subprocess.check_output([node, str(wasm)], env=env, text=True))

        fixture = bytes.fromhex((root / "tests/fixtures/tgb_v1_knob_dodge.hex").read_text())
        (Path(directory) / "trace_bundle.h").write_text(
            "static constexpr unsigned char trace_bundle[] = {"
            + ",".join(str(byte) for byte in fixture) + "};\n")
        lua_objects = {"native": [], "wasm": []}
        commands = []
        for target, c_compiler in (("native", cc), ("wasm", str(emcc))):
            for name in lua_source.SOURCE_FILES:
                output = Path(directory) / f"{target}-{name}.o"
                lua_objects[target].append(output)
                commands.append([c_compiler, "-std=c99", "-O2", "-ffp-contract=off",
                                 f"-I{lua}", "-c", str(lua / name), "-o", str(output)])
        with ThreadPoolExecutor(max_workers=8) as executor:
            list(executor.map(lambda command: subprocess.run(command, env=env, check=True),
                              commands))
        game_sources = [
            root / "engine/test/trace_lua_game.cpp",
            *(root / "engine" / name for name in (
                "bundle_script.cpp", "bundle_view.cpp", "draw_frame.cpp",
                "fixed_step_clock.cpp", "fixed_step_runner.cpp", "lua_runtime.cpp",
                "manifest_view.cpp")),
        ]
        game_flags = [*flags, f"-I{directory}", f"-I{lua}"]
        subprocess.run([compiler, *game_flags, *map(str, game_sources),
                        *map(str, lua_objects["native"]), "-o", str(native)], check=True)
        subprocess.run([str(emcc.with_name("em++")), *game_flags, *map(str, game_sources),
                        *map(str, lua_objects["wasm"]), "-sENVIRONMENT=node",
                        "-sEXIT_RUNTIME=1", "-o", str(wasm)], env=env, check=True)
        compare(subprocess.check_output([str(native)], text=True),
                subprocess.check_output([node, str(wasm)], env=env, text=True))
    print("Native and WebAssembly fixed-step and Lua game traces match.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"Trace check failed: {error}", file=sys.stderr)
        sys.exit(1)
