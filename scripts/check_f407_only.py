#!/usr/bin/env python3
"""Fail-closed static checks for an exported F407-only candidate."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import urlsplit

import f407_export


REQUIRED_LICENSES = (
    "boards/dji_c_board_f407/Drivers/CMSIS/Device/ST/STM32F4xx/LICENSE.md",
    "boards/dji_c_board_f407/Drivers/CMSIS/LICENSE.txt",
    "boards/dji_c_board_f407/Drivers/STM32F4xx_HAL_Driver/LICENSE.md",
    "boards/dji_c_board_f407/Middlewares/ST/threadx/LICENSE.txt",
    "boards/dji_c_board_f407/Middlewares/ST/threadx/LICENSED-HARDWARE.txt",
    "boards/dji_c_board_f407/Middlewares/ST/usbx/LICENSE.txt",
    "boards/dji_c_board_f407/Middlewares/ST/usbx/LICENSED-HARDWARE.txt",
)

SURFACE_FILES = (
    "CMakeLists.txt",
    "CMakePresets.json",
    "cmake/gcc-arm-none-eabi.cmake",
    "README.md",
    "AGENTS.md",
)
TEXT_SUFFIXES = {
    "",
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".h",
    ".hpp",
    ".ioc",
    ".json",
    ".ld",
    ".md",
    ".ps1",
    ".py",
    ".s",
    ".sh",
    ".txt",
    ".yaml",
    ".yml",
}
TEMP_SUFFIXES = {".bak", ".log", ".orig", ".pyc", ".swp", ".tmp"}
TEMP_NAMES = {
    ".DS_Store",
    "CMakeCache.txt",
    "Thumbs.db",
}
WINDOWS_PATH_RE = re.compile(r"(?<![A-Za-z0-9_])[A-Za-z]:[\\/]")
UNC_PATH_RE = re.compile(r"\\\\[^\\\s]+\\[^\\\s]+")
CREDENTIAL_URL_RE = re.compile(
    r"https?://[^/\s:@]+(?::[^@\s/]*)?@", re.IGNORECASE
)
CREDENTIAL_QUERY_RE = re.compile(
    r"(?i)[?&](?:access_token|api_key|password|passwd|secret|token)="
    r"[^&#\s\"']+"
)
FILE_URL_MARKER = "file:" + "//"
PRIVATE_KEY_MARKER = "-----BEGIN PRIVATE " + "KEY-----"


def _read_text(
    path: Path, errors: list[str], relative: str
) -> str | None:
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as exc:
        errors.append(f"cannot read UTF-8 text {relative}: {exc}")
        return None


def check_delivery_surface(root: Path) -> list[str]:
    """Check user/build entry files, while allowing provenance/negative tests."""

    errors: list[str] = []
    tracked, tracked_failure = _git(root, "ls-files", "-z")
    if tracked_failure is None:
        delivery_paths = [item for item in tracked.split("\0") if item]
    else:
        delivery_paths = [
            path.relative_to(root).as_posix()
            for path in root.rglob("*")
            if path.is_file() and ".git" not in path.relative_to(root).parts
        ]
    for relative in delivery_paths:
        lowered_path = relative.casefold()
        if (
            lowered_path.startswith("board/")
            or "h723" in lowered_path
            or lowered_path == "stm32h723xg_flash.ld"
        ):
            errors.append(f"H723/legacy board path in delivery: {relative}")
        if lowered_path.startswith("boards/") and not lowered_path.startswith(
            "boards/dji_c_board_f407/"
        ):
            errors.append(f"non-F407 board path in delivery: {relative}")

    selected: list[Path] = []
    for relative in SURFACE_FILES:
        path = root / relative
        if path.is_file():
            selected.append(path)
    docs = root / "docs"
    if docs.is_dir():
        selected.extend(sorted(path for path in docs.rglob("*.md") if path.is_file()))

    for path in selected:
        relative = path.relative_to(root).as_posix()
        text = _read_text(path, errors, relative)
        if text is None:
            continue
        lowered = text.casefold()
        normalized = text.replace("\\", "/")
        if any(
            re.search(pattern, normalized)
            for pattern in (
                r"(?im)\bcmake\b[^\r\n]*(?:--preset|--build)\b"
                r"[^\r\n]*h723",
                r"(?im)\bPNX_BOARD\b[^\r\n]*h723",
                r"(?im)(?:^|/)boards?/[^\r\n` ]*h723",
            )
        ):
            errors.append(f"H723 user/build entry in {relative}")
        if "stm32h723" in lowered:
            errors.append(f"H723 MCU entry in {relative}")
        if re.search(r"(?:^|/)board/", normalized):
            errors.append(f"forbidden board/Core/main.c-style path in {relative}")
        if "original-board" in lowered:
            errors.append(f"original-board preset/entry in {relative}")
        if "stm32h723xg_flash.ld" in lowered:
            errors.append(f"H723 linker entry in {relative}")

    cmake = root / "CMakeLists.txt"
    if cmake.is_file():
        cmake_text = _read_text(cmake, errors, "CMakeLists.txt")
        if cmake_text is not None and re.search(
            r"set\s*\(\s*PNX_BOARD(?:\s|$)", cmake_text
        ):
            errors.append("PNX_BOARD user selector remains in CMakeLists.txt")

    presets_path = root / "CMakePresets.json"
    if presets_path.is_file():
        try:
            presets = json.loads(presets_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            errors.append(f"invalid CMakePresets.json: {exc}")
        else:
            serialized = json.dumps(presets)
            if "PNX_BOARD" in serialized:
                errors.append("PNX_BOARD user selector remains in presets")
            if "h723" in serialized.casefold():
                errors.append("H723 preset/entry remains")
            names = {
                item.get("name")
                for key in ("configurePresets", "buildPresets", "testPresets")
                for item in presets.get(key, [])
                if isinstance(item, dict)
            }
            if "original-board" in names or "original-board-release" in names:
                errors.append("original-board preset remains")
    return errors


def _is_temporary(relative: str) -> bool:
    path = Path(relative)
    if path.name in TEMP_NAMES or path.suffix.casefold() in TEMP_SUFFIXES:
        return True
    return any(part.casefold() in {"__pycache__", "build"} for part in path.parts)


def check_cleanliness(root: Path) -> list[str]:
    """Reject machine paths, local rewrites, secrets and build/temp payload."""

    errors: list[str] = []
    tracked, tracked_failure = _git(root, "ls-files", "-z")
    if tracked_failure is None:
        candidates = [
            root / Path(*Path(relative).parts)
            for relative in tracked.split("\0")
            if relative
        ]
    else:
        candidates = sorted(root.rglob("*"))
    for path in candidates:
        relative = path.relative_to(root).as_posix()
        if relative == ".git" or relative.startswith(".git/"):
            continue
        if path.is_symlink():
            errors.append(f"symlink is forbidden: {relative}")
            continue
        if path.is_dir():
            if _is_temporary(relative):
                errors.append(f"temporary/build artifact: {relative}")
            continue
        if not path.is_file():
            errors.append(f"unsupported filesystem entry: {relative}")
            continue
        if _is_temporary(relative):
            errors.append(f"temporary/build artifact: {relative}")
        if path.suffix.casefold() not in TEXT_SUFFIXES:
            continue
        text = _read_text(path, errors, relative)
        if text is None:
            continue
        if WINDOWS_PATH_RE.search(text) or UNC_PATH_RE.search(text):
            errors.append(f"absolute Windows path in {relative}")
        if FILE_URL_MARKER in text.casefold():
            errors.append(f"file URL in {relative}")
        if PRIVATE_KEY_MARKER in text:
            errors.append(f"private key material in {relative}")
        if CREDENTIAL_URL_RE.search(text):
            errors.append(f"credential-bearing HTTPS URL in {relative}")
        if CREDENTIAL_QUERY_RE.search(text):
            errors.append(f"credential token in URL in {relative}")
    return errors


def check_gitmodules_text(text: str) -> list[str]:
    errors: list[str] = []
    urls = [
        line.split("=", 1)[1].strip()
        for line in text.splitlines()
        if line.strip().lower().startswith("url") and "=" in line
    ]
    if not urls:
        return ["no submodule URLs found"]
    for url in urls:
        parsed = urlsplit(url)
        try:
            parsed.port
        except ValueError:
            errors.append(
                "submodule URL is not clean credential-free HTTPS"
            )
            continue
        if (
            parsed.scheme != "https"
            or not parsed.hostname
            or parsed.username is not None
            or parsed.password is not None
            or bool(parsed.query)
            or bool(parsed.fragment)
        ):
            errors.append(
                "submodule URL is not clean credential-free HTTPS"
            )
    return errors


def _git(root: Path, *args: str) -> tuple[str, str | None]:
    completed = subprocess.run(
        ["git", *args],
        cwd=root,
        capture_output=True,
        env=f407_export._safe_git_environment(os.environ),
        text=True,
    )
    if completed.returncode != 0:
        return "", completed.stderr.strip() or f"exit {completed.returncode}"
    return completed.stdout, None


def _git_index(
    root: Path,
) -> tuple[dict[str, str], dict[str, str], list[str]]:
    output, failure = _git(root, "ls-files", "--stage")
    if failure is not None:
        return {}, {}, [f"cannot inspect candidate Git index: {failure}"]
    file_modes: dict[str, str] = {}
    gitlinks: dict[str, str] = {}
    errors: list[str] = []
    for line in output.splitlines():
        try:
            metadata, path = line.split("\t", 1)
            mode, object_id, stage = metadata.split()
            path = f407_export.validate_relative(path.replace("\\", "/"))
        except (ValueError, f407_export.ExportError):
            errors.append(f"cannot parse Git index entry: {line}")
            continue
        if stage != "0":
            errors.append(f"non-zero Git index stage: {path}")
        if mode == "160000":
            gitlinks[path] = object_id
        elif mode in {"100644", "100755"}:
            file_modes[path] = mode
        else:
            errors.append(f"unsupported tracked Git mode {mode}: {path}")
    return file_modes, gitlinks, errors


def _load_json(path: Path, label: str, errors: list[str]) -> dict[str, object]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        errors.append(f"invalid {label}: {exc}")
        return {}
    if not isinstance(data, dict):
        errors.append(f"{label} must be an object")
        return {}
    return data


def _raw_sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def check_provenance(root: Path) -> list[str]:
    """Verify candidate manifests, imported inputs, Git index and tree hash."""

    errors: list[str] = []
    provenance_path = root / f407_export.PROVENANCE_PATH
    provenance = _load_json(
        provenance_path, "F407-only provenance manifest", errors
    )
    if provenance.get("schema_version") != 1:
        errors.append("unsupported F407-only provenance schema")
    if provenance.get("manifest_type") != "pnx-f407-only-export-provenance":
        errors.append("wrong F407-only provenance manifest type")
    if (
        provenance.get("status")
        != "unaccepted-unpublished-hardware-unverified-candidate"
    ):
        errors.append("candidate provenance status exceeds F4-2 authority")
    expected_constraints = {
        "hardware_used": False,
        "push_performed": False,
        "remote_modified": False,
        "tag_created": False,
        "vault_ingested": False,
    }
    if provenance.get("constraints") != expected_constraints:
        errors.append("candidate provenance constraints are not fail-closed")
    authoritative = provenance.get("authoritative_source")
    if not isinstance(authoritative, dict):
        errors.append("authoritative_source must be an object")
        authoritative = {}
    if (
        authoritative.get("repository_role")
        != "pnx-h723-f407-multiboard-integration"
    ):
        errors.append("wrong authoritative repository role")
    for key in ("firmware_commit", "source_manifest_commit", "exporter_commit"):
        value = authoritative.get(key)
        if not isinstance(value, str) or not f407_export.COMMIT_RE.fullmatch(value):
            errors.append(f"invalid authoritative_source.{key}")

    if provenance.get("runtime_gates", {}).get("hardware_gate") != (
        "HARDWARE_UNVERIFIED"
    ):
        errors.append("candidate hardware gate is not HARDWARE_UNVERIFIED")
    usb = provenance.get("usb")
    if (
        not isinstance(usb, dict)
        or usb.get("telemetry_abi") != 4
        or usb.get("descriptor_identity_state") != "UNASSIGNED_FAIL_CLOSED"
    ):
        errors.append("candidate USB ABI/descriptor state is not fail-closed")
    distribution = provenance.get("f407_only_distribution")
    expected_distribution = {
        "fresh_clone": "NOT_RUN",
        "hardware": "HARDWARE_UNVERIFIED",
        "software_validation": "NOT_RUN",
        "team_release": "NOT_PUBLISHED",
    }
    if (
        not isinstance(distribution, dict)
        or distribution != expected_distribution
    ):
        errors.append("candidate distribution status exceeds F4-2 authority")

    export_data = provenance.get("export")
    if not isinstance(export_data, dict):
        errors.append("export provenance must be an object")
        export_data = {}
    if export_data.get("canonical_tree_format") != f407_export.TREE_FORMAT:
        errors.append("wrong canonical tree format")
    if export_data.get("idempotence") != "NOT_RUN":
        errors.append("candidate export idempotence status is overstated")
    declared_hash = export_data.get("canonical_tree_sha256")
    if (
        not isinstance(declared_hash, str)
        or not re.fullmatch(r"[0-9A-F]{64}", declared_hash)
    ):
        errors.append("invalid canonical tree SHA-256")

    file_modes, gitlinks, index_errors = _git_index(root)
    errors.extend(index_errors)
    status, status_failure = _git(root, "status", "--short")
    if status_failure is not None:
        errors.append(f"cannot inspect candidate status: {status_failure}")
    elif status.strip():
        errors.append(f"candidate worktree is dirty: {status.strip()}")

    declared_modules = provenance.get("submodules")
    declared_pins: dict[str, str] = {}
    if not isinstance(declared_modules, dict):
        errors.append("candidate submodules must be an object")
    else:
        for path, value in declared_modules.items():
            commit = value.get("commit") if isinstance(value, dict) else None
            try:
                safe_path = f407_export.validate_relative(path)
            except f407_export.ExportError as exc:
                errors.append(str(exc))
                continue
            if not isinstance(commit, str) or not f407_export.COMMIT_RE.fullmatch(
                commit
            ):
                errors.append(f"invalid candidate gitlink commit: {safe_path}")
            else:
                declared_pins[safe_path] = commit
            if (
                not isinstance(value, dict)
                or value.get("remote_reachability") != "not_rechecked"
            ):
                errors.append(
                    f"candidate remote reachability is overstated: {safe_path}"
                )
    if declared_pins != gitlinks:
        errors.append("candidate Git index gitlinks differ from provenance")

    inventory_relative = export_data.get("input_inventory_path")
    try:
        inventory_relative = f407_export.validate_relative(inventory_relative)
    except (f407_export.ExportError, TypeError):
        errors.append("invalid input inventory path")
        inventory_relative = "release/f407-only-input-inventory.json"
    inventory = _load_json(
        root / inventory_relative, "F407-only input inventory", errors
    )
    if inventory.get("schema_version") != 1:
        errors.append("unsupported F407-only input inventory schema")
    if inventory.get("manifest_type") != "pnx-f407-only-input-inventory":
        errors.append("wrong F407-only input inventory type")
    imported_paths: set[str] = set()
    files = inventory.get("files")
    if not isinstance(files, list):
        errors.append("input inventory files must be a list")
        files = []
    for index, item in enumerate(files):
        if not isinstance(item, dict):
            errors.append(f"input inventory file {index} must be an object")
            continue
        try:
            destination = f407_export.validate_relative(item.get("destination"))
        except (f407_export.ExportError, TypeError):
            errors.append(f"invalid input inventory destination at {index}")
            continue
        if destination in imported_paths:
            errors.append(f"duplicate input inventory destination: {destination}")
            continue
        imported_paths.add(destination)
        target = root / destination
        if not target.is_file():
            errors.append(f"missing imported file: {destination}")
            continue
        if item.get("sha256") != _raw_sha256(target):
            errors.append(f"imported file hash mismatch: {destination}")
        if item.get("size") != target.stat().st_size:
            errors.append(f"imported file size mismatch: {destination}")
        if item.get("git_mode") != file_modes.get(destination):
            errors.append(f"imported file mode mismatch: {destination}")
        if item.get("source_role") not in f407_export.POLICY_ROLES:
            errors.append(f"invalid imported file source role: {destination}")
        role = item.get("source_role")
        expected_commit = {
            "accepted_firmware": authoritative.get("firmware_commit"),
            "accepted_source_manifest": authoritative.get(
                "source_manifest_commit"
            ),
            "exporter_overlay": authoritative.get("exporter_commit"),
            "exporter_template": authoritative.get("exporter_commit"),
        }.get(role)
        if item.get("source_commit") != expected_commit:
            errors.append(
                f"imported file role/source commit mismatch: {destination}"
            )
        for key in ("source_commit", "object_id"):
            value = item.get(key)
            if not isinstance(value, str) or not f407_export.COMMIT_RE.fullmatch(
                value
            ):
                errors.append(f"invalid imported file {key}: {destination}")

    inventory_links = inventory.get("gitlinks")
    if not isinstance(inventory_links, list):
        errors.append("input inventory gitlinks must be a list")
        inventory_links = []
    inventory_pins: dict[str, str] = {}
    for index, item in enumerate(inventory_links):
        if not isinstance(item, dict):
            errors.append(f"input inventory gitlink {index} must be an object")
            continue
        try:
            destination = f407_export.validate_relative(
                item.get("destination")
            )
        except (f407_export.ExportError, TypeError):
            errors.append(f"invalid input inventory gitlink at {index}")
            continue
        commit = item.get("commit")
        if destination in inventory_pins:
            errors.append(
                f"duplicate input inventory gitlink: {destination}"
            )
            continue
        if not isinstance(commit, str) or not f407_export.COMMIT_RE.fullmatch(
            commit
        ):
            errors.append(f"invalid inventory gitlink commit: {destination}")
            continue
        inventory_pins[destination] = commit
        if (
            item.get("git_mode") != "160000"
            or item.get("source_role") != "accepted_shared_gitlink"
            or item.get("source_commit")
            != authoritative.get("firmware_commit")
        ):
            errors.append(
                f"invalid inventory gitlink provenance: {destination}"
            )
    if inventory_pins != declared_pins:
        errors.append("input inventory gitlinks differ from provenance")

    policy: dict[str, object] = {}
    policy_data = inventory.get("policy")
    if not isinstance(policy_data, dict):
        errors.append("input inventory policy must be an object")
    else:
        try:
            policy_relative = f407_export.validate_relative(
                policy_data.get("path")
            )
        except (f407_export.ExportError, TypeError):
            errors.append("invalid inventory policy path")
        else:
            policy_path = root / policy_relative
            if not policy_path.is_file():
                errors.append("candidate export policy is missing")
            elif policy_data.get("sha256") != _raw_sha256(policy_path):
                errors.append("candidate export policy hash mismatch")
            elif policy_data.get("source_commit") != authoritative.get(
                "exporter_commit"
            ):
                errors.append("candidate export policy commit mismatch")
            else:
                try:
                    policy = f407_export.load_policy(policy_path)
                except f407_export.ExportError as exc:
                    errors.append(f"candidate export policy invalid: {exc}")
                else:
                    accepted = policy["accepted_source"]
                    if accepted.get("firmware_commit") != authoritative.get(
                        "firmware_commit"
                    ):
                        errors.append("policy/source firmware commit mismatch")
                    if accepted.get("manifest_commit") != authoritative.get(
                        "source_manifest_commit"
                    ):
                        errors.append("policy/source manifest commit mismatch")
                    if accepted.get("manifest_path") != authoritative.get(
                        "source_manifest_path"
                    ):
                        errors.append("policy/source manifest path mismatch")
                    if set(policy.get("submodule_paths", [])) != set(
                        declared_pins
                    ):
                        errors.append("policy/candidate submodule set mismatch")
                    build = provenance.get("build")
                    if (
                        not isinstance(build, dict)
                        or build.get("firmware_presets")
                        != policy.get("firmware_presets")
                        or build.get("host_test_preset") != "host-tests"
                        or build.get("board_selector_required") is not False
                    ):
                        errors.append("candidate build provenance mismatch")

    generated_paths = {
        inventory_relative,
        f407_export.PROVENANCE_PATH,
    }
    if set(file_modes) != imported_paths | generated_paths:
        errors.append("tracked file set differs from imported plus generated paths")

    source_manifest_path = root / "release" / "f407-only-source-manifest.json"
    source_manifest = _load_json(
        source_manifest_path, "accepted-source manifest copy", errors
    )
    if (
        source_manifest.get("schema_version") != 1
        or source_manifest.get("manifest_type")
        != "pnx-f407-only-source-baseline"
        or source_manifest.get("status")
        != "accepted-software-source-hardware-unverified"
    ):
        errors.append("accepted-source manifest copy has wrong schema/status")
    source_authority = source_manifest.get("authoritative_source")
    if (
        not isinstance(source_authority, dict)
        or source_authority.get("superproject_commit")
        != authoritative.get("firmware_commit")
    ):
        errors.append("accepted-source copy firmware commit mismatch")
    source_modules = source_manifest.get("submodules")
    source_pins = {
        path: value.get("commit")
        for path, value in source_modules.items()
        if isinstance(source_modules, dict) and isinstance(value, dict)
    } if isinstance(source_modules, dict) else {}
    if source_pins != declared_pins:
        errors.append("accepted-source copy gitlinks differ from provenance")

    board = provenance.get("f407_board")
    if not isinstance(board, dict):
        errors.append("candidate f407_board must be an object")
        board = {}
    if board != source_manifest.get("f407_board"):
        errors.append("candidate board provenance differs from accepted source")
    software = provenance.get("software_gates")
    if (
        not isinstance(software, dict)
        or software.get("source_manifest_evidence")
        != source_manifest.get("software_gates")
        or software.get("f407_only_software_validation") != "NOT_RUN"
    ):
        errors.append("candidate software Gate provenance mismatch")
    if provenance.get("runtime_gates") != source_manifest.get(
        "runtime_gates"
    ):
        errors.append("candidate runtime Gate provenance mismatch")
    if provenance.get("usb") != source_manifest.get("usb"):
        errors.append("candidate USB provenance differs from accepted source")
    for path_key, hash_key in (
        ("minimal_ioc_path", "minimal_ioc_sha256"),
        (
            "formal_generated_tree_manifest_path",
            "formal_generated_tree_manifest_sha256",
        ),
    ):
        try:
            relative = f407_export.validate_relative(board.get(path_key))
        except (f407_export.ExportError, TypeError):
            errors.append(f"invalid f407_board.{path_key}")
            continue
        target = root / relative
        if not target.is_file():
            errors.append(f"missing F407 provenance input: {relative}")
        elif board.get(hash_key) != _raw_sha256(target):
            errors.append(f"F407 provenance hash mismatch: {relative}")
    formal_path = board.get("formal_generated_tree_manifest_path")
    if isinstance(formal_path, str) and (root / formal_path).is_file():
        formal = _load_json(root / formal_path, "formal tree manifest", errors)
        if formal.get("tree_sha256") != board.get("formal_generated_tree_hash"):
            errors.append("formal generated-tree hash differs from provenance")
        if formal.get("file_count") != board.get(
            "formal_generated_tree_file_count"
        ):
            errors.append("formal generated-tree file count differs")

    if declared_pins and isinstance(declared_hash, str):
        try:
            actual = f407_export.canonical_tree(
                root,
                declared_pins,
                file_modes,
                ignore_untracked=True,
            ).sha256
        except f407_export.ExportError as exc:
            errors.append(f"cannot calculate candidate canonical tree: {exc}")
        else:
            if actual != declared_hash:
                errors.append("candidate canonical tree hash mismatch")
    return errors


def check_candidate(root: Path) -> list[str]:
    errors = check_delivery_surface(root)
    errors.extend(check_cleanliness(root))
    errors.extend(check_provenance(root))
    gitmodules = root / ".gitmodules"
    if not gitmodules.is_file():
        errors.append("missing .gitmodules")
    else:
        gitmodules_text = _read_text(
            gitmodules, errors, ".gitmodules"
        )
        if gitmodules_text is not None:
            errors.extend(check_gitmodules_text(gitmodules_text))
    for relative in REQUIRED_LICENSES:
        if not (root / relative).is_file():
            errors.append(f"missing required license: {relative}")
    return errors


def check_promoted_workspace(root: Path) -> list[str]:
    """Check a promoted workspace without reasserting immutable export identity."""

    errors = check_delivery_surface(root)
    errors.extend(check_cleanliness(root))
    gitmodules = root / ".gitmodules"
    if not gitmodules.is_file():
        errors.append("missing .gitmodules")
    else:
        gitmodules_text = _read_text(
            gitmodules, errors, ".gitmodules"
        )
        if gitmodules_text is not None:
            errors.extend(check_gitmodules_text(gitmodules_text))
    for relative in REQUIRED_LICENSES:
        if not (root / relative).is_file():
            errors.append(f"missing required license: {relative}")
    return errors


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    parser.add_argument(
        "--promoted-workspace",
        action="store_true",
        help=(
            "check delivery, cleanliness, gitmodules and licenses without "
            "reasserting the immutable exporter-candidate tree identity"
        ),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.promoted_workspace:
        errors = check_promoted_workspace(args.repo_root.resolve())
        label = "F407-only promoted workspace gate"
    else:
        errors = check_candidate(args.repo_root.resolve())
        label = "F407-only candidate gate"
    if errors:
        print(f"{label} failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print(f"{label} passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
