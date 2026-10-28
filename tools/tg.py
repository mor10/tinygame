#!/usr/bin/env python3
"""TinyGame helper: build, flash, test, and talk to the console over USB.

  python3 tools/tg.py build                 compile the firmware
  python3 tools/tg.py flash                 compile + upload to the board
  python3 tools/tg.py test                  run all unit tests on this computer
  python3 tools/tg.py monitor [-s 10]       print the board's log for N seconds
  python3 tools/tg.py send "turn left 4" click  send dev commands (see below)
  python3 tools/tg.py shot [file.png]       save a screenshot (default shot.png)
  python3 tools/tg.py new <id> "<NAME>"     create src/games/<id>/ from the template

Dev commands for `send`: turn <left|right> <signed-transitions> (-1000000..1000000),
click|hold|down|up [left|right] (default left),
tilt <x> <y>|off, launch <id>, info, log on|off, wait <seconds>.
System > Sleep or 3 minutes idle sleeps; either knob click wakes.
USB may reconnect after wake; serial cannot wake the board.
"""

import argparse
import base64
import binascii
import glob
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATE = os.path.join(ROOT, ".claude", "skills", "make-a-game", "template")
ESPRESSIF_USB_VID = 0x303A
CXXFLAGS = ["-std=c++17", "-Wall", "-Wextra", "-Werror"]
CXX_KEYWORDS = set("""
alignas alignof and and_eq asm auto bitand bitor bool break case catch char
char16_t char32_t class compl const constexpr const_cast continue decltype
default delete do double dynamic_cast else enum explicit export extern false
float for friend goto if inline int long mutable namespace new noexcept not
not_eq nullptr operator or or_eq private protected public register reinterpret_cast
return short signed sizeof static static_assert static_cast struct switch template
this thread_local throw true try typedef typeid typename union unsigned using
virtual void volatile wchar_t while xor xor_eq std
""".split())
# Files that (directly or via local headers) include these need the board or
# LVGL, so host tests skip them.
DEVICE_HEADERS = {"lvgl.h", "Arduino.h"}
INCLUDE = re.compile(r'#include\s*[<"]([^>"]+)[>"]')


def pio(*args):
    return subprocess.call([sys.executable, "-m", "platformio", *args], cwd=ROOT)


def find_port(explicit=None):
    if explicit:
        return explicit
    from serial.tools import list_ports

    ports = [p.device for p in list_ports.comports() if p.vid == ESPRESSIF_USB_VID]
    if not ports:
        raise RuntimeError("No TinyGame board found. Is the USB cable plugged in?")
    if len(ports) != 1:
        raise RuntimeError("Multiple boards found; select one with --port: " + ", ".join(ports))
    return ports[0]


def open_serial(explicit=None):
    import serial

    port = serial.Serial(find_port(explicit), 115200, timeout=0.1, write_timeout=3)
    port.reset_input_buffer()
    return port


def read_lines(port, seconds, stop=None):
    """Yields lines for `seconds`, or until stop(line) is true."""
    end = time.monotonic() + seconds
    buffer = b""
    while time.monotonic() < end:
        buffer += port.read(4096)
        if len(buffer) > 16384:
            raise RuntimeError("Serial response exceeded the line limit.")
        while b"\n" in buffer:
            line, buffer = buffer.split(b"\n", 1)
            text = line.decode(errors="replace").rstrip("\r")
            yield text
            if stop and stop(text):
                return


def cmd_build(_):
    return pio("run")


def cmd_flash(args):
    return pio("run", "-t", "upload", "--upload-port", find_port(args.port))


def cmd_monitor(args):
    with open_serial(args.port) as port:
        for line in read_lines(port, args.seconds):
            print(line, flush=True)
    return 0


def cmd_send(args):
    with open_serial(args.port) as port:
        for command in args.commands:
            if "\n" in command or "\r" in command:
                raise ValueError("Send one command per argument, without newlines.")
            if command.startswith("wait "):
                time.sleep(nonnegative_seconds(command[5:]))
                continue
            if command.strip() == "shot":
                raise ValueError("Use 'tg.py shot [file.png]' to save a screenshot.")
            port.write((command + "\n").encode())
            acknowledged = False
            done = lambda t: t == "[DEV] ok" or t.startswith("[DEV] error")
            for line in read_lines(port, 3, done):
                if line.startswith("[DEV] error"):
                    raise RuntimeError(line)
                if line.startswith("[DEV]"):
                    print(line)
                acknowledged |= line == "[DEV] ok"
            if not acknowledged:
                raise RuntimeError(f"No acknowledgement for {command!r}.")
            time.sleep(0.05)  # let input events reach separate game updates
    return 0


def write_png(path, width, height, rgb565_le):
    rows = []
    for y in range(height):
        row = bytearray([0])
        for x in range(width):
            (v,) = struct.unpack_from("<H", rgb565_le, (y * width + x) * 2)
            r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
            row += bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31))
        rows.append(bytes(row))

    def chunk(kind, data):
        payload = kind + data
        return struct.pack(">I", len(data)) + payload + struct.pack(">I", zlib.crc32(payload))

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(b"".join(rows), 9)))
        f.write(chunk(b"IEND", b""))


def cmd_shot(args):
    with open_serial(args.port) as port:
        port.write(b"shot\n")
        pixels = read_screenshot(read_lines(port, 15))
    write_png(args.file, 320, 172, pixels)
    print(f"Saved {args.file} (320x172)")
    return 0


