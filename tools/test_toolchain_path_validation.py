#!/usr/bin/env python3
"""Focused regression checks for Pico toolchain-path validation."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile


def run_case(script: Path, cache: Path, root: Path, bin_dir: Path, gcc: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            "python3",
            str(script),
            "--cache",
            str(cache),
            "--expected-root",
            str(root),
            "--expected-bin",
            str(bin_dir),
            "--expected-gcc",
            str(gcc),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def write_cache(path: Path, cache_type: str, value: Path) -> None:
    path.write_text(f"PICO_TOOLCHAIN_PATH:{cache_type}={value}\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--script", required=True, type=Path)
    parser.add_argument("--expected-root", required=True, type=Path)
    parser.add_argument("--expected-bin", required=True, type=Path)
    parser.add_argument("--expected-gcc", required=True, type=Path)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory(prefix="pml-toolchain-path-test.") as tmp_text:
        tmp = Path(tmp_text)
        cases = [
            ("codex-cloud-root", "INTERNAL", args.expected_root, 0, "representation=toolchain-root"),
            ("github-actions-bin", "PATH", args.expected_bin, 0, "representation=toolchain-bin"),
        ]
        unrelated = tmp / "unrelated-toolchain"
        unrelated.mkdir()
        cases.append(("unrelated-path", "PATH", unrelated, 1, "toolchain_path_validation=FAIL"))

        for name, cache_type, value, expected_rc, expected_text in cases:
            cache = tmp / f"{name}.cache"
            write_cache(cache, cache_type, value)
            result = run_case(args.script, cache, args.expected_root, args.expected_bin, args.expected_gcc)
            combined = result.stdout + result.stderr
            if result.returncode != expected_rc or expected_text not in combined:
                print(f"toolchain_path_regression=FAIL case={name}")
                print(combined)
                return 1
            print(f"toolchain_path_regression=PASS case={name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
