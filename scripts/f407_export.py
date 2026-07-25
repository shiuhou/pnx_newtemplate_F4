#!/usr/bin/env python3
"""Deterministic F407-only export primitives.

The CLI is added after the core object-materialization and canonical-tree
contract is covered by host tests.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from typing import Any, Iterable, Mapping


TREE_FORMAT = "pnx-f407-export-tree-v1"
PROVENANCE_PATH = "release/f407-only-provenance.json"
PENDING_HASH = "PENDING"
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
WINDOWS_ABSOLUTE_RE = re.compile(r"^[A-Za-z]:/")
POLICY_ROLES = {
    "accepted_firmware",
    "accepted_source_manifest",
    "exporter_overlay",
    "exporter_template",
}
REMOVED_GIT_ENV = {
    "GIT_ALTERNATE_OBJECT_DIRECTORIES",
    "GIT_ASKPASS",
    "GIT_CEILING_DIRECTORIES",
    "GIT_COMMON_DIR",
    "GIT_CONFIG",
    "GIT_CONFIG_COUNT",
    "GIT_CONFIG_PARAMETERS",
    "GIT_DEFAULT_HASH",
    "GIT_DEFAULT_REF_FORMAT",
    "GIT_DIR",
    "GIT_EXEC_PATH",
    "GIT_GRAFT_FILE",
    "GIT_INDEX_FILE",
    "GIT_NAMESPACE",
    "GIT_OBJECT_DIRECTORY",
    "GIT_PROTOCOL_FROM_USER",
    "GIT_PROXY_COMMAND",
    "GIT_REPLACE_REF_BASE",
    "GIT_SHALLOW_FILE",
    "GIT_SSL_NO_VERIFY",
    "GIT_SSH",
    "GIT_SSH_COMMAND",
    "GIT_TEMPLATE_DIR",
    "GIT_ALLOW_PROTOCOL",
    "GIT_WORK_TREE",
    "SSH_ASKPASS",
}
REMOVED_GIT_ENV_PREFIXES = (
    "GIT_CONFIG_KEY_",
    "GIT_CONFIG_VALUE_",
    "GIT_TRACE",
)


class ExportError(RuntimeError):
    """Raised when an export input cannot be represented safely."""


@dataclass(frozen=True)
class GitEntry:
    mode: str
    kind: str
    object_id: str
    path: str


@dataclass(frozen=True)
class CanonicalTree:
    format: str
    sha256: str
    records: tuple[dict[str, object], ...]


@dataclass(frozen=True)
class ExportResult:
    output: Path
    canonical_sha256: str
    git_tree: str
    git_commit: str
    source_commit: str
    source_manifest_commit: str
    exporter_commit: str


def _validate_policy(data: object) -> dict[str, Any]:
    if not isinstance(data, dict):
        raise ExportError("export policy must be an object")
    if data.get("schema_version") != 1:
        raise ExportError("unsupported export policy schema")
    if data.get("policy_type") != "pnx-f407-only-export-policy":
        raise ExportError("wrong export policy type")
    accepted = data.get("accepted_source")
    if not isinstance(accepted, dict):
        raise ExportError("accepted_source must be an object")
    for key in ("firmware_commit", "manifest_commit"):
        if not isinstance(accepted.get(key), str) or not COMMIT_RE.fullmatch(
            accepted[key]
        ):
            raise ExportError(f"accepted_source.{key} must be a full commit")
    validate_relative(str(accepted.get("manifest_path", "")))

    destinations: dict[str, str] = {}
    expected_roles = {
        "source_entries": "accepted_firmware",
        "manifest_entries": "accepted_source_manifest",
        "exporter_entries": "exporter_overlay",
    }
    for collection, expected_role in expected_roles.items():
        items = data.get(collection)
        if not isinstance(items, list) or not items:
            raise ExportError(f"{collection} must be a non-empty list")
        for index, item in enumerate(items):
            if not isinstance(item, dict):
                raise ExportError(f"{collection}[{index}] must be an object")
            source = validate_relative(str(item.get("path", "")))
            destination = validate_relative(
                str(item.get("destination", source))
            )
            if item.get("kind") not in {"file", "tree"}:
                raise ExportError(f"invalid {collection} kind: {source}")
            if item.get("source_role") != expected_role:
                raise ExportError(
                    f"invalid {collection} source role: {source}"
                )
            previous = destinations.get(destination)
            if previous is not None:
                raise ExportError(
                    f"duplicate destination owner: {destination} "
                    f"({previous}, {collection})"
                )
            destinations[destination] = collection

    templates = data.get("templates")
    if not isinstance(templates, list) or not templates:
        raise ExportError("templates must be a non-empty list")
    for index, item in enumerate(templates):
        if not isinstance(item, dict):
            raise ExportError(f"templates[{index}] must be an object")
        validate_relative(str(item.get("path", "")))
        destination = validate_relative(str(item.get("destination", "")))
        if item.get("mode") not in {"100644", "100755"}:
            raise ExportError(f"invalid template mode: {destination}")
        if item.get("source_role") != "exporter_template":
            raise ExportError(f"invalid template source role: {destination}")
        previous = destinations.get(destination)
        if previous is not None:
            raise ExportError(
                f"duplicate destination owner: {destination} "
                f"({previous}, templates)"
            )
        destinations[destination] = "templates"

    submodules = data.get("submodule_paths")
    if (
        not isinstance(submodules, list)
        or not submodules
        or not all(isinstance(item, str) for item in submodules)
        or len(set(submodules)) != len(submodules)
    ):
        raise ExportError("submodule_paths must be a unique string list")
    for item in submodules:
        validate_relative(item)
    denied = data.get("deny_paths")
    if not isinstance(denied, list) or not denied:
        raise ExportError("deny_paths must be a non-empty list")
    for item in denied:
        validate_relative(str(item))
    firmware_presets = data.get("firmware_presets")
    if (
        not isinstance(firmware_presets, list)
        or not firmware_presets
        or not all(
            isinstance(item, str)
            and re.fullmatch(r"[a-z0-9][a-z0-9-]*", item)
            for item in firmware_presets
        )
        or len(set(firmware_presets)) != len(firmware_presets)
    ):
        raise ExportError(
            "firmware_presets must be a unique non-empty preset-name list"
        )
    generated = data.get("generated_paths")
    if not isinstance(generated, dict):
        raise ExportError("generated_paths must be an object")
    inventory_path = validate_relative(
        str(generated.get("input_inventory", ""))
    )
    provenance_path = validate_relative(str(generated.get("provenance", "")))
    if provenance_path != PROVENANCE_PATH:
        raise ExportError(
            f"generated provenance path must be {PROVENANCE_PATH}"
        )
    if inventory_path == provenance_path:
        raise ExportError("generated inventory and provenance paths must differ")
    return data


def load_policy(path: Path) -> dict[str, Any]:
    """Load and structurally validate the machine-readable export boundary."""

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ExportError(f"cannot load export policy: {path}") from exc
    return _validate_policy(data)


def _run_git(
    repo: Path,
    *args: str,
    input_bytes: bytes | None = None,
    env: Mapping[str, str] | None = None,
) -> bytes:
    effective_env = (
        dict(env) if env is not None else _safe_git_environment(os.environ)
    )
    try:
        return subprocess.run(
            ["git", *args],
            cwd=repo,
            input=input_bytes,
            check=True,
            capture_output=True,
            env=effective_env,
        ).stdout
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.decode("utf-8", errors="replace").strip()
        raise ExportError(
            f"git {' '.join(args)} failed ({exc.returncode}): {stderr}"
        ) from exc


def _safe_git_environment(
    base: Mapping[str, str],
) -> dict[str, str]:
    """Remove repository/object/config injection from a Git child process."""

    env = dict(base)
    for key in list(env):
        if (
            key.startswith("GIT_")
            or key == "SSH_ASKPASS"
        ):
            env.pop(key, None)
    env.update(
        {
            "GIT_ATTR_NOSYSTEM": "1",
            "GIT_NO_REPLACE_OBJECTS": "1",
            "GIT_TERMINAL_PROMPT": "0",
        }
    )
    return env


def validate_relative(value: str) -> str:
    """Return a normalized repository-relative POSIX path or fail closed."""

    if not isinstance(value, str) or not value or "\x00" in value:
        raise ExportError("path must be a non-empty string without NUL")
    if "\\" in value:
        raise ExportError(f"backslash is forbidden in repository path: {value!r}")
    if value.startswith("/") or value.startswith("//"):
        raise ExportError(f"absolute path is forbidden: {value!r}")
    if WINDOWS_ABSOLUTE_RE.match(value):
        raise ExportError(f"Windows absolute path is forbidden: {value!r}")
    pure = PurePosixPath(value)
    if pure.is_absolute() or any(part in {"", ".", ".."} for part in pure.parts):
        raise ExportError(f"unsafe repository path: {value!r}")
    normalized = pure.as_posix()
    if normalized.casefold() == ".git" or normalized.casefold().startswith(
        ".git/"
    ):
        raise ExportError(f"Git metadata path is forbidden: {value!r}")
    return normalized


def _parse_ls_tree(payload: bytes) -> dict[str, GitEntry]:
    entries: dict[str, GitEntry] = {}
    folded: dict[str, str] = {}
    for raw in payload.split(b"\0"):
        if not raw:
            continue
        try:
            metadata, raw_path = raw.split(b"\t", 1)
            mode, kind, object_id = metadata.decode("ascii").split(" ", 2)
            path = validate_relative(raw_path.decode("utf-8"))
        except (ValueError, UnicodeDecodeError) as exc:
            raise ExportError("cannot parse git ls-tree record") from exc
        if mode not in {"100644", "100755", "160000"}:
            raise ExportError(f"unsupported Git mode {mode}: {path}")
        if mode == "160000" and kind != "commit":
            raise ExportError(f"gitlink is not a commit: {path}")
        if mode != "160000" and kind != "blob":
            raise ExportError(f"non-blob source entry is forbidden: {path}")
        collision = folded.get(path.casefold())
        if collision is not None and collision != path:
            raise ExportError(f"case-fold path collision: {collision} vs {path}")
        folded[path.casefold()] = path
        entries[path] = GitEntry(mode, kind, object_id, path)
    return entries


def list_git_entries(
    repo: Path, commit: str, paths: Iterable[str]
) -> dict[str, GitEntry]:
    """List recursively selected blobs/gitlinks from an immutable Git object."""

    selected = [validate_relative(path) for path in paths]
    if not selected:
        raise ExportError("at least one source path is required")
    payload = _run_git(repo, "ls-tree", "-r", "-z", commit, "--", *selected)
    entries = _parse_ls_tree(payload)
    missing = [
        path
        for path in selected
        if path not in entries
        and not any(candidate.startswith(f"{path}/") for candidate in entries)
    ]
    if missing:
        raise ExportError(f"source path is absent at {commit}: {', '.join(missing)}")
    return entries


def materialize_git_entries(
    repo: Path,
    commit: str,
    entries: Mapping[str, GitEntry],
    output: Path,
) -> None:
    """Write selected blobs directly from a commit, never from the worktree."""

    output.mkdir(parents=True, exist_ok=True)
    for path, entry in sorted(entries.items()):
        if path != entry.path:
            raise ExportError(f"entry key/path mismatch: {path} vs {entry.path}")
        destination = output / Path(*PurePosixPath(path).parts)
        if entry.mode == "160000":
            if destination.exists():
                raise ExportError(f"gitlink destination unexpectedly exists: {path}")
            continue
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists() or destination.is_symlink():
            raise ExportError(f"duplicate export destination: {path}")
        destination.write_bytes(_run_git(repo, "show", f"{commit}:{path}"))


def _git_blob(repo: Path, commit: str, path: str) -> bytes:
    return _run_git(repo, "show", f"{commit}:{validate_relative(path)}")


def _load_json_blob(repo: Path, commit: str, path: str, label: str) -> dict[str, Any]:
    try:
        data = json.loads(_git_blob(repo, commit, path).decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ExportError(f"invalid JSON Git object for {label}") from exc
    if not isinstance(data, dict):
        raise ExportError(f"{label} must be a JSON object")
    return data


def _write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(data, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def _destination_for(
    selected_root: str, destination_root: str, source_path: str
) -> str:
    if source_path == selected_root:
        return destination_root
    suffix = source_path.removeprefix(f"{selected_root}/")
    if suffix == source_path:
        raise ExportError(
            f"selected source escaped root {selected_root}: {source_path}"
        )
    return validate_relative(f"{destination_root}/{suffix}")


def _materialize_policy_collection(
    repo: Path,
    commit: str,
    items: list[dict[str, Any]],
    output: Path,
    owners: dict[str, str],
    file_modes: dict[str, str],
    inventory: list[dict[str, object]],
) -> None:
    for item in items:
        source_root = validate_relative(item["path"])
        destination_root = validate_relative(
            item.get("destination", source_root)
        )
        entries = list_git_entries(repo, commit, [source_root])
        if item["kind"] == "file" and set(entries) != {source_root}:
            raise ExportError(f"policy file entry is not one blob: {source_root}")
        for source_path, entry in sorted(entries.items()):
            if entry.mode == "160000":
                raise ExportError(
                    f"gitlink must use submodule_paths, not file policy: {source_path}"
                )
            declared_mode = item.get("mode")
            if declared_mode is not None and declared_mode != entry.mode:
                raise ExportError(
                    f"template mode {declared_mode} does not match "
                    f"Git blob mode {entry.mode}: {source_path}"
                )
            destination = _destination_for(
                source_root, destination_root, source_path
            )
            previous = owners.get(destination)
            if previous is not None:
                raise ExportError(
                    f"expanded destination collision: {destination} "
                    f"({previous}, {item['source_role']})"
                )
            owners[destination] = item["source_role"]
            payload = _git_blob(repo, commit, source_path)
            target = output / Path(*PurePosixPath(destination).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(payload)
            if os.name != "nt":
                target.chmod(0o755 if entry.mode == "100755" else 0o644)
            file_modes[destination] = entry.mode
            inventory.append(
                {
                    "destination": destination,
                    "git_mode": entry.mode,
                    "object_id": entry.object_id,
                    "sha256": hashlib.sha256(payload).hexdigest().upper(),
                    "size": len(payload),
                    "source_commit": commit,
                    "source_path": source_path,
                    "source_role": item["source_role"],
                }
            )


def _validate_source_manifest(
    manifest: dict[str, Any],
    firmware_commit: str,
    submodule_paths: list[str],
) -> dict[str, str]:
    if manifest.get("manifest_type") != "pnx-f407-only-source-baseline":
        raise ExportError("wrong accepted-source manifest type")
    source = manifest.get("authoritative_source")
    if (
        not isinstance(source, dict)
        or source.get("superproject_commit") != firmware_commit
    ):
        raise ExportError("accepted-source manifest firmware commit mismatch")
    runtime = manifest.get("runtime_gates")
    if (
        not isinstance(runtime, dict)
        or runtime.get("hardware_gate") != "HARDWARE_UNVERIFIED"
    ):
        raise ExportError("accepted-source hardware gate is not unverified")
    usb = manifest.get("usb")
    if (
        not isinstance(usb, dict)
        or usb.get("telemetry_abi") != 4
        or usb.get("descriptor_identity_state") != "UNASSIGNED_FAIL_CLOSED"
    ):
        raise ExportError("accepted-source USB identity/ABI mismatch")
    modules = manifest.get("submodules")
    if not isinstance(modules, dict) or set(modules) != set(submodule_paths):
        raise ExportError("accepted-source submodule set mismatch")
    pins: dict[str, str] = {}
    for path in submodule_paths:
        entry = modules.get(path)
        commit = entry.get("commit") if isinstance(entry, dict) else None
        if not isinstance(commit, str) or not COMMIT_RE.fullmatch(commit):
            raise ExportError(f"invalid accepted-source submodule commit: {path}")
        pins[path] = commit
    return pins


def _verify_gitlinks(
    repo: Path, firmware_commit: str, pins: Mapping[str, str]
) -> None:
    entries = list_git_entries(repo, firmware_commit, pins)
    if set(entries) != set(pins):
        raise ExportError("firmware gitlink set differs from source manifest")
    for path, expected in pins.items():
        entry = entries[path]
        if entry.mode != "160000" or entry.object_id != expected:
            raise ExportError(f"firmware gitlink mismatch: {path}")


def _sanitized_git_environment(home: Path) -> dict[str, str]:
    env = _safe_git_environment(os.environ)
    home.mkdir(parents=True, exist_ok=True)
    global_config = home / "global.gitconfig"
    system_config = home / "system.gitconfig"
    global_config.write_text("", encoding="utf-8")
    system_config.write_text("", encoding="utf-8")
    env.update(
        {
            "GIT_ATTR_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": str(global_config),
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_SYSTEM": str(system_config),
            "GIT_NO_REPLACE_OBJECTS": "1",
            "HOME": str(home),
            "USERPROFILE": str(home),
            "XDG_CONFIG_HOME": str(home / "xdg"),
        }
    )
    return env


def _initialize_candidate_repository(
    output: Path,
    file_modes: Mapping[str, str],
    pins: Mapping[str, str],
    repo: Path,
    exporter_commit: str,
) -> tuple[str, str]:
    with tempfile.TemporaryDirectory(prefix="pnx-f407-git-") as temporary:
        temporary_root = Path(temporary)
        template = temporary_root / "empty-template"
        template.mkdir()
        env = _sanitized_git_environment(temporary_root / "home")
        _run_git(
            output,
            "init",
            "-q",
            "--object-format=sha1",
            "-b",
            "main",
            f"--template={template}",
            env=env,
        )
        _run_git(
            output,
            "-c",
            "core.autocrlf=false",
            "add",
            "--all",
            env=env,
        )
        for path, mode in sorted(file_modes.items()):
            if mode == "100755":
                _run_git(
                    output,
                    "update-index",
                    "--chmod=+x",
                    "--",
                    path,
                    env=env,
                )
        for path, commit in sorted(pins.items()):
            _run_git(
                output,
                "update-index",
                "--add",
                "--cacheinfo",
                f"160000,{commit},{path}",
                env=env,
            )
        timestamp = _run_git(
            repo, "show", "-s", "--format=%ct", exporter_commit
        ).decode("ascii").strip()
        commit_env = dict(env)
        commit_env.update(
            {
                "GIT_AUTHOR_DATE": f"@{timestamp} +0000",
                "GIT_AUTHOR_EMAIL": "f407-export@local.invalid",
                "GIT_AUTHOR_NAME": "PnX F407 Export Tool",
                "GIT_COMMITTER_DATE": f"@{timestamp} +0000",
                "GIT_COMMITTER_EMAIL": "f407-export@local.invalid",
                "GIT_COMMITTER_NAME": "PnX F407 Export Tool",
            }
        )
        _run_git(
            output,
            "-c",
            "commit.gpgSign=false",
            "commit",
            "-qm",
            "release: materialize F407-only candidate",
            env=commit_env,
        )
        for path in pins:
            (output / Path(*PurePosixPath(path).parts)).mkdir()
        tree = _run_git(output, "rev-parse", "HEAD^{tree}", env=env).decode().strip()
        commit = _run_git(output, "rev-parse", "HEAD", env=env).decode().strip()
        status = _run_git(output, "status", "--short", env=env).decode().strip()
        if status:
            raise ExportError(
                f"candidate repository is dirty after commit: {status}"
            )
        if _run_git(output, "remote", env=env).strip():
            raise ExportError("candidate repository unexpectedly has a remote")
        if _run_git(output, "tag", env=env).strip():
            raise ExportError("candidate repository unexpectedly has a tag")
        return tree, commit


def _remove_tree(path: Path) -> None:
    def make_writable_and_retry(
        function: object, target: str, error_info: object
    ) -> None:
        del error_info
        os.chmod(target, stat.S_IWRITE)
        function(target)  # type: ignore[operator]

    shutil.rmtree(path, onerror=make_writable_and_retry)


def _validate_output_location(repo: Path, output: Path) -> None:
    """Allow in-repository exports only below the approved ignored root."""

    try:
        relative = output.relative_to(repo).as_posix()
    except ValueError:
        return
    approved_root = "build/f407-only-export"
    if not relative.startswith(f"{approved_root}/"):
        raise ExportError(
            "in-repository output must be below the approved ignored "
            f"export root {approved_root}"
        )
    ignored = subprocess.run(
        ["git", "check-ignore", "--quiet", "--no-index", "--", relative],
        cwd=repo,
        capture_output=True,
        env=_safe_git_environment(os.environ),
    )
    if ignored.returncode != 0:
        raise ExportError(
            "in-repository output is not covered by the approved ignored "
            f"export root: {relative}"
        )


def export_candidate(
    repo: Path,
    exporter_commit: str,
    policy_path: str,
    output: Path,
) -> ExportResult:
    """Materialize one deterministic local F407-only Git candidate."""

    repo = repo.resolve()
    output = output.resolve()
    policy_path = validate_relative(policy_path)
    _validate_output_location(repo, output)
    if output.exists():
        raise ExportError(f"output already exists: {output}")
    if not COMMIT_RE.fullmatch(exporter_commit):
        raise ExportError("exporter commit must be a full 40-character SHA")
    head = _run_git(repo, "rev-parse", "HEAD").decode().strip()
    if head != exporter_commit:
        raise ExportError("exporter commit must equal repository HEAD")
    if _run_git(repo, "status", "--short", "--untracked-files=no").strip():
        raise ExportError("tracked exporter worktree must be clean")

    try:
        policy_data = json.loads(
            _git_blob(repo, exporter_commit, policy_path).decode("utf-8")
        )
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ExportError("invalid export policy Git object") from exc
    policy = _validate_policy(policy_data)
    accepted = policy["accepted_source"]
    firmware_commit = accepted["firmware_commit"]
    manifest_commit = accepted["manifest_commit"]
    manifest_path = accepted["manifest_path"]
    manifest = _load_json_blob(
        repo, manifest_commit, manifest_path, "accepted-source manifest"
    )
    submodule_paths = list(policy["submodule_paths"])
    pins = _validate_source_manifest(
        manifest, firmware_commit, submodule_paths
    )
    _verify_gitlinks(repo, firmware_commit, pins)

    output.mkdir(parents=True)
    owners: dict[str, str] = {}
    file_modes: dict[str, str] = {}
    inventory: list[dict[str, object]] = []
    try:
        _materialize_policy_collection(
            repo,
            firmware_commit,
            policy["source_entries"],
            output,
            owners,
            file_modes,
            inventory,
        )
        _materialize_policy_collection(
            repo,
            manifest_commit,
            policy["manifest_entries"],
            output,
            owners,
            file_modes,
            inventory,
        )
        _materialize_policy_collection(
            repo,
            exporter_commit,
            policy["exporter_entries"],
            output,
            owners,
            file_modes,
            inventory,
        )
        _materialize_policy_collection(
            repo,
            exporter_commit,
            [
                {
                    "path": item["path"],
                    "destination": item["destination"],
                    "kind": "file",
                    "mode": item["mode"],
                    "source_role": item["source_role"],
                }
                for item in policy["templates"]
            ],
            output,
            owners,
            file_modes,
            inventory,
        )

        for denied in policy["deny_paths"]:
            if (output / Path(*PurePosixPath(denied).parts)).exists():
                raise ExportError(f"denied path entered candidate: {denied}")

        generated = policy["generated_paths"]
        inventory_path = validate_relative(generated["input_inventory"])
        provenance_path = validate_relative(generated["provenance"])
        for path in (inventory_path, provenance_path):
            if path in owners:
                raise ExportError(f"generated path has an imported owner: {path}")
            file_modes[path] = "100644"

        policy_payload = _git_blob(repo, exporter_commit, policy_path)
        policy_destination = next(
            (
                item["destination"]
                for item in inventory
                if item["source_commit"] == exporter_commit
                and item["source_path"] == policy_path
            ),
            None,
        )
        if not isinstance(policy_destination, str):
            raise ExportError("export policy is not present in candidate inputs")
        inventory_document = {
            "schema_version": 1,
            "manifest_type": "pnx-f407-only-input-inventory",
            "policy": {
                "path": policy_destination,
                "sha256": hashlib.sha256(policy_payload).hexdigest().upper(),
                "source_commit": exporter_commit,
            },
            "files": sorted(inventory, key=lambda item: item["destination"]),
            "gitlinks": [
                {
                    "commit": commit,
                    "destination": path,
                    "git_mode": "160000",
                    "source_commit": firmware_commit,
                    "source_role": "accepted_shared_gitlink",
                }
                for path, commit in sorted(pins.items())
            ],
        }
        _write_json(
            output / Path(*PurePosixPath(inventory_path).parts),
            inventory_document,
        )

        provenance = {
            "schema_version": 1,
            "manifest_type": "pnx-f407-only-export-provenance",
            "status": "unaccepted-unpublished-hardware-unverified-candidate",
            "authoritative_source": {
                "firmware_commit": firmware_commit,
                "source_manifest_commit": manifest_commit,
                "source_manifest_path": manifest_path,
                "exporter_commit": exporter_commit,
                "repository_role": "pnx-h723-f407-multiboard-integration",
            },
            "submodules": {
                path: {
                    "commit": commit,
                    "remote_reachability": "not_rechecked",
                }
                for path, commit in sorted(pins.items())
            },
            "f407_board": manifest.get("f407_board"),
            "software_gates": {
                "source_manifest_evidence": manifest.get("software_gates"),
                "f407_only_software_validation": "NOT_RUN",
            },
            "runtime_gates": {
                **manifest.get("runtime_gates", {}),
                "hardware_gate": "HARDWARE_UNVERIFIED",
            },
            "usb": {
                **manifest.get("usb", {}),
                "descriptor_identity_state": "UNASSIGNED_FAIL_CLOSED",
                "telemetry_abi": 4,
            },
            "build": {
                "firmware_presets": list(policy["firmware_presets"]),
                "host_test_preset": "host-tests",
                "board_selector_required": False,
            },
            "export": {
                "canonical_tree_format": TREE_FORMAT,
                "canonical_tree_sha256": PENDING_HASH,
                "input_inventory_path": inventory_path,
                "policy_path": policy_destination,
                "idempotence": "NOT_RUN",
            },
            "f407_only_distribution": {
                "fresh_clone": "NOT_RUN",
                "hardware": "HARDWARE_UNVERIFIED",
                "software_validation": "NOT_RUN",
                "team_release": "NOT_PUBLISHED",
            },
            "constraints": {
                "hardware_used": False,
                "push_performed": False,
                "remote_modified": False,
                "tag_created": False,
                "vault_ingested": False,
            },
        }
        provenance_target = output / Path(*PurePosixPath(provenance_path).parts)
        _write_json(provenance_target, provenance)
        identity = canonical_tree(output, pins, file_modes)
        provenance["export"]["canonical_tree_sha256"] = identity.sha256
        _write_json(provenance_target, provenance)
        verified = canonical_tree(output, pins, file_modes)
        if verified.sha256 != identity.sha256:
            raise ExportError("canonical identity changed after manifest update")
        git_tree, git_commit = _initialize_candidate_repository(
            output, file_modes, pins, repo, exporter_commit
        )
    except Exception:
        if output.exists():
            _remove_tree(output)
        raise

    return ExportResult(
        output=output,
        canonical_sha256=identity.sha256,
        git_tree=git_tree,
        git_commit=git_commit,
        source_commit=firmware_commit,
        source_manifest_commit=manifest_commit,
        exporter_commit=exporter_commit,
    )


def _canonical_manifest_bytes(path: Path) -> bytes:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        export = data["export"]
        current = export["canonical_tree_sha256"]
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, KeyError, TypeError) as exc:
        raise ExportError(f"invalid candidate provenance manifest: {path}") from exc
    if not isinstance(export, dict) or not isinstance(current, str):
        raise ExportError("candidate canonical hash field must be a string")
    export["canonical_tree_sha256"] = PENDING_HASH
    return (
        json.dumps(data, indent=2, ensure_ascii=False, sort_keys=False) + "\n"
    ).encode("utf-8")


def canonical_tree(
    root: Path,
    gitlinks: Mapping[str, str],
    file_modes: Mapping[str, str] | None = None,
    *,
    ignore_untracked: bool = False,
) -> CanonicalTree:
    """Calculate the canonical candidate content identity."""

    root = root.resolve()
    if not root.is_dir():
        raise ExportError(f"canonical root is not a directory: {root}")
    modes = dict(file_modes or {})
    records: list[dict[str, object]] = []
    folded: dict[str, str] = {}
    gitlink_paths = {
        validate_relative(path): commit for path, commit in gitlinks.items()
    }
    if file_modes is not None and not ignore_untracked:
        actual_files = {
            path.relative_to(root).as_posix()
            for path in root.rglob("*")
            if path.is_file()
            and not path.is_symlink()
            and path.relative_to(root).as_posix() != ".git"
            and not path.relative_to(root).as_posix().startswith(".git/")
            and not any(
                path.relative_to(root).as_posix() == link
                or path.relative_to(root).as_posix().startswith(f"{link}/")
                for link in gitlink_paths
            )
        }
        if actual_files != set(modes):
            extra = sorted(actual_files - set(modes))
            missing = sorted(set(modes) - actual_files)
            raise ExportError(
                "canonical file-mode inventory differs from filesystem "
                f"(extra={extra}, missing={missing})"
            )

    candidates = (
        sorted(root.rglob("*"))
        if file_modes is None
        else [
            root / Path(*PurePosixPath(validate_relative(path)).parts)
            for path in sorted(modes)
        ]
    )
    for candidate in candidates:
        relative = candidate.relative_to(root).as_posix()
        if relative == ".git" or relative.startswith(".git/"):
            continue
        path = validate_relative(relative)
        matching_link = next(
            (
                link
                for link in gitlink_paths
                if path == link or path.startswith(f"{link}/")
            ),
            None,
        )
        if matching_link is not None:
            if (
                path == matching_link
                and candidate.is_dir()
                and not any(candidate.iterdir())
            ):
                continue
            raise ExportError(f"gitlink content was copied into candidate: {path}")
        collision = folded.get(path.casefold())
        if collision is not None and collision != path:
            raise ExportError(f"case-fold path collision: {collision} vs {path}")
        folded[path.casefold()] = path
        if candidate.is_symlink():
            raise ExportError(f"symlink is forbidden in candidate: {path}")
        if candidate.is_dir():
            continue
        if not candidate.is_file():
            raise ExportError(f"unsupported filesystem entry: {path}")
        mode = modes.get(path, "100644")
        if mode not in {"100644", "100755"}:
            raise ExportError(f"unsupported canonical file mode {mode}: {path}")
        payload = (
            _canonical_manifest_bytes(candidate)
            if path == PROVENANCE_PATH
            else candidate.read_bytes()
        )
        records.append(
            {
                "kind": "file",
                "mode": mode,
                "path": path,
                "sha256": hashlib.sha256(payload).hexdigest().upper(),
                "size": len(payload),
            }
        )

    for path, commit in sorted(gitlink_paths.items()):
        if not COMMIT_RE.fullmatch(commit):
            raise ExportError(f"invalid full gitlink commit for {path}: {commit}")
        collision = folded.get(path.casefold())
        if collision is not None:
            raise ExportError(f"file/gitlink path collision: {collision} vs {path}")
        folded[path.casefold()] = path
        records.append(
            {
                "commit": commit,
                "kind": "gitlink",
                "mode": "160000",
                "path": path,
            }
        )

    records.sort(key=lambda item: str(item["path"]))
    serialized = bytearray()
    for item in records:
        if item["kind"] == "file":
            serialized.extend(
                (
                    f"file\t{item['mode']}\t{item['size']}\t"
                    f"{item['sha256']}\t{item['path']}\n"
                ).encode("utf-8")
            )
        else:
            serialized.extend(
                (
                    f"gitlink\t160000\t{item['commit']}\t{item['path']}\n"
                ).encode("utf-8")
            )
    return CanonicalTree(
        format=TREE_FORMAT,
        sha256=hashlib.sha256(serialized).hexdigest().upper(),
        records=tuple(records),
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Export one deterministic local F407-only Git candidate"
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    parser.add_argument(
        "--exporter-commit",
        help="clean exporter implementation commit; defaults to HEAD",
    )
    parser.add_argument(
        "--policy-path",
        default="release/f407-only-export-policy.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo = args.repo_root.resolve()
    exporter_commit = args.exporter_commit
    if exporter_commit is None:
        try:
            exporter_commit = _run_git(repo, "rev-parse", "HEAD").decode().strip()
        except ExportError as exc:
            print(f"F407-only export failed: {exc}", file=sys.stderr)
            return 2
    try:
        result = export_candidate(
            repo,
            exporter_commit,
            args.policy_path,
            args.output,
        )
    except ExportError as exc:
        print(f"F407-only export failed: {exc}", file=sys.stderr)
        return 2
    print(
        json.dumps(
            {
                "canonical_tree_sha256": result.canonical_sha256,
                "candidate_commit": result.git_commit,
                "exporter_commit": result.exporter_commit,
                "firmware_commit": result.source_commit,
                "git_tree": result.git_tree,
                "output": str(result.output),
                "source_manifest_commit": result.source_manifest_commit,
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
