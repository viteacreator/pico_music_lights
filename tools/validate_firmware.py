#!/usr/bin/env python3
"""Validate Pico firmware artifacts and emit deterministic size reports."""
from __future__ import annotations

import argparse
import pathlib
import re
import struct
import subprocess
import sys
import tempfile

FLASH_START, FLASH_END = 0x10000000, 0x11000000
RAM_RANGES = ((0x20000000, 0x20042000),)
RP2040_FAMILY = 0xE48BFF56


def run(*args: str) -> str:
    return subprocess.run(args, check=True, text=True, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT).stdout


def ihex(path: pathlib.Path) -> dict[int, int]:
    image: dict[int, int] = {}
    base = 0
    eof = False
    for number, raw in enumerate(path.read_text(encoding="ascii").splitlines(), 1):
        if eof or not raw.startswith(":"):
            raise ValueError(f"invalid Intel HEX record at line {number}")
        record = bytes.fromhex(raw[1:])
        if len(record) < 5 or record[0] + 5 != len(record) or sum(record) & 0xFF:
            raise ValueError(f"invalid Intel HEX length/checksum at line {number}")
        count, address, kind = record[0], int.from_bytes(record[1:3], "big"), record[3]
        data = record[4:4 + count]
        if kind == 0:
            for offset, value in enumerate(data):
                absolute = base + address + offset
                if absolute in image:
                    raise ValueError(f"overlapping Intel HEX data at 0x{absolute:x}")
                image[absolute] = value
        elif kind == 1:
            if count or address or number != len(path.read_text(encoding="ascii").splitlines()):
                raise ValueError("invalid or non-final Intel HEX EOF")
            eof = True
        elif kind == 2:
            if count != 2 or address:
                raise ValueError("invalid extended-segment-address record")
            base = int.from_bytes(data, "big") << 4
        elif kind == 4:
            if count != 2 or address:
                raise ValueError("invalid extended-linear-address record")
            base = int.from_bytes(data, "big") << 16
        elif kind in (3, 5):
            if count != 4 or address:
                raise ValueError(f"invalid Intel HEX start-address record type {kind}")
        else:
            raise ValueError(f"unsupported Intel HEX record type {kind}")
    if not eof or not image:
        raise ValueError("Intel HEX has no data or EOF")
    return image


def metadata_has(text: str, label: str, value: str) -> bool:
    return re.search(rf"^\s*{re.escape(label)}:\s+{re.escape(value)}\s*$", text, re.MULTILINE) is not None


