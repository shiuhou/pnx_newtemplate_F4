#!/usr/bin/env bash
set -euxo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

preset_output="$(
    python scripts/check_cubemx_production.py --list-f407-presets | tr -d '\r'
)"
mapfile -t presets <<< "${preset_output}"
test "${#presets[@]}" -gt 0

for preset in "${presets[@]}"
do
    cmake --fresh --preset "${preset}"
    cmake --build --preset "${preset}"
done
