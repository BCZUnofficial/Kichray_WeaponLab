#!/usr/bin/env python3
"""Build and run portable production-code checks with C++17; no CMake needed."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess
p = argparse.ArgumentParser()
p.add_argument("--sanitize", action="store_true")
p.add_argument("--build-dir", default="build-portable")
args = p.parse_args()
root = Path(__file__).resolve().parent
out = Path(args.build_dir).resolve()
out.mkdir(parents=True, exist_ok=True)
compiler = shlex.split(os.environ.get("CXX", "g++"))
flags = ["-std=c++17", "-O1", "-g", "-UNDEBUG", "-Isrc", "-Ithird_party/imgui"]
if args.sanitize:
    flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
sources = ["src/Core.cpp", "src/DDS.cpp", "src/OGF.cpp", "src/WeaponData.cpp", "src/IconAtlas.cpp"]
ui_sources = ["src/WeaponUI.cpp"] + [f"third_party/imgui/{s}.cpp" for s in
    ["imgui", "imgui_draw", "imgui_tables", "imgui_widgets"]]
tests = {
    "viewport-tests": "ViewportToolsTests.cpp",
    "parser-tests": "ParserTests.cpp",
    "flight-input-tests": "FlightInputTests.cpp",
    "renderer-preview-tests": "RendererPreviewTests.cpp",
    "weapon-data-tests": "WeaponDataTests.cpp",
    "icon-atlas-tests": "IconAtlasTests.cpp",
    "weapon-ui-tests": "WeaponUITests.cpp",
}
log = []
def run(command):
    print(" ".join(map(str, command)), flush=True)
    result = subprocess.run(command, cwd=root, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=240)
    log.append({"command": list(map(str, command)), "code": result.returncode,
                "output": result.stdout})
    if result.stdout:
        print(result.stdout, end="", flush=True)
    (out / "check-log.json").write_text(json.dumps(log, ensure_ascii=False, indent=2))
    if result.returncode:
        raise SystemExit(result.returncode)
objects = {}
for source in sources + ui_sources:
    obj = out / (source.replace("/", "_") + ".o")
    run(compiler + flags + ["-c", source, "-o", str(obj)])
    objects[source] = str(obj)
for name, test in tests.items():
    exe = out / name
    inputs = [objects[s] for s in sources]
    if name == "weapon-ui-tests":
        inputs += [objects[s] for s in ui_sources]
    run(compiler + flags + [str(root / "tests" / test)] + inputs + ["-o", str(exe)])
    run([str(exe)])
print(f"All {len(tests)} portable test suites passed.")
