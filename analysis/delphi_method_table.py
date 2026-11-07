#!/usr/bin/env python3
"""Recover Delphi published-method entries from a 32-bit PE image.

Delphi stores an event-handler entry as:
    uint16 entry_size; uint32 code_va; uint8 name_len; char name[name_len]

For the BC3 target, entry_size is 7 + name_len.  This scanner deliberately
requires the code pointer to land in an executable PE section so ordinary
Pascal strings are not reported as methods.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import dataclass, asdict
from pathlib import Path


@dataclass(frozen=True)
class Section:
    name: str
    virtual_address: int
    virtual_size: int
    raw_offset: int
    raw_size: int
    characteristics: int

    @property
    def executable(self) -> bool:
        return bool(self.characteristics & 0x20000000)


@dataclass(frozen=True)
class Method:
    name: str
    code_va: int
    code_file_offset: int | None
    entry_file_offset: int


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def parse_pe32(data: bytes) -> tuple[int, list[Section]]:
    if data[:2] != b"MZ":
        raise ValueError("not an MZ executable")
    pe_offset = u32(data, 0x3C)
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError("invalid PE signature")
    coff = pe_offset + 4
    section_count = u16(data, coff + 2)
    optional_size = u16(data, coff + 16)
    optional = coff + 20
    if u16(data, optional) != 0x10B:
        raise ValueError("only PE32 images are supported")
    image_base = u32(data, optional + 28)
    section_table = optional + optional_size
    sections: list[Section] = []
    for index in range(section_count):
        offset = section_table + index * 40
        raw_name = data[offset : offset + 8].split(b"\0", 1)[0]
        sections.append(
            Section(
                name=raw_name.decode("ascii", "replace"),
                virtual_size=u32(data, offset + 8),
                virtual_address=u32(data, offset + 12),
                raw_size=u32(data, offset + 16),
                raw_offset=u32(data, offset + 20),
                characteristics=u32(data, offset + 36),
            )
        )
    return image_base, sections


def va_to_file_offset(
    va: int, image_base: int, sections: list[Section]
) -> int | None:
    rva = va - image_base
    for section in sections:
        extent = max(section.virtual_size, section.raw_size)
        if section.virtual_address <= rva < section.virtual_address + extent:
            delta = rva - section.virtual_address
            if delta < section.raw_size:
                return section.raw_offset + delta
            return None
    return None


def is_executable_va(va: int, image_base: int, sections: list[Section]) -> bool:
    rva = va - image_base
    return any(
        section.executable
        and section.virtual_address <= rva
        < section.virtual_address + max(section.virtual_size, section.raw_size)
        for section in sections
    )


def scan(data: bytes, image_base: int, sections: list[Section]) -> list[Method]:
    methods: list[Method] = []
    limit = len(data) - 8
    for offset in range(limit):
        entry_size = u16(data, offset)
        if not 8 <= entry_size <= 262:
            continue
        name_len = data[offset + 6]
        if entry_size != 7 + name_len or name_len == 0:
            continue
        end = offset + 7 + name_len
        if end > len(data):
            continue
        raw_name = data[offset + 7 : end]
        if not all(0x20 < byte < 0x7F for byte in raw_name):
            continue
        code_va = u32(data, offset + 2)
        if not is_executable_va(code_va, image_base, sections):
            continue
        methods.append(
            Method(
                name=raw_name.decode("ascii"),
                code_va=code_va,
                code_file_offset=va_to_file_offset(code_va, image_base, sections),
                entry_file_offset=offset,
            )
        )
    return methods


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("pe", type=Path)
    parser.add_argument("patterns", nargs="*", help="case-insensitive name filters")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()

    data = args.pe.read_bytes()
    image_base, sections = parse_pe32(data)
    methods = scan(data, image_base, sections)
    if args.patterns:
        filters = [value.casefold() for value in args.patterns]
        methods = [
            method
            for method in methods
            if any(value in method.name.casefold() for value in filters)
        ]

    if args.json:
        payload = {
            "file": str(args.pe.resolve()),
            "sha256": hashlib.sha256(data).hexdigest(),
            "image_base": image_base,
            "sections": [asdict(section) for section in sections],
            "methods": [asdict(method) for method in methods],
        }
        print(json.dumps(payload, indent=2))
    else:
        print(f"image_base=0x{image_base:08x} methods={len(methods)}")
        for method in methods:
            file_offset = (
                f"0x{method.code_file_offset:08x}"
                if method.code_file_offset is not None
                else "unmapped"
            )
            print(
                f"0x{method.code_va:08x}  file={file_offset}  "
                f"rtti=0x{method.entry_file_offset:08x}  {method.name}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
