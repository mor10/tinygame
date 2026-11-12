"""Build and run the loopback simulator without extra Python dependencies."""
import os
import shutil
import subprocess
import sys
import webbrowser

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def cmd_sim(args):
    board = getattr(args, "board", "147b")
    result = subprocess.call([sys.executable, os.path.join(ROOT, "tools", "sim_build.py"),
                              "--board", board], cwd=ROOT)
    if result or args.build_only:
        return result
    node = shutil.which("node")
    if not node:
        raise RuntimeError("The simulator server needs Node.js 22 or newer.")
    if not 0 <= args.port <= 65535:
        raise ValueError("Simulator HTTP port must be 0..65535.")
    env = dict(os.environ, PYTHON=sys.executable)
    child = subprocess.Popen(
        [node, os.path.join(ROOT, "sim", "server.mjs"), "--port", str(args.port),
         "--board", board],
        cwd=ROOT, env=env, stdout=subprocess.PIPE, text=True,
    )
    try:
        url = child.stdout.readline().strip()
        if not url.startswith("http://127.0.0.1:"):
            raise RuntimeError("Simulator server failed to start.")
        print("TinyGame simulator: " + url, flush=True)
        print("Press Ctrl+C to stop. Source edits rebuild automatically.", flush=True)
        if not args.no_open:
            webbrowser.open(url)
        for line in child.stdout:
            print(line, end="", flush=True)
        return child.wait()
    except KeyboardInterrupt:
        child.terminate()
        child.wait(timeout=10)
        return 0
    finally:
        if child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
