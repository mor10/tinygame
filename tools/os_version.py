"""Reproducible OS version shared by firmware and simulator builds."""

import hashlib
from pathlib import Path


def version(root: Path) -> str:
    # Games and documentation do not change the OS version. No Git checkout or
    # manually maintained release number is needed, including for source archives.
    files = [root / name for name in (
        "src/main.cpp", "platformio.ini", "tools/os_version.py", "tools/pio_version.py",
    )]
    for folder in ("include", "src/os", "src/hal"):
        files.extend(path for path in (root / folder).rglob("*") if path.is_file())
    digest = hashlib.sha256()
    for path in sorted(files):
        digest.update(path.relative_to(root).as_posix().encode() + b"\0")
        digest.update(path.read_bytes() + b"\0")
    return digest.hexdigest()[:12]


def write_header(root: Path, destination: Path) -> None:
    content = ('#pragma once\nnamespace os_version {\n'
               f'inline constexpr char LABEL[] = "OS {version(root)}";\n'
               '}\n')
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text() != content:
        destination.write_text(content)