def read_screenshot(lines):
    started = False
    pixels = bytearray()
    expected = 320 * 172 * 2
    for line in lines:
        if line.startswith("[DEV] error"):
            raise RuntimeError(line)
        if not started:
            if line.startswith("SHOT "):
                if line.split() != ["SHOT", "320", "172", "RGB565LE"]:
                    raise ValueError("Unsupported screenshot header.")
                started = True
            continue
        if line == "END":
            if len(pixels) != expected:
                raise ValueError(f"Screenshot incomplete ({len(pixels)} bytes).")
            return bytes(pixels)
        try:
            pixels.extend(base64.b64decode(line, validate=True))
        except binascii.Error as error:
            raise ValueError("Invalid screenshot pixel data.") from error
        if len(pixels) > expected:
            raise ValueError("Screenshot contains too many pixels.")
    raise RuntimeError("Screenshot timed out before its header or END marker.")


def nonnegative_seconds(value):
    seconds = float(value)
    if not math.isfinite(seconds) or seconds < 0:
        raise ValueError("Duration must be a finite, nonnegative number.")
    return seconds


def find_tests():
    """Each test_*.cpp is built with the host-safe .cpp files of its module."""
    jobs = []
    for test in sorted(glob.glob(os.path.join(ROOT, "tests", "test_*.cpp"))):
        jobs.append((test, os.path.join(ROOT, "src", "os")))
    for test in sorted(glob.glob(os.path.join(ROOT, "src", "games", "*", "test", "test_*.cpp"))):
        jobs.append((test, os.path.dirname(os.path.dirname(test))))
    return jobs


def needs_device(path, search_dirs, seen):
    if path in seen:
        return False
    seen.add(path)
    with open(path, encoding="utf-8") as f:
        for header in INCLUDE.findall(f.read()):
            if header in DEVICE_HEADERS:
                return True
            for folder in [os.path.dirname(path), *search_dirs]:
                local = os.path.join(folder, header)
                if os.path.isfile(local) and needs_device(local, search_dirs, seen):
                    return True
    return False


def host_safe_sources(module):
    search = [module, os.path.join(ROOT, "include"), os.path.join(ROOT, "src", "hal")]
    return [path for path in sorted(glob.glob(os.path.join(module, "*.cpp")))
            if not needs_device(path, search, set())]


def cmd_test(_):
    compiler = os.environ.get("CXX", "c++")
    failed = 0
    sources = {}
    with tempfile.TemporaryDirectory() as tmp:
        for test, module in find_tests():
            name = os.path.relpath(test, ROOT)
            exe = os.path.join(tmp, "t")
            includes = ["-I", os.path.join(ROOT, "include"), "-I", module,
                        "-I", os.path.join(ROOT, "src", "hal")]
            if module not in sources:
                sources[module] = host_safe_sources(module)
            build = [compiler, *CXXFLAGS, *includes, test, *sources[module], "-o", exe]
            if subprocess.call(build) != 0 or subprocess.call([exe]) != 0:
                print(f"FAIL {name}")
                failed += 1
            else:
                print(f"ok   {name}")
    failed += subprocess.call(
        [sys.executable, "-B", "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py"],
        cwd=ROOT,
    ) != 0
    print("all tests passed" if failed == 0 else f"{failed} test file(s) failed")
    return 1 if failed else 0


def cmd_new(args):
    game_id = args.id
    if (not re.fullmatch(r"[a-z][a-z0-9_]{0,14}", game_id)
            or game_id in CXX_KEYWORDS or "__" in game_id):
        raise ValueError("Game id: lowercase letters, digits, _; start with a letter; "
                         "max 15. No C++ keywords or double underscores.")
    name = args.name
    if not name.strip() or len(name) > 13 or any(ord(c) < 32 or ord(c) > 126 for c in name):
        raise ValueError("Game name: 1-13 printable ASCII characters, not just spaces.")
    name = name.upper()
    target = os.path.join(ROOT, "src", "games", game_id)
    if os.path.exists(target):
        raise ValueError(f"{os.path.relpath(target, ROOT)} already exists.")
    class_name = "".join(part.capitalize() for part in game_id.split("_")) or "MyGame"
    shutil.copytree(TEMPLATE, target)
    for path in glob.glob(os.path.join(target, "**", "*"), recursive=True):
        if os.path.isfile(path):
            with open(path, encoding="utf-8") as f:
                text = f.read()
            values = {"__ID__": game_id, "__CLASS__": class_name,
                      "__NAME__": json.dumps(name)[1:-1] if path.endswith(".cpp") else name}
            text = re.sub(r"__ID__|__NAME__|__CLASS__", lambda m: values[m[0]], text)
            with open(path, "w", encoding="utf-8") as f:
                f.write(text)
    print(f"Created src/games/{game_id}/. Next: python3 tools/tg.py build")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("build").set_defaults(run=cmd_build)
    p = sub.add_parser("flash")
    p.add_argument("--port", help="explicit USB serial device")
    p.set_defaults(run=cmd_flash)
    sub.add_parser("test").set_defaults(run=cmd_test)
    p = sub.add_parser("monitor")
    p.add_argument("-s", "--seconds", type=nonnegative_seconds, default=10)
    p.add_argument("--port")
    p.set_defaults(run=cmd_monitor)
    p = sub.add_parser("send")
    p.add_argument("commands", nargs="+")
    p.add_argument("--port")
    p.set_defaults(run=cmd_send)
    p = sub.add_parser("shot")
    p.add_argument("file", nargs="?", default="shot.png")
    p.add_argument("--port")
    p.set_defaults(run=cmd_shot)
    p = sub.add_parser("new")
    p.add_argument("id")
    p.add_argument("name")
    p.set_defaults(run=cmd_new)
    args = parser.parse_args()
    try:
        result = args.run(args)
    except ModuleNotFoundError as error:
        parser.exit(1, f"Missing Python dependency: {error.name}. See README setup instructions.\n")
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"Error: {error}\n")
    sys.exit(result)


if __name__ == "__main__":
    main()
