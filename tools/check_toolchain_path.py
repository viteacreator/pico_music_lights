#!/usr/bin/env python3
"""Validate Pico SDK PICO_TOOLCHAIN_PATH cache entries semantically."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


def read_cache_entry(cache: Path, name: str) -> tuple[str, str]:
    prefix = f"{name}:"
    for line in cache.read_text(encoding="utf-8").splitlines():
        if line.startswith(prefix):
            type_and_value = line[len(prefix) :]
            if "=" not in type_and_value:
                raise ValueError(f"malformed {name} cache entry: {line}")
            cache_type, value = type_and_value.split("=", 1)
            return cache_type, value
    raise ValueError(f"missing {name} cache entry")


def canonical_existing_dir(path_text: str) -> Path:
    if not path_text:
        raise ValueError("PICO_TOOLCHAIN_PATH is empty")
    path = Path(path_text)
    if not path.is_dir():
        raise ValueError(f"PICO_TOOLCHAIN_PATH does not name an existing directory: {path_text}")
    return path.resolve(strict=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", required=True, type=Path)
    parser.add_argument("--expected-root", required=True, type=Path)
    parser.add_argument("--expected-bin", required=True, type=Path)
    parser.add_argument("--expected-gcc", required=True, type=Path)
    args = parser.parse_args()

    try:
        cache_type, value = read_cache_entry(args.cache, "PICO_TOOLCHAIN_PATH")
        canonical_value = canonical_existing_dir(value)
        expected_root = args.expected_root.resolve(strict=True)
        expected_bin = args.expected_bin.resolve(strict=True)
        expected_gcc = args.expected_gcc.resolve(strict=True)

        if canonical_value == expected_root:
            candidate_gcc = canonical_value / "bin" / "arm-none-eabi-gcc"
            representation = "toolchain-root"
        elif canonical_value == expected_bin:
            candidate_gcc = canonical_value / "arm-none-eabi-gcc"
            representation = "toolchain-bin"
        else:
            raise ValueError(
                "PICO_TOOLCHAIN_PATH resolves outside the validated Arm toolchain: "
                f"cache_type={cache_type} value={value} canonical={canonical_value} "
                f"expected_root={expected_root} expected_bin={expected_bin}"
            )

        if candidate_gcc.resolve(strict=True) != expected_gcc:
            raise ValueError(
                "PICO_TOOLCHAIN_PATH does not resolve to the validated Arm GCC: "
                f"candidate={candidate_gcc} expected_gcc={expected_gcc}"
            )
    except (OSError, ValueError) as exc:
        print(f"toolchain_path_validation=FAIL reason={exc}", file=sys.stderr)
        return 1

    print(
        "toolchain_path_validation=PASS "
        f"cache_type={cache_type} value={value} canonical={canonical_value} "
        f"representation={representation}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
