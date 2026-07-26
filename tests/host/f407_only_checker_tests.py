#!/usr/bin/env python3
"""Regression tests for the promoted pure-F407 static checker."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "scripts"))

import check_f407_only  # noqa: E402


class F407OnlyCheckerTests(unittest.TestCase):
    def test_dji_c_board_artifact_path_is_allowed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "README.md").write_text(
                "Use build/dji-c-board/pnx_embedded.elf.\n",
                encoding="utf-8",
            )

            errors = check_f407_only.check_delivery_surface(root)

        self.assertNotIn(
            "forbidden board/Core/main.c-style path in README.md",
            errors,
        )

    def test_promoted_workspace_mode_skips_immutable_candidate_identity(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for relative in check_f407_only.SURFACE_FILES:
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                content = (
                    '{"version": 6, "configurePresets": []}\n'
                    if relative == "CMakePresets.json"
                    else "pure F407 workspace\n"
                )
                target.write_text(content, encoding="utf-8")
            for relative in check_f407_only.REQUIRED_LICENSES:
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text("retained license\n", encoding="utf-8")
            (root / ".gitmodules").write_text(
                '[submodule "pnx_bsp"]\n'
                "\tpath = pnx_bsp\n"
                "\turl = https://example.invalid/pnx_bsp.git\n",
                encoding="utf-8",
            )

            completed = subprocess.run(
                [
                    sys.executable,
                    str(REPO_ROOT / "scripts" / "check_f407_only.py"),
                    "--repo-root",
                    str(root),
                    "--promoted-workspace",
                ],
                capture_output=True,
                text=True,
            )

        self.assertEqual(
            0,
            completed.returncode,
            completed.stdout + completed.stderr,
        )

    def test_negative_h723_status_and_provenance_are_allowed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "README.md").write_text(
                "PURE_F407_NO_H723=PASS\n"
                "Historical multiboard authority retains H723.\n",
                encoding="utf-8",
            )

            errors = check_f407_only.check_delivery_surface(root)

        self.assertNotIn("H723 user/build entry in README.md", errors)

    def test_h723_preset_command_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "README.md").write_text(
                "Run cmake --preset h723-debug.\n",
                encoding="utf-8",
            )

            errors = check_f407_only.check_delivery_surface(root)

        self.assertTrue(
            any("H723" in error for error in errors),
            errors,
        )


if __name__ == "__main__":
    unittest.main()
