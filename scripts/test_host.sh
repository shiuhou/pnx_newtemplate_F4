#!/usr/bin/env bash
set -euxo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

resolve_host_cxx() {
    if [[ -n "${PNX_HOST_CXX:-}" ]]; then
        if [[ -x "${PNX_HOST_CXX}" ]]; then
            printf '%s\n' "${PNX_HOST_CXX}"
            return
        fi
        command -v "${PNX_HOST_CXX}"
        return
    fi

    # Git Bash may resolve the extensionless shim first. Windows CMake treats
    # that absolute path literally and does not append .exe, so prefer the
    # physical executable when it is available.
    if command -v x86_64-w64-mingw32-clang++.exe >/dev/null 2>&1; then
        command -v x86_64-w64-mingw32-clang++.exe
        return
    fi
    if command -v x86_64-w64-mingw32-clang++ >/dev/null 2>&1; then
        command -v x86_64-w64-mingw32-clang++
        return
    fi

    if [[ -n "${LOCALAPPDATA:-}" ]] && command -v cygpath >/dev/null 2>&1; then
        local local_app_data
        local_app_data="$(cygpath -u "${LOCALAPPDATA}")"
        local compiler
        shopt -s nullglob
        for compiler in \
            "${local_app_data}"/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.*/llvm-mingw-*/bin/x86_64-w64-mingw32-clang++.exe
        do
            if [[ -x "${compiler}" ]]; then
                printf '%s\n' "${compiler}"
                shopt -u nullglob
                return
            fi
        done
        shopt -u nullglob
    fi

    if command -v g++ >/dev/null 2>&1; then
        command -v g++
        return
    fi
    if command -v clang++ >/dev/null 2>&1; then
        command -v clang++
        return
    fi
    printf '%s\n' \
        'No native C++17 compiler found. Set PNX_HOST_CXX.' >&2
    return 1
}

host_cxx="$(resolve_host_cxx)"
export PATH="$(dirname "${host_cxx}"):${PATH}"
cd "${repo_root}"

cmake --fresh --preset host-tests "-DCMAKE_CXX_COMPILER=${host_cxx}"
cmake --build --preset host-tests
ctest --preset host-tests --output-on-failure
