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

ROM_SIZE = 32768  # Default for ROM-only carts. MBC1 expands via --rom-banks.

# Cartridge type byte ($0147).
CART_ROM_ONLY = 0x00
CART_MBC1     = 0x01
CART_MBC3     = 0x13  # MBC3 + RAM + BATTERY (no RTC runtime yet)
CART_MBC5     = 0x1B  # MBC5 + RAM + BATTERY

# CGB flag byte ($0143).
CGB_DMG_ONLY      = 0x00  # DMG (monochrome) only; CGB boots in compat mode.
CGB_COMPATIBLE    = 0x80  # Plays on both DMG and CGB.
CGB_ONLY          = 0xC0  # CGB-exclusive; DMG refuses to boot.

# ROM-size byte ($0148): encodes number of 16 KB banks as log2(banks)-1.
#   0x00 = 2 banks (32 KB)   0x04 = 32 banks (512 KB)
#   0x01 = 4 banks (64 KB)   0x05 = 64 banks (1 MB)
#   0x02 = 8 banks (128 KB)  0x06 = 128 banks (2 MB)
#   0x03 = 16 banks (256 KB) 0x07 = 256 banks (4 MB)
ROM_BANK_BYTE = {2: 0x00, 4: 0x01, 8: 0x02, 16: 0x03, 32: 0x04,
                 64: 0x05, 128: 0x06, 256: 0x07, 512: 0x08}


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


def extract_symbols(data):
    """Extract (address, name) pairs from an ELF32 symbol table.

    Returns a list of (addr, name) tuples for defined, named symbols that are
    either functions or data objects with a non-zero address. Emission format
    for GB debuggers (BGB / Emulicious / no$gmb) is bank:addr, so callers wrap
    this with a bank prefix (always 00 for non-MBC builds).
    """
    if data[:4] != b'\x7fELF' or data[4] != 1 or data[5] != 1:
        raise ValueError("extract_symbols expects a 32-bit LE ELF")

    e_shoff = struct.unpack_from('<I', data, 32)[0]
    e_shentsize = struct.unpack_from('<H', data, 46)[0]
    e_shnum = struct.unpack_from('<H', data, 48)[0]
    e_shstrndx = struct.unpack_from('<H', data, 50)[0]

    # Read all section headers. Fields (40 bytes each, all u32):
    #   sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size,
    #   sh_link, sh_info, sh_addralign, sh_entsize
    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sections.append(struct.unpack_from('<IIIIIIIIII', data, off))

    # Section name string table — used only to identify .symtab/.strtab by
    # type, not by name, so we don't actually need .shstrtab here.

    # Locate the first SHT_SYMTAB (type=2) and its linked .strtab (via sh_link).
    symtab_idx = None
    for i, s in enumerate(sections):
        if s[1] == 2:  # SHT_SYMTAB
            symtab_idx = i
            break
    if symtab_idx is None:
        return []

    symtab = sections[symtab_idx]
    strtab_idx = symtab[6]  # sh_link
    if strtab_idx >= len(sections):
        return []
    strtab = sections[strtab_idx]

    sym_data = data[symtab[4]:symtab[4] + symtab[5]]
    str_data = data[strtab[4]:strtab[4] + strtab[5]]
    entsize = symtab[9] if symtab[9] else 16

    def read_cstr(buf, off):
        end = buf.find(b'\x00', off)
        return buf[off:end].decode('ascii', errors='replace') if end >= 0 else ''

    symbols = []
    # Skip the first entry (always the undefined symbol).
    for off in range(entsize, len(sym_data), entsize):
        (st_name, st_value, st_size, st_info,
         st_other, st_shndx) = struct.unpack_from('<IIIBBH', sym_data, off)
        # SHN_UNDEF=0, SHN_ABS=0xFFF1: skip undefined; keep absolute.
        if st_shndx == 0:
            continue
        # STT_FUNC=2, STT_OBJECT=1, STT_NOTYPE=0 — accept all three.
        stype = st_info & 0xF
        if stype not in (0, 1, 2):
            continue
        # STB_LOCAL=0, STB_GLOBAL=1, STB_WEAK=2 — skip locals (compiler noise).
        sbind = st_info >> 4
        if sbind == 0:
            continue
        name = read_cstr(str_data, st_name)
        if not name or name.startswith('$') or name.startswith('.L'):
            continue
        symbols.append((st_value, name))

    symbols.sort()
    return symbols


def emit_sym_file(symbols, path):
    """Write a BGB/Emulicious/no$gmb-compatible .sym file.

    Format: one symbol per line as `BB:AAAA name` where BB is the ROM bank
    (always 00 for non-MBC builds) and AAAA is the 16-bit address in upper
    hex. Non-ROM symbols (WRAM $C000+, HRAM $FF80+) use their raw address.
    """
    with open(path, 'w') as f:
        f.write('; Generated by make-gb-rom.py\n')
        for addr, name in symbols:
            f.write(f'00:{addr:04X} {name}\n')


