#!/usr/bin/env bash
set -euxo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

source_paths=(pnx_devices pnx_modules demo)

if rg -n \
    '\bHAL_[A-Za-z0-9_]+|\b(hcan|hfdcan|huart)[0-9]+\b|PNX_BOARD_|DJI_C_BOARD|STM32F407' \
    "${source_paths[@]}" \
    --glob '*.c' --glob '*.cpp' --glob '*.h' --glob '*.hpp'
then
    echo "Board boundary violation found in device/module/demo source." >&2
    exit 1
fi

if find boards/dji_c_board_f407 -type d \
    \( -name User -o -name Task -o -name Service \) -print |
    grep -q .
then
    echo "Legacy User/Task/Service directory copied into F407 board tree." >&2
    exit 1
fi

if [[ -e configs/generated/config.hpp ||
      -e configs/generated/robot_config.hpp ]]
then
    echo "Source-tree generated configuration exists." >&2
    exit 1
fi

if [[ -n "$(git ls-files configs/generated)" ]]
then
    echo "Generated configuration is tracked in the source tree." >&2
    exit 1
fi

echo "Board boundary checks passed."
