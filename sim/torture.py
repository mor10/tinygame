#!/usr/bin/env python3
"""Reset the board at random moments while a cart is saving, and check every save survives.

Usage: torture.py PORT [--cycles 50] [--cart console/carts/tests/save_torture.lua]
Uploads the torture cart as `save_torture`, then repeats: reset the board (opening the serial
port resets it), `saveflush 0` so the save is rewritten every frame, `play save_torture`, read
its start-up check, let it run 0.3-2.5 s, and reset again mid-run, so resets land mid-write. Passes when every start logs "torture ok"
and no save is reported damaged. A reset is a stand-in for a power cut: it stops the chip
mid-write, but the flash chip itself stays powered.
"""
import argparse, random, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("port")
ap.add_argument("--cycles", type=int, default=50)
ap.add_argument("--cart", default="console/carts/tests/save_torture.lua")
ap.add_argument("--flush-ms", type=int, default=0, help="save write interval during the test (0 = every frame)")
a = ap.parse_args()

def open_board():
    p = serial.Serial(a.port, 115200, timeout=0.1)
    p.dtr = False
    p.rts = False
    buf, t_end = b"", time.time() + 6
    while b'"id":"hello"' not in buf and time.time() < t_end:
        buf += p.read(4096)
    return p, buf

def read_for(p, seconds, until=None):
    buf, t_end = b"", time.time() + seconds
    while time.time() < t_end:
        buf += p.read(4096)
        if until and until in buf:
            break
    return buf

p, _ = open_board()
data = open(a.cart, "rb").read()
p.write(b"cart %d save_torture\n" % len(data) + data)
read_for(p, 2.0)
p.close()

ok = bad = damaged = 0
counts = []
for i in range(1, a.cycles + 1):
    p, boot = open_board()
    p.write(b"saveflush %d\nplay save_torture\n" % a.flush_ms)
    out = boot + read_for(p, 3.0, b"torture ")
    if b"was damaged" in out:
        damaged += 1
    line = next((l for l in out.split(b"\n") if b"torture " in l), b"")
    if b"torture ok" in line:
        ok += 1
        counts.append(int(line.split(b"n=")[1].split(b'"')[0]))
    else:
        bad += 1
        print("cycle %d: %s" % (i, line.decode(errors="replace") or "no start-up line"))
    time.sleep(random.uniform(0.3, 2.5))   # reset lands at a random point in the save cycle
    p.close()
    print("cycle %2d: %s n=%s" % (i, "ok " if b"torture ok" in line else "BAD", counts[-1] if counts else "-"), flush=True)

print("cycles %d, ok %d, bad %d, damaged %d, counter %s -> %s" % (a.cycles, ok, bad, damaged, counts[:1], counts[-1:]))
sys.exit(0 if bad == 0 and damaged == 0 else 1)
