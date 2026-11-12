import os
from pathlib import Path
import csv
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BoardProfileTests(unittest.TestCase):
    def test_both_profile_headers_and_gpio_banks(self):
        compiler = os.environ.get("CXX", "c++")
        source = ROOT / "tests/fixtures/board_profile_test.cpp"
        with tempfile.TemporaryDirectory() as tmp:
            for profile in ("147", "169"):
                with self.subTest(profile=profile):
                    executable = Path(tmp) / f"profile-{profile}"
                    result = subprocess.run(
                        [
                            compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                            f"-DTG_BOARD={profile}", "-I", str(ROOT / "include"),
                            "-I", str(ROOT / "src/hal"), str(source), "-o", str(executable),
                        ],
                        capture_output=True, text=True, check=False,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)
                    result = subprocess.run(
                        [str(executable)], capture_output=True, text=True, check=False,
                    )
                    self.assertEqual(result.returncode, 0, result.stderr)

    def test_partition_table_preserves_ota_slots_and_uses_existing_data_region(self):
        path = ROOT / "partitions/tinygame_16mb.csv"
        with path.open(newline="", encoding="utf-8") as source:
            rows = list(csv.reader(line for line in source if not line.lstrip().startswith("#")))
        entries = {
            row[0].strip(): (row[1].strip(), row[2].strip(),
                             int(row[3].strip(), 0), int(row[4].strip(), 0))
            for row in rows if row and row[0].strip()
        }
        self.assertEqual(entries["app0"], ("app", "ota_0", 0x10000, 0x640000))
        self.assertEqual(entries["app1"], ("app", "ota_1", 0x650000, 0x640000))
        self.assertEqual(entries["littlefs"], ("data", "0x83", 0xC90000, 0x360000))
        self.assertEqual(entries["coredump"], ("data", "coredump", 0xFF0000, 0x10000))
        ordered = sorted((offset, offset + size, name)
                         for name, (_, _, offset, size) in entries.items())
        self.assertTrue(all(left[1] <= right[0] for left, right in zip(ordered, ordered[1:])))
        self.assertEqual(ordered[-1][1], 0x1000000)


if __name__ == "__main__":
    unittest.main()
