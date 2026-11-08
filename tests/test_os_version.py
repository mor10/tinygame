import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("os_version", ROOT / "tools/os_version.py")
os_version = importlib.util.module_from_spec(spec)
spec.loader.exec_module(os_version)


class OsVersionTests(unittest.TestCase):
    def test_version_tracks_os_not_games_and_is_reproducible(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ("src/main.cpp", "platformio.ini", "tools/os_version.py",
                         "tools/pio_version.py", "include/api.h", "src/os/os.cpp",
                         "src/hal/input.cpp", "src/games/example/game.cpp"):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(name)
            first = os_version.version(root)
            self.assertRegex(first, r"^[0-9a-f]{12}$")
            (root / "src/games/example/game.cpp").write_text("new game")
            self.assertEqual(os_version.version(root), first)
            for name in ("src/os/os.cpp", "src/hal/input.cpp", "include/api.h"):
                path = root / name
                original = path.read_text()
                path.write_text("changed")
                self.assertNotEqual(os_version.version(root), first)
                path.write_text(original)
                self.assertEqual(os_version.version(root), first)
            output = root / "build/tg_os_version.h"
            os_version.write_header(root, output)
            self.assertIn(f'"OS {first}"', output.read_text())
            modified = output.stat().st_mtime_ns
            os_version.write_header(root, output)
            self.assertEqual(output.stat().st_mtime_ns, modified)
            other = root / "sim-build/tg_os_version.h"
            os_version.write_header(root, other)
            self.assertEqual(output.read_bytes(), other.read_bytes())


if __name__ == "__main__":
    unittest.main()