def allocated_sections(readelf: str) -> list[tuple[str, int, int]]:
    sections = []
    pattern = re.compile(r"^\s*\[\s*\d+\]\s+(\S+)\s+\S+\s+([0-9a-fA-F]+)\s+"
                         r"[0-9a-fA-F]+\s+([0-9a-fA-F]+)\s+\S+\s+([A-Z]+)")
    for line in readelf.splitlines():
        match = pattern.match(line)
        if match and "A" in match.group(4):
            sections.append((match.group(1), int(match.group(2), 16), int(match.group(3), 16)))
    if not sections:
        raise ValueError("no allocatable ELF sections found")
    return sections


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--tool-prefix", required=True)
    parser.add_argument("--picotool", required=True)
    parser.add_argument("--report", required=True)
    args = parser.parse_args()
    prefix = pathlib.Path(args.prefix)
    artifacts = {suffix: prefix.with_suffix(suffix) for suffix in
                 (".elf", ".uf2", ".bin", ".hex", ".dis")}
    artifacts[".map"] = prefix.with_suffix(".elf.map")
    for name, path in artifacts.items():
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"missing or empty required artifact: {path} ({name})")
    if "<main>:" not in artifacts[".dis"].read_text(encoding="utf-8", errors="replace"):
        raise ValueError("disassembly does not contain the application entry point")

    readelf_tool = args.tool_prefix + "readelf"
    objcopy = args.tool_prefix + "objcopy"
    size_tool = args.tool_prefix + "size"
    nm_tool = args.tool_prefix + "nm"
    header = run(readelf_tool, "-h", str(artifacts[".elf"]))
    attrs = run(readelf_tool, "-A", str(artifacts[".elf"]))
    if "ELF32" not in header or "ARM" not in header or "EABI" not in header:
        raise ValueError("ELF is not ELF32 ARM EABI")
    if "Tag_CPU_arch" not in attrs or not re.search(r"v6|6-M", attrs):
        raise ValueError("ELF lacks RP2040-compatible ARM architecture attributes")

    elf_info = run(args.picotool, "info", "-a", str(artifacts[".elf"]))
    uf2_info = run(args.picotool, "info", "-a", str(artifacts[".uf2"]))
    if not metadata_has(elf_info, "pico_board", "pico_w") or not metadata_has(elf_info, "sdk version", "2.3.0"):
        raise ValueError("ELF picotool metadata does not identify Pico W and SDK 2.3.0")
    if (not re.search(r"family\s+ID 'rp2040'", uf2_info)
            or not metadata_has(uf2_info, "name", "pico_music_lights")
            or not metadata_has(uf2_info, "pico_board", "pico_w")):
        raise ValueError("UF2 picotool metadata does not identify RP2040 Pico W pico_music_lights")

    uf2 = artifacts[".uf2"].read_bytes()
    if len(uf2) % 512:
        raise ValueError("UF2 size is not a multiple of 512")
    blocks = [uf2[i:i + 512] for i in range(0, len(uf2), 512)]
    for index, block in enumerate(blocks):
        fields = struct.unpack_from("<8I", block)
        if fields[0:2] != (0x0A324655, 0x9E5D5157) or struct.unpack_from("<I", block, 508)[0] != 0x0AB16F30:
            raise ValueError(f"invalid UF2 magic in block {index}")
        if not fields[2] & 0x2000 or fields[7] != RP2040_FAMILY:
            raise ValueError(f"UF2 block {index} lacks RP2040 family ID")

    with tempfile.TemporaryDirectory() as temporary:
        rebuilt_bin = pathlib.Path(temporary) / "from-elf.bin"
        rebuilt_hex = pathlib.Path(temporary) / "from-elf.hex"
        run(objcopy, "-O", "binary", str(artifacts[".elf"]), str(rebuilt_bin))
        run(objcopy, "-O", "ihex", str(artifacts[".elf"]), str(rebuilt_hex))
        if rebuilt_bin.read_bytes() != artifacts[".bin"].read_bytes():
            raise ValueError("generated BIN differs from ELF reconstruction")
        if ihex(rebuilt_hex) != ihex(artifacts[".hex"]):
            raise ValueError("generated HEX image differs from ELF reconstruction")

    sections = allocated_sections(run(readelf_tool, "-SW", str(artifacts[".elf"])))
    for name, address, size in sections:
        end = address + size
        in_flash = FLASH_START <= address and end <= FLASH_END
        in_ram = any(start <= address and end <= limit for start, limit in RAM_RANGES)
        if size and not (in_flash or in_ram):
            raise ValueError(f"allocatable section {name} lies outside RP2040 memory")
    persistent_end = FLASH_START + 2 * 1024 * 1024
    persistent_start = persistent_end - 8192
    application_image_end = max((address + size for _, address, size in sections
                                 if FLASH_START <= address < FLASH_END), default=FLASH_START)
    if application_image_end > persistent_start:
        raise ValueError(f"application image overlaps persistent region: {application_image_end:#x} > {persistent_start:#x}")
    flash = sum(size for _, address, size in sections
                if FLASH_START <= address and address + size <= FLASH_END)
    ram = sum(size for _, address, size in sections
              if any(start <= address and address + size <= end for start, end in RAM_RANGES))
    gnu_lines = run(size_tool, str(artifacts[".elf"])).splitlines()
    if len(gnu_lines) < 2:
        raise ValueError("GNU size did not report text/data/bss")
    values = gnu_lines[-1].split()
    text_size, data_size, bss_size, total = map(int, values[:4])

    symbols = []
    for line in run(nm_tool, "-S", "--size-sort", "--radix=d", str(artifacts[".elf"])).splitlines():
        fields = line.split(maxsplit=3)
        if (len(fields) == 4 and fields[1].isdigit() and int(fields[1])
                and fields[3] != "end" and not fields[3].startswith("__")):
            address, size, kind, name = int(fields[0]), int(fields[1]), fields[2], fields[3]
            symbols.append((address, size, kind, name))
    flash_symbols = sorted((s for s in symbols if FLASH_START <= s[0] < FLASH_END), key=lambda x: (-x[1], x[3]))[:10]
    ram_symbols = sorted((s for s in symbols if any(a <= s[0] < b for a, b in RAM_RANGES)), key=lambda x: (-x[1], x[3]))[:10]

    lines = [
        "firmware_validation=PASS",
        f"gnu_text_bytes={text_size}", f"gnu_data_bytes={data_size}",
        f"gnu_bss_bytes={bss_size}", f"gnu_total_bytes={total}",
        f"address_flash_alloc_bytes={flash}", f"address_ram_alloc_bytes={ram}",
        f"application_image_end={application_image_end:#010x}",
        f"persistent_region_start={persistent_start:#010x}",
        f"persistent_region_end={persistent_end:#010x}",
        f"slot_a_start={persistent_start:#010x}",
        f"slot_a_end={persistent_start + 4096:#010x}",
        f"slot_b_start={persistent_start + 4096:#010x}",
        f"slot_b_end={persistent_end:#010x}",
        "persistent_overlap=false",
        "schema_payload_buffer_bytes=978",
        "slot_buffer_bytes=4096",
    ]
    lines.extend(f"artifact_{suffix[1:]}_bytes={path.stat().st_size}" for suffix, path in artifacts.items())
    lines.append("largest_flash_symbols=size,address,type,name")
    lines.extend(f"  {size},{address:#010x},{kind},{name}" for address, size, kind, name in flash_symbols)
    lines.append("largest_ram_symbols=size,address,type,name")
    lines.extend(f"  {size},{address:#010x},{kind},{name}" for address, size, kind, name in ram_symbols)
    pathlib.Path(args.report).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"firmware validation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
