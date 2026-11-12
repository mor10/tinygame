"""Compile-check bounded Lua text without running it."""

import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from lua_source import CACHE, REVISION, SOURCE_FILES, source_dir

SOURCE_CHECK_TIMEOUT_SECONDS = 20


def _parser():
    compiler = shutil.which("cc")
    if compiler is None:
        raise RuntimeError("Lua syntax checking requires a C compiler (cc).")
    helper = Path(__file__).with_name("lua_syntax.c")
    version_result = subprocess.run([compiler, "--version"], capture_output=True)
    if version_result.returncode:
        raise RuntimeError("Could not identify the C compiler for Lua syntax checking.")
    version = version_result.stdout
    key = hashlib.sha256(helper.read_bytes() + REVISION.encode() +
                         os.fsencode(compiler) + version).hexdigest()[:20]
    directory = CACHE / "lua-check"
    directory.mkdir(parents=True, exist_ok=True)
    binary = directory / f"parser-{key}"
    if binary.is_file():
        return binary

    lua = source_dir()
    with tempfile.TemporaryDirectory(prefix="build-", dir=directory) as tmp:
        output = Path(tmp) / "parser"
        command = [compiler, "-O0", "-I", str(lua), str(helper),
                   *(str(lua / source) for source in SOURCE_FILES), "-lm", "-o", str(output)]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(f"Could not build the Lua syntax checker:\n{result.stderr.strip()}")
        os.replace(output, binary)
    return binary


def check_sources(files):
    sources = [(name, content) for name, content in sorted(files.items())
               if name.endswith(".lua")]
    if not sources:
        return
    parser = _parser()
    deadline = time.monotonic() + SOURCE_CHECK_TIMEOUT_SECONDS
    for name, content in sources:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise RuntimeError(f"{name}: Lua syntax check timed out.")
        try:
            result = subprocess.run([str(parser), f"@{name}"], input=content,
                                    capture_output=True, timeout=remaining)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"{name}: Lua syntax check timed out.") from error
        if result.returncode:
            message = result.stderr.decode("utf-8", errors="replace").strip()
            raise RuntimeError(f"{name}: Lua syntax check failed: {message or 'parser failed'}")
