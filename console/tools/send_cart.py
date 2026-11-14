#!/usr/bin/env python3
"""Send a .lua cart to the TinyTwist console over USB serial and print its JSON output.

Usage: send_cart.py PORT CART.lua [--seconds N] [--raw-bytes HEX]
Opening the port resets the board, so the script waits for the boot line first.
Needs pyserial (the ESP-IDF Python environment has it).
"""
import argparse, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("port")
ap.add_argument("cart", nargs="?")
ap.add_argument("--seconds", type=float, default=4.0, help="how long to print output after sending")
ap.add_argument("--timestamps", action="store_true", help="prefix each line with ms since the cart was sent")
ap.add_argument("--raw-bytes", help="send these hex bytes as the cart instead of a file (for tests)")
a = ap.parse_args()

if a.raw_bytes:
    data = bytes.fromhex(a.raw_bytes)
elif a.cart:
    data = open(a.cart, "rb").read()
else:
    sys.exit("give a cart file or --raw-bytes")

p = serial.Serial(a.port, 115200, timeout=0.1)
p.dtr = False
p.rts = False
buf, t_end = b"", time.time() + 6
while b'"t":"boot"' not in buf and time.time() < t_end:
    buf += p.read(4096)
time.sleep(0.8)              # let the built-in cart start
p.read(65536)
p.write(b"cart %d\n" % len(data))
p.write(data)
t_sent = time.time()
t_end = t_sent + a.seconds
pending = b""
while time.time() < t_end:
    pending += p.read(4096)
    *lines, pending = pending.split(b"\n")
    for line in lines:
        line = line.strip()
        if line.startswith(b"{"):
            prefix = "%6d ms  " % ((time.time() - t_sent) * 1000) if a.timestamps else ""
            print(prefix + line.decode(errors="replace"), flush=True)
