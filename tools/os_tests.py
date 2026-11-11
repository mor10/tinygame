#!/usr/bin/env python3
"""Host tests for the firmware's pure logic (hal/ and os/).

  python3 tools/os_tests.py

Builds and runs every hal/test/test_*.cpp and os/test/test_*.cpp with the same
compiler flags as `tg.py test`, plus the platform-free sources in os/ that the
tests link (settings.cpp, status_light.cpp). Nothing here touches Arduino or
the board. To fold this into `tg test`, call run_all() from tg.py's cmd_test.
"""

import importlib.util
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_tg():
    spec = importlib.util.spec_from_file_location("tg_tool", ROOT / "tools" / "tg.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# Portable sources the tests link. The drivers (display, input.cpp, ...) include
# Arduino headers and are only built by PlatformIO.
LOGIC_SOURCES = [ROOT / "os" / "settings.cpp", ROOT / "os" / "status_light.cpp"]
INCLUDES = [f"-I{ROOT / 'hal'}", f"-I{ROOT / 'os'}"]


def run_all():
    tg = load_tg()
    tests = sorted([*(ROOT / "hal" / "test").glob("test_*.cpp"),
                    *(ROOT / "os" / "test").glob("test_*.cpp")])
    if not tests:
        print("no tests found")
        return 1
    out = tg.BUILD / "os_tests"
    out.mkdir(parents=True, exist_ok=True)
    # The engine is linked into every test (the demo scene tests need it); its
    # objects come from tg.py's cache, so this is cheap after `tg.py test`.
    headers = tg.header_digest()
    engine_objects = [tg.compile_object(s, headers) for s in tg.engine_sources()]
    failed = []
    for test in tests:
        name = str(test.relative_to(ROOT))
        print(name)
        exe = out / test.stem
        command = [tg.compiler(), *tg.CXXFLAGS, *INCLUDES,
                   str(test), *map(str, LOGIC_SOURCES), *map(str, engine_objects),
                   "-o", str(exe)]
        if None in engine_objects or subprocess.run(command).returncode != 0:
            print("  build failed")
            failed.append(name)
        elif subprocess.run([str(exe)]).returncode != 0:
            failed.append(name)
    if failed:
        print(f"FAILED: {len(failed)} of {len(tests)} test files: " + ", ".join(failed))
        return 1
    print(f"all {len(tests)} os/hal test files passed")
    return 0


if __name__ == "__main__":
    sys.exit(run_all())
