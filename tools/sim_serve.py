#!/usr/bin/env python3
"""Build the browser simulator and serve it on this computer.

  python3 tools/sim_serve.py [bundle-file]     build, start the server, open the page
  python3 tools/sim_serve.py --no-open         do not open a browser
  python3 tools/sim_serve.py --port 8080       use this port (default: any free port)
  python3 tools/sim_serve.py --build-only      build the WebAssembly and stop

The bundle file may not exist yet. When it changes the page reloads it; the
WebAssembly is not rebuilt. Press Ctrl+C to stop.
"""

import argparse
import shutil
import subprocess
import sys
import webbrowser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("bundle", nargs="?", help="game bundle file to watch (may not exist yet)")
    parser.add_argument("--port", type=int, default=0)
    parser.add_argument("--no-open", action="store_true", help="do not open a browser")
    parser.add_argument("--build-only", action="store_true", help="build and stop")
    args = parser.parse_args()
    if not 0 <= args.port <= 65535:
        parser.error("--port must be from 0 to 65535")

    built = subprocess.run([sys.executable, str(ROOT / "tools" / "sim_build.py")])
    if built.returncode != 0:
        return built.returncode
    if args.build_only:
        return 0
    node = shutil.which("node")
    if node is None:
        print("Node.js (version 22 or newer) is needed to serve the simulator.", file=sys.stderr)
        return 1

    command = [node, str(ROOT / "sim" / "server.mjs"), "--port", str(args.port)]
    if args.bundle:
        command.append(args.bundle)
    server = subprocess.Popen(command, stdout=subprocess.PIPE, text=True)
    try:
        url = server.stdout.readline().strip()
        if not url.startswith("http://"):
            print("The simulator server did not start.", file=sys.stderr)
            return server.wait() or 1
        print(f"Simulator: {url}", flush=True)
        if not args.no_open:
            webbrowser.open(url)
        for line in server.stdout:
            print(line, end="", flush=True)
        return server.wait()
    except KeyboardInterrupt:
        return 0
    finally:
        if server.poll() is None:
            server.terminate()
            server.wait()


if __name__ == "__main__":
    sys.exit(main())
