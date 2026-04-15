#!/usr/bin/env python3
"""Convert an SM83 ELF executable to a Game Boy ROM (.gb) file.

Usage:
    python3 make-gb-rom.py input.elf -o output.gb [--title TITLE]
    python3 make-gb-rom.py --check-only output.gb

The linker script (sm83.ld) marks the ROM header ($0100-$014F) as NOLOAD.
This tool fills in the entry point, Nintendo logo, title, and checksums.
"""

import argparse
import struct
import sys

# The Nintendo logo bitmap — must match exactly or the boot ROM hangs.
# 48 bytes at ROM offset $0104-$0133.
NINTENDO_LOGO = bytes([
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
])

ROM_SIZE = 32768  # 32 KB, no MBC


def parse_elf32(data):
    """Parse a 32-bit little-endian ELF and return its loadable segments."""
    if data[:4] != b'\x7fELF':
        raise ValueError("Not an ELF file")
    if data[4] != 1:
        raise ValueError("Not a 32-bit ELF (expected ELFCLASS32)")
    if data[5] != 1:
        raise ValueError("Not little-endian (expected ELFDATA2LSB)")

    e_phoff = struct.unpack_from('<I', data, 28)[0]
    e_phentsize = struct.unpack_from('<H', data, 42)[0]
    e_phnum = struct.unpack_from('<H', data, 44)[0]

    segments = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        (p_type, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz,
         p_flags, p_align) = struct.unpack_from('<IIIIIIII', data, off)
        # PT_LOAD = 1
        if p_type == 1 and p_filesz > 0:
            segments.append({
                'vaddr': p_vaddr,
                'paddr': p_paddr,
                'filesz': p_filesz,
                'memsz': p_memsz,
                'data': data[p_offset:p_offset + p_filesz],
            })
    return segments


def build_rom(elf_data, title="SM83PROG"):
    """Build a 32KB Game Boy ROM from ELF loadable segments."""
    rom = bytearray(ROM_SIZE)

    segments = parse_elf32(elf_data)
    if not segments:
        raise ValueError("No loadable segments found in ELF")

    # Copy loadable segments into the ROM image.
    # Only segments with physical addresses in ROM range ($0000-$7FFF) are copied.
    for seg in segments:
        addr = seg['paddr']
        size = seg['filesz']
        if addr >= ROM_SIZE:
            continue  # WRAM/HRAM segment — skip (initialized at runtime)
        end = addr + size
        if end > ROM_SIZE:
            raise ValueError(
                f"Segment at ${addr:04X} extends past ROM end "
                f"(${end:04X} > ${ROM_SIZE:04X})")
        rom[addr:addr + size] = seg['data']

    # Entry point at $0100: NOP; JP $0150
    rom[0x0100] = 0x00        # NOP
    rom[0x0101] = 0xC3        # JP
    rom[0x0102] = 0x50        # lo($0150)
    rom[0x0103] = 0x01        # hi($0150)

    # Nintendo logo at $0104-$0133
    rom[0x0104:0x0134] = NINTENDO_LOGO

    # Title at $0134-$0143 (up to 16 bytes, zero-padded)
    title_bytes = title.encode('ascii')[:16]
    rom[0x0134:0x0134 + len(title_bytes)] = title_bytes

    # CGB flag at $0143: $80 = CGB-compatible (also plays on DMG)
    rom[0x0143] = 0x80

    # Cartridge type at $0147: $00 = ROM only (no MBC)
    rom[0x0147] = 0x00

    # ROM size at $0148: $00 = 32 KB (2 banks)
    rom[0x0148] = 0x00

    # RAM size at $0149: $00 = no external RAM
    rom[0x0149] = 0x00

    # Destination code at $014A: $01 = non-Japanese
    rom[0x014A] = 0x01

    # Old licensee code at $014B: $33 = use new licensee code
    rom[0x014B] = 0x33

    # Header checksum at $014D: complement sum of $0134-$014C
    hdr_sum = 0
    for b in rom[0x0134:0x014D]:
        hdr_sum = (hdr_sum - b - 1) & 0xFF
    rom[0x014D] = hdr_sum

    # Global checksum at $014E-$014F: sum of all bytes except $014E-$014F
    glob_sum = 0
    for i in range(ROM_SIZE):
        if i == 0x014E or i == 0x014F:
            continue
        glob_sum = (glob_sum + rom[i]) & 0xFFFF
    rom[0x014E] = (glob_sum >> 8) & 0xFF  # big-endian
    rom[0x014F] = glob_sum & 0xFF

    return bytes(rom)


def check_rom(rom_data):
    """Validate a Game Boy ROM's checksums and structure."""
    errors = []

    if len(rom_data) < ROM_SIZE:
        errors.append(f"ROM too small: {len(rom_data)} bytes (expected {ROM_SIZE})")
        return errors

    # Nintendo logo
    logo = rom_data[0x0104:0x0134]
    if logo != NINTENDO_LOGO:
        errors.append("Nintendo logo mismatch (boot ROM would reject this)")

    # Entry point
    entry = rom_data[0x0100:0x0104]
    if entry != b'\x00\xC3\x50\x01':
        errors.append(
            f"Entry point: got {entry.hex()} "
            f"(expected 00c35001 = NOP; JP $0150)")

    # Header checksum
    hdr_sum = 0
    for b in rom_data[0x0134:0x014D]:
        hdr_sum = (hdr_sum - b - 1) & 0xFF
    if rom_data[0x014D] != hdr_sum:
        errors.append(
            f"Header checksum: got ${rom_data[0x014D]:02X}, "
            f"expected ${hdr_sum:02X}")

    # Global checksum
    glob_sum = 0
    for i in range(len(rom_data)):
        if i == 0x014E or i == 0x014F:
            continue
        glob_sum = (glob_sum + rom_data[i]) & 0xFFFF
    stored = (rom_data[0x014E] << 8) | rom_data[0x014F]
    if stored != glob_sum:
        errors.append(
            f"Global checksum: got ${stored:04X}, expected ${glob_sum:04X}")

    return errors


def main():
    parser = argparse.ArgumentParser(
        description="Convert SM83 ELF to Game Boy ROM")
    parser.add_argument("input", help="Input ELF file (or .gb for --check-only)")
    parser.add_argument("-o", "--output", help="Output .gb ROM file")
    parser.add_argument("--title", default="SM83PROG",
                        help="ROM title (max 16 chars, default: SM83PROG)")
    parser.add_argument("--check-only", action="store_true",
                        help="Validate an existing .gb file's checksums")
    args = parser.parse_args()

    if args.check_only:
        with open(args.input, 'rb') as f:
            rom_data = f.read()
        errors = check_rom(rom_data)
        if errors:
            for e in errors:
                print(f"FAIL: {e}", file=sys.stderr)
            sys.exit(1)
        else:
            print(f"OK: {args.input} ({len(rom_data)} bytes, checksums valid)")
            sys.exit(0)

    if not args.output:
        parser.error("--output is required (or use --check-only)")

    with open(args.input, 'rb') as f:
        elf_data = f.read()

    rom = build_rom(elf_data, title=args.title)

    with open(args.output, 'wb') as f:
        f.write(rom)

    # Self-check
    errors = check_rom(rom)
    if errors:
        for e in errors:
            print(f"WARNING: {e}", file=sys.stderr)
    else:
        print(f"Wrote {args.output} ({len(rom)} bytes, checksums valid)")


if __name__ == '__main__':
    main()
