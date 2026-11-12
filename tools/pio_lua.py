Import("env")

from pathlib import Path
import sys


ROOT = Path(env.subst("$PROJECT_DIR"))
sys.path.insert(0, str(ROOT / "tools"))
from lua_source import SOURCE_FILES, source_dir


if not env["PIOENV"].startswith("test-card-"):
    lua = source_dir()
    env.Append(CPPPATH=[str(lua), str(ROOT / "engine")])
    build = Path(env.subst("$BUILD_DIR"))
    env.BuildSources(
        str(build / "lua"),
        str(lua),
        src_filter=[f"+<{name}>" for name in SOURCE_FILES],
    )
    env.BuildSources(
        str(build / "engine-v2"),
        str(ROOT / "engine"),
        src_filter=["+<bundle_script.cpp>", "+<bundle_view.cpp>", "+<draw_frame.cpp>",
                    "+<manifest_view.cpp>",
                    "+<fixed_step_clock.cpp>", "+<fixed_step_runner.cpp>",
                    "+<lua_runtime.cpp>"],
    )
