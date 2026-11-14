#!/usr/bin/env python3
"""Run a cart on the board and in the simulator and compare their frame hashes.

Usage: compare.py PORT CART.lua [--frames N] [--seed S]
The board run uses the console's `hash` command (deterministic, no input); the simulator runs
sim/build/tinygame-sim with the same frames and seed. Exit 0 when every hash matches.
Run it with the ESP-IDF Python environment (it needs pyserial).
"""
import argparse, json, os, subprocess, sys, time
import serial

ap = argparse.ArgumentParser()
ap.add_argument("port")
ap.add_argument("cart")
ap.add_argument("--frames", type=int, default=300)
ap.add_argument("--seed", type=int, default=0)
a = ap.parse_args()

sim = os.path.join(os.path.dirname(os.path.abspath(__file__)), "build", "tinygame-sim")
out = subprocess.run([sim, a.cart, "--frames", str(a.frames), "--seed", str(a.seed)], capture_output=True, text=True).stdout
sim_hash = {j["frame"]: j["fnv"] for j in map(json.loads, out.splitlines()) if j["t"] == "hash"}

data = open(a.cart, "rb").read()
p = serial.Serial(a.port, 115200, timeout=0.1)
p.dtr = False
p.rts = False
buf, t_end = b"", time.time() + 6
while b'"t":"boot"' not in buf and time.time() < t_end:
    buf += p.read(4096)
time.sleep(0.8)
p.read(65536)
p.write(b"cart %d\n" % len(data) + data)
time.sleep(1.0)
p.write(b"hash %d %d\n" % (a.frames, a.seed))
dev_hash, pending, t_end = {}, b"", time.time() + a.frames / 20 + 10
while time.time() < t_end and a.frames not in dev_hash:
    pending += p.read(4096)
    *lines, pending = pending.split(b"\n")
    for line in lines:
        if line.startswith(b'{"t":"hash"'):
            j = json.loads(line)
            dev_hash[j["frame"]] = j["fnv"]
        elif line.startswith(b'{"t":"error"'):
            print("board:", line.decode(errors="replace")[:200])

frames = sorted(set(sim_hash) | set(dev_hash))
bad = [f for f in frames if sim_hash.get(f) != dev_hash.get(f)]
for f in frames:
    mark = "" if f not in bad else "   <-- differs"
    print("frame %4d  board %s  sim %s%s" % (f, dev_hash.get(f, "-" * 8), sim_hash.get(f, "-" * 8), mark))
print("MATCH" if not bad else "MISMATCH at %d of %d frames" % (len(bad), len(frames)))
sys.exit(0 if not bad else 1)
