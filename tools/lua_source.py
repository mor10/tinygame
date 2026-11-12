"""Pinned Lua 5.4 source shared by native, firmware, and WASM builds."""

from pathlib import Path
import subprocess


VERSION = "5.4.9"
REVISION = "312b9efaa1061c2c4cad08554dbc1351c3270eef"
REPOSITORY = "https://github.com/lua/lua.git"
CACHE = Path.home() / ".cache" / "tinygame-sim"
SOURCE_FILES = (
    "lapi.c", "lcode.c", "lctype.c", "ldebug.c", "ldo.c", "ldump.c",
    "lfunc.c", "lgc.c", "llex.c", "lmem.c", "lobject.c", "lopcodes.c",
    "lparser.c", "lstate.c", "lstring.c", "ltable.c", "ltm.c", "lundump.c",
    "lvm.c", "lzio.c", "lauxlib.c", "lbaselib.c", "lmathlib.c", "lstrlib.c",
    "ltablib.c",
)


def source_dir() -> Path:
    source = CACHE / f"lua-{VERSION}"
    CACHE.mkdir(parents=True, exist_ok=True)
    if not source.exists():
        subprocess.run(
            ["git", "-c", "advice.detachedHead=false", "clone", "--quiet",
             "--depth", "1", "--branch", f"v{VERSION}", REPOSITORY, str(source)],
            check=True,
        )
    revision = subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
    ).strip()
    if revision != REVISION:
        raise RuntimeError(f"Cached Lua source is not the pinned {VERSION} revision")
    missing = [name for name in SOURCE_FILES if not (source / name).is_file()]
    if missing:
        raise RuntimeError(f"Pinned Lua source is missing files: {', '.join(missing)}")
    return source
