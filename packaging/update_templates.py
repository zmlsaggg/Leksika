#!/usr/bin/env python3
"""Fill release templates from verified SHA-256 sums. No network or credentials."""
import json
import re
import sys
from pathlib import Path

version, checksums, output = sys.argv[1:]
if not re.fullmatch(r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)", version):
    raise SystemExit("A stable X.Y.Z version is required")
sums = {}
for line in Path(checksums).read_text(encoding="utf-8").splitlines():
    match = re.fullmatch(r"([0-9a-f]{64})  (leksika-native-[a-z0-9-]+\.(?:zip|tar\.gz))", line)
    if not match or match[2] in sums:
        raise SystemExit("Invalid checksums.txt")
    sums[match[2]] = match[1]
files = ["leksika-native-windows-x64.zip", "leksika-native-macos-arm64.tar.gz",
         "leksika-native-macos-x64.tar.gz", "leksika-native-linux-x64.tar.gz"]
if any(name not in sums for name in files):
    raise SystemExit("Missing archive checksum")
root = Path(__file__).resolve().parent
out = Path(output)
out.mkdir(parents=True, exist_ok=True)
manifest = json.loads((root / "scoop/leksus-core.json").read_text(encoding="utf-8"))
manifest["version"] = version
win = manifest["architecture"]["64bit"]
win["url"] = win["url"].replace("v0.0.0", "v" + version)
win["hash"] = sums[files[0]]
(out / "leksus-core.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
formula = (root / "homebrew/leksus-core.rb").read_text(encoding="utf-8")
formula = formula.replace('version "0.0.0"', f'version "{version}"')
for name in files[1:]:
    formula = formula.replace('sha256 "' + '0' * 64 + '"', f'sha256 "{sums[name]}"', 1)
(out / "leksus-core.rb").write_text(formula, encoding="utf-8")
