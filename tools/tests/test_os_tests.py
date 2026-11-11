"""tools/os_tests.py must fail when a test program does not build or link."""

import contextlib
import importlib.util
import io
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("os_tests_tool", ROOT / "tools" / "os_tests.py")
os_tests = importlib.util.module_from_spec(spec)
spec.loader.exec_module(os_tests)


def run(sources):
    with tempfile.TemporaryDirectory() as folder:
        paths = []
        for name, text in sources.items():
            path = Path(folder) / name
            path.write_text(text)
            paths.append(path)
        with contextlib.redirect_stdout(io.StringIO()) as out:
            code = os_tests.run_all(paths)
        return code, out.getvalue()


class OsTestsExitCode(unittest.TestCase):
    def test_a_passing_program_passes(self):
        code, _ = run({"test_ok.cpp": "int main() { return 0; }\n"})
        self.assertEqual(code, 0)

    def test_a_failing_program_fails(self):
        code, _ = run({"test_bad.cpp": "int main() { return 1; }\n"})
        self.assertEqual(code, 1)

    def test_a_compile_error_fails(self):
        code, out = run({"test_syntax.cpp": "int main() { return ; ;; this is not C++\n"})
        self.assertEqual(code, 1)
        self.assertIn("build failed", out)

    def test_a_link_error_fails(self):
        code, out = run({"test_link.cpp": "int missing();\nint main() { return missing(); }\n"})
        self.assertEqual(code, 1)
        self.assertIn("build failed", out)

    def test_one_broken_build_fails_the_whole_run_even_after_passing_ones(self):
        code, _ = run({"test_a.cpp": "int main() { return 0; }\n",
                       "test_b.cpp": "int missing();\nint main() { return missing(); }\n"})
        self.assertEqual(code, 1)


if __name__ == "__main__":
    unittest.main()
