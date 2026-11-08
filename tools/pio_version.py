from pathlib import Path
import sys

Import("env")

root = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(root / "tools"))
from os_version import write_header

generated = Path(env.subst("$BUILD_DIR")) / "generated"
write_header(root, generated / "tg_os_version.h")
env.Append(CPPPATH=[str(generated)])