def build_rom(elf_data, title="SM83PROG", mbc=CART_ROM_ONLY, rom_banks=2,
              cgb_flag=CGB_COMPATIBLE):
    """Build a Game Boy ROM from ELF loadable segments.

    mbc: cartridge type byte ($0147). Default ROM-only.
    rom_banks: number of 16 KB banks in the output file. Must be ≥ 2 (one
      fixed bank 0 + at least one bank 1). 2 = 32 KB ROM-only default.
    cgb_flag: $0143 byte. CGB_COMPATIBLE (default) keeps existing behaviour.
    """
    if rom_banks < 2 or rom_banks not in ROM_BANK_BYTE:
        raise ValueError(
            f"Unsupported --rom-banks {rom_banks}; pick one of "
            f"{sorted(b for b in ROM_BANK_BYTE if b >= 2)}")
    rom_size = rom_banks * 0x4000
    # Default-fill banks 1+ with 0xFF (matches every real MBC cart).
    rom = bytearray(b'\xFF' * rom_size)
    # Bank 0 is filled with 0x00 by hardware convention; pre-zero it so
    # the header region stays clean.
    rom[0:0x4000] = bytearray(0x4000)

    segments = parse_elf32(elf_data)
    if not segments:
        raise ValueError("No loadable segments found in ELF")

    # Copy loadable segments into the ROM image using paddr (LMA) as the
    # file offset. Any segment with paddr ≥ rom_size points at WRAM/HRAM
    # and is initialised at runtime by crt0.
    for seg in segments:
        addr = seg['paddr']
        size = seg['filesz']
        if addr >= rom_size:
            continue  # WRAM/HRAM segment — skip (initialised at runtime)
        end = addr + size
        if end > rom_size:
            raise ValueError(
                f"Segment at ${addr:04X} extends past ROM end "
                f"(${end:04X} > ${rom_size:04X}). Increase --rom-banks.")
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

    # CGB flag at $0143: 0x00/0x80/0xC0 per --dmg-only / default / --cgb-only.
    rom[0x0143] = cgb_flag

    # Cartridge type at $0147: ROM-only / MBC1 / MBC3 / MBC5.
    rom[0x0147] = mbc

    # ROM size at $0148: log2(banks)-1 form.
    rom[0x0148] = ROM_BANK_BYTE[rom_banks]

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

    # Global checksum at $014E-$014F: sum of all bytes except $014E-$014F,
    # covering the entire ROM file (all banks).
    glob_sum = 0
    for i in range(rom_size):
        if i == 0x014E or i == 0x014F:
            continue
        glob_sum = (glob_sum + rom[i]) & 0xFFFF
    rom[0x014E] = (glob_sum >> 8) & 0xFF  # big-endian
    rom[0x014F] = glob_sum & 0xFF

    return bytes(rom)


def check_rom(rom_data):
    """Validate a Game Boy ROM's checksums and structure."""
    errors = []

    if len(rom_data) < 0x8000 or len(rom_data) % 0x4000 != 0:
        errors.append(
            f"ROM size {len(rom_data)} bytes is not a valid multiple of "
            f"16 KB ≥ 32 KB")
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
    parser.add_argument("--emit-sym", metavar="PATH",
                        help="Also write a BGB/Emulicious-format .sym file")
    mbc_group = parser.add_mutually_exclusive_group()
    mbc_group.add_argument("--mbc1", action="store_const",
                           dest="mbc", const=CART_MBC1,
                           help="Stamp cartridge type $0147 = MBC1 ($01)")
    mbc_group.add_argument("--mbc3", action="store_const",
                           dest="mbc", const=CART_MBC3,
                           help="Stamp cartridge type $0147 = MBC3+RAM+BATTERY ($13)")
    mbc_group.add_argument("--mbc5", action="store_const",
                           dest="mbc", const=CART_MBC5,
                           help="Stamp cartridge type $0147 = MBC5+RAM+BATTERY ($1B)")
    parser.set_defaults(mbc=CART_ROM_ONLY)
    parser.add_argument("--rom-banks", type=int, default=2,
                        choices=sorted(b for b in ROM_BANK_BYTE if b >= 2),
                        help="Number of 16 KB ROM banks (default: 2 = 32 KB)")
    cgb_group = parser.add_mutually_exclusive_group()
    cgb_group.add_argument("--cgb-only", action="store_const",
                           dest="cgb_flag", const=CGB_ONLY,
                           help="Stamp $0143 = 0xC0 (CGB-exclusive; DMG refuses)")
    cgb_group.add_argument("--dmg-only", action="store_const",
                           dest="cgb_flag", const=CGB_DMG_ONLY,
                           help="Stamp $0143 = 0x00 (DMG/monochrome)")
    parser.set_defaults(cgb_flag=CGB_COMPATIBLE)
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

    rom = build_rom(elf_data, title=args.title, mbc=args.mbc,
                    rom_banks=args.rom_banks, cgb_flag=args.cgb_flag)

    with open(args.output, 'wb') as f:
        f.write(rom)

    if args.emit_sym:
        symbols = extract_symbols(elf_data)
        emit_sym_file(symbols, args.emit_sym)
        print(f"Wrote {args.emit_sym} ({len(symbols)} symbols)")

    # Self-check
    errors = check_rom(rom)
    if errors:
        for e in errors:
            print(f"WARNING: {e}", file=sys.stderr)
    else:
        print(f"Wrote {args.output} ({len(rom)} bytes, checksums valid)")


if __name__ == '__main__':
    main()
