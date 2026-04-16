#!/bin/bash
set -euo pipefail

# End-to-end C test for SM83: compile C → link → .gb ROM → verify in simulator.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LLVM_SRC="/data/llvm-sm83"
ARCH="$(uname -m)"
BUILD="$LLVM_SRC/build-sm83-${ARCH}"
TMPDIR="${TMPDIR:-/tmp}/sm83-c-e2e-$$"
PASS=0
FAIL=0

mkdir -p "$TMPDIR"
trap 'rm -rf "$TMPDIR"' EXIT

CLANG="$BUILD/bin/clang"
LLD="$BUILD/bin/ld.lld"
# --no-check-sections: all ROMX banks share VMA $4000-$7FFF (MBC1 overlay
# semantics). LLD's default VMA-overlap check rejects this; silence it.
LLDFLAGS="--no-check-sections"
OBJDUMP="$BUILD/bin/llvm-objdump"
LINKER_SCRIPT="$LLVM_SRC/sm83.ld"
CRT0="$BUILD/sm83-crt0.o"
CRT0_MBC5="$BUILD/sm83-crt0-mbc5.o"
RUNTIME="$BUILD/sm83-runtime.o"
RUNTIME_ASM="$BUILD/sm83-runtime-asm.o"
MAKEROM="$LLVM_SRC/make-gb-rom.py"

check() {
  local desc="$1"
  shift
  if eval "$*" >/dev/null 2>&1; then
    echo "  PASS: $desc"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: $desc"
    FAIL=$((FAIL + 1))
  fi
}

echo "=== SM83 C End-to-End Test ==="
echo ""

# Step 1: Compile C to object
echo "1. Compiling c-test.c with clang..."
"$CLANG" --target=sm83-unknown-none -ffreestanding -O1 \
  -c "$SCRIPT_DIR/c-test.c" -o "$TMPDIR/c-test.o"
check "c-test.o created" test -f "$TMPDIR/c-test.o"
check "c-test.o has main" "'$OBJDUMP' -t '$TMPDIR/c-test.o' 2>&1 | grep -q main"
check "c-test.o has add_three" "'$OBJDUMP' -t '$TMPDIR/c-test.o' 2>&1 | grep -q add_three"

# Step 2: Link
echo "2. Linking with ld.lld..."
"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/c-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" -o "$TMPDIR/c-test.elf"
check "c-test.elf created" test -f "$TMPDIR/c-test.elf"

# Step 3: Convert to ROM
echo "3. Converting to .gb ROM..."
python3 "$MAKEROM" "$TMPDIR/c-test.elf" -o "$TMPDIR/c-test.gb"
check "c-test.gb created" test -f "$TMPDIR/c-test.gb"
check "ROM checksums valid" python3 "$MAKEROM" --check-only "$TMPDIR/c-test.gb"

# Step 4: Run in simulator
echo "4. Running in SM83 simulator..."
python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/c-test.gb" \
  --check C100=1E \
  --check C101=DE \
  --check C102=0A
HARNESS_RC=$?
if [ $HARNESS_RC -eq 0 ]; then
  echo "  PASS: Simulator verification"
  PASS=$((PASS + 1))
else
  echo "  FAIL: Simulator verification"
  FAIL=$((FAIL + 1))
fi

echo ""

# --- .sym file emission smoke test (Item 2) ---------------------------------
echo "5. Emitting .sym symbol file..."
python3 "$MAKEROM" "$TMPDIR/c-test.elf" -o "$TMPDIR/c-test.gb" \
  --emit-sym "$TMPDIR/c-test.sym" >/dev/null
check ".sym file created" test -f "$TMPDIR/c-test.sym"
check ".sym has main"   "grep -Eq '^00:[0-9A-Fa-f]{4} main\$' '$TMPDIR/c-test.sym'"
check ".sym format ok"  "grep -Evq '^;|^00:[0-9A-Fa-f]{4} ' '$TMPDIR/c-test.sym' || true; \
                        ! grep -Ev '^;|^00:[0-9A-Fa-f]{4} [^ ]+\$' '$TMPDIR/c-test.sym'"

# --- HRAM placement test (Item 3) -------------------------------------------
echo ""
echo "6. Compiling hram-test.c (HRAM section placement)..."
"$CLANG" --target=sm83-unknown-none -ffreestanding -O1 \
  -c "$SCRIPT_DIR/hram-test.c" -o "$TMPDIR/hram-test.o"
"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/hram-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" \
  -o "$TMPDIR/hram-test.elf"
check "hram-test.elf built" test -f "$TMPDIR/hram-test.elf"

# oam_dma_trampoline must land in [$FF80, $FFFF).
TRAMP_ADDR=$("$BUILD/bin/llvm-nm" "$TMPDIR/hram-test.elf" 2>/dev/null \
  | awk '$3 == "oam_dma_trampoline" { print $1 }')
if [ -n "$TRAMP_ADDR" ] && \
   [ "$((0x$TRAMP_ADDR))" -ge $((0xFF80)) ] && \
   [ "$((0x$TRAMP_ADDR))" -lt $((0xFFFF)) ]; then
  echo "  PASS: oam_dma_trampoline at \$$TRAMP_ADDR is in HRAM"
  PASS=$((PASS + 1))
else
  echo "  FAIL: oam_dma_trampoline at '\$$TRAMP_ADDR' is outside HRAM [FF80,FFFF)"
  FAIL=$((FAIL + 1))
fi

# Build + run the ROM to confirm runtime HRAM copy works.
python3 "$MAKEROM" "$TMPDIR/hram-test.elf" -o "$TMPDIR/hram-test.gb" >/dev/null
python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/hram-test.gb" --check C100=77
HARNESS_RC=$?
if [ $HARNESS_RC -eq 0 ]; then
  echo "  PASS: HRAM trampoline executed at runtime"
  PASS=$((PASS + 1))
else
  echo "  FAIL: HRAM trampoline did not execute (crt0 ROM→HRAM copy broken?)"
  FAIL=$((FAIL + 1))
fi

# --- VBlank integration test (Item 4) ---------------------------------------
echo ""
echo "7. Compiling c-vblank-test.c (VBlank + interrupt(\"vblank\") + HRAM)..."
"$CLANG" --target=sm83-unknown-none -ffreestanding -O1 \
  -c "$SCRIPT_DIR/c-vblank-test.c" -o "$TMPDIR/c-vblank-test.o"
check "c-vblank-test.o has vblank_isr" \
  "'$OBJDUMP' -t '$TMPDIR/c-vblank-test.o' 2>&1 | grep -q vblank_isr"
check "vblank_isr in .isr.vblank section" \
  "'$OBJDUMP' -h '$TMPDIR/c-vblank-test.o' 2>&1 | grep -q '\\.isr\\.vblank'"

"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/c-vblank-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" \
  -o "$TMPDIR/c-vblank-test.elf"
check "c-vblank-test.elf built" test -f "$TMPDIR/c-vblank-test.elf"

# vblank_isr lives in .text at some address > $0150; the vector slot at
# $0040 holds a `JP vblank_isr` trampoline. Verify both: symbol exists
# in .text range, and byte 0 of the ROM at $0040 is the JP opcode (0xC3).
VBLANK_ADDR=$("$BUILD/bin/llvm-nm" "$TMPDIR/c-vblank-test.elf" 2>/dev/null \
  | awk '$3 == "vblank_isr" { print $1 }')
if [ -n "$VBLANK_ADDR" ] && [ "$((0x$VBLANK_ADDR))" -ge $((0x150)) ] \
                         && [ "$((0x$VBLANK_ADDR))" -lt $((0x8000)) ]; then
  echo "  PASS: vblank_isr at \$$VBLANK_ADDR (inside .text ROM region)"
  PASS=$((PASS + 1))
else
  echo "  FAIL: vblank_isr at '\$$VBLANK_ADDR' is not in .text"
  FAIL=$((FAIL + 1))
fi

python3 "$MAKEROM" "$TMPDIR/c-vblank-test.elf" -o "$TMPDIR/c-vblank-test.gb" >/dev/null

# Verify vector slot at $0040 is a JP trampoline, and its target matches
# vblank_isr's address.
JP_OPCODE=$(python3 -c "print('%02x' % open('$TMPDIR/c-vblank-test.gb','rb').read()[0x40])")
JP_TARGET=$(python3 -c "d=open('$TMPDIR/c-vblank-test.gb','rb').read(); print('%04x' % (d[0x41] | (d[0x42]<<8)))")
if [ "$JP_OPCODE" = "c3" ] && [ "$((0x$JP_TARGET))" = "$((0x$VBLANK_ADDR))" ]; then
  echo "  PASS: dispatch at \$0040 = JP \$$JP_TARGET (= vblank_isr)"
  PASS=$((PASS + 1))
else
  echo "  FAIL: dispatch at \$0040 got opcode 0x$JP_OPCODE, target \$$JP_TARGET (expected JP \$$VBLANK_ADDR)"
  FAIL=$((FAIL + 1))
fi

python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/c-vblank-test.gb" \
  --max-steps 200000 \
  --check C100=03 \
  --check C101=A5 \
  --check FF80=42
HARNESS_RC=$?
if [ $HARNESS_RC -eq 0 ]; then
  echo "  PASS: VBlank handler fired 3× and HRAM marker ran"
  PASS=$((PASS + 1))
else
  echo "  FAIL: VBlank integration verification"
  FAIL=$((FAIL + 1))
fi


# --- MBC1 bank switching test (Item 1) --------------------------------------
echo ""
echo "8. Compiling mbc1-test.c (ROM bank 2 access via BANK_SWITCH)..."
"$CLANG" --target=sm83-unknown-none -ffreestanding -O1 \
  -c "$SCRIPT_DIR/mbc1-test.c" -o "$TMPDIR/mbc1-test.o"
check "mbc1-test.o has .romx.bank2 section" \
  "'$OBJDUMP' -h '$TMPDIR/mbc1-test.o' 2>&1 | grep -q '\\.romx\\.bank2'"

"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/mbc1-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" \
  -o "$TMPDIR/mbc1-test.elf"
check "mbc1-test.elf built" test -f "$TMPDIR/mbc1-test.elf"

# Confirm the payload symbol resolved into the bank-2 VMA window ($4000-$7FFF).
PAYLOAD_ADDR=$("$BUILD/bin/llvm-nm" "$TMPDIR/mbc1-test.elf" 2>/dev/null \
  | awk '$3 == "bank2_payload" { print $1 }')
if [ -n "$PAYLOAD_ADDR" ] && \
   [ "$((0x$PAYLOAD_ADDR))" -ge $((0x4000)) ] && \
   [ "$((0x$PAYLOAD_ADDR))" -lt $((0x8000)) ]; then
  echo "  PASS: bank2_payload at \$$PAYLOAD_ADDR (VMA in \$4000-\$7FFF bank window)"
  PASS=$((PASS + 1))
else
  echo "  FAIL: bank2_payload at '\$$PAYLOAD_ADDR' is not in bank window"
  FAIL=$((FAIL + 1))
fi

# Build as MBC1 with 4 banks (64 KB) — bank 2's LMA at $8000 must be in the file.
python3 "$MAKEROM" "$TMPDIR/mbc1-test.elf" -o "$TMPDIR/mbc1-test.gb" \
  --mbc1 --rom-banks 4 >/dev/null

# Header checks: $0147 = 0x01 (MBC1), $0148 = 0x01 (4 banks), file is 64 KB.
MBC_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/mbc1-test.gb','rb').read()[0x147])")
ROMSIZE_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/mbc1-test.gb','rb').read()[0x148])")
FILESIZE=$(stat -c%s "$TMPDIR/mbc1-test.gb")
if [ "$MBC_BYTE" = "01" ] && [ "$ROMSIZE_BYTE" = "01" ] && [ "$FILESIZE" -eq 65536 ]; then
  echo "  PASS: MBC1 header (\$0147=\$01, \$0148=\$01) and 64 KB file"
  PASS=$((PASS + 1))
else
  echo "  FAIL: MBC1 header got \$0147=\$$MBC_BYTE \$0148=\$$ROMSIZE_BYTE file=$FILESIZE bytes"
  FAIL=$((FAIL + 1))
fi

python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/mbc1-test.gb" \
  --check C100=DE --check C101=AD --check C102=BE --check C103=EF
HARNESS_RC=$?
if [ $HARNESS_RC -eq 0 ]; then
  echo "  PASS: BANK_SWITCH(2) payload copy produced DEADBEEF at \$C100"
  PASS=$((PASS + 1))
else
  echo "  FAIL: MBC1 bank-2 read did not produce expected payload"
  FAIL=$((FAIL + 1))
fi

# --- CGB header + MMIO test (Item 2) ----------------------------------------
echo ""
echo "9. Compiling cgb-test.c (CGB MMIO + --cgb-only header)..."
"$CLANG" --target=sm83-unknown-none -ffreestanding -O1 \
  -c "$SCRIPT_DIR/cgb-test.c" -o "$TMPDIR/cgb-test.o"
"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/cgb-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" \
  -o "$TMPDIR/cgb-test.elf"
check "cgb-test.elf built" test -f "$TMPDIR/cgb-test.elf"

python3 "$MAKEROM" "$TMPDIR/cgb-test.elf" -o "$TMPDIR/cgb-test.gb" \
  --cgb-only >/dev/null

CGB_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/cgb-test.gb','rb').read()[0x143])")
if [ "$CGB_BYTE" = "c0" ]; then
  echo "  PASS: CGB flag \$0143 = \$c0 (CGB-only)"
  PASS=$((PASS + 1))
else
  echo "  FAIL: CGB flag \$0143 got \$$CGB_BYTE (expected \$c0)"
  FAIL=$((FAIL + 1))
fi

# Also verify default (no flag) still stamps $0143 = 0x80 and --dmg-only → 0x00.
python3 "$MAKEROM" "$TMPDIR/cgb-test.elf" -o "$TMPDIR/cgb-test-default.gb" >/dev/null
python3 "$MAKEROM" "$TMPDIR/cgb-test.elf" -o "$TMPDIR/cgb-test-dmg.gb" --dmg-only >/dev/null
DEFAULT_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/cgb-test-default.gb','rb').read()[0x143])")
DMG_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/cgb-test-dmg.gb','rb').read()[0x143])")
if [ "$DEFAULT_BYTE" = "80" ] && [ "$DMG_BYTE" = "00" ]; then
  echo "  PASS: CGB flag defaults (\$80) and --dmg-only (\$00)"
  PASS=$((PASS + 1))
else
  echo "  FAIL: CGB flag default=\$$DEFAULT_BYTE --dmg-only=\$$DMG_BYTE"
  FAIL=$((FAIL + 1))
fi

python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/cgb-test.gb" --check C100=A5
HARNESS_RC=$?
if [ $HARNESS_RC -eq 0 ]; then
  echo "  PASS: CGB MMIO writes did not crash and main ran to completion"
  PASS=$((PASS + 1))
else
  echo "  FAIL: CGB MMIO test did not halt with sentinel at \$C100"
  FAIL=$((FAIL + 1))
fi

# --- MBC5 bank switching test (Round 6 item 3) ------------------------------
echo ""
echo "10. Compiling mbc5-test.c (ROM bank 3 via MBC5, dual-register crt0)..."
"$CLANG" --target=sm83-unknown-none -ffreestanding -O1 \
  -c "$SCRIPT_DIR/mbc5-test.c" -o "$TMPDIR/mbc5-test.o"
check "mbc5-test.o has .romx.bank3 section" \
  "'$OBJDUMP' -h '$TMPDIR/mbc5-test.o' 2>&1 | grep -q '\\.romx\\.bank3'"

# Link against the MBC5 crt0 variant (writes both $2000 and $3000 at startup).
"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/mbc5-test.o" "$CRT0_MBC5" "$RUNTIME" "$RUNTIME_ASM" \
  -o "$TMPDIR/mbc5-test.elf"
check "mbc5-test.elf built (against sm83-crt0-mbc5.o)" test -f "$TMPDIR/mbc5-test.elf"

# 4 banks (64 KB) is enough for bank 3's LMA at $C000.
python3 "$MAKEROM" "$TMPDIR/mbc5-test.elf" -o "$TMPDIR/mbc5-test.gb" \
  --mbc5 --rom-banks 4 >/dev/null

MBC_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/mbc5-test.gb','rb').read()[0x147])")
ROMSIZE_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/mbc5-test.gb','rb').read()[0x148])")
FILESIZE=$(stat -c%s "$TMPDIR/mbc5-test.gb")
if [ "$MBC_BYTE" = "1b" ] && [ "$ROMSIZE_BYTE" = "01" ] && [ "$FILESIZE" -eq 65536 ]; then
  echo "  PASS: MBC5 header (\$0147=\$1b, \$0148=\$01) and 64 KB file"
  PASS=$((PASS + 1))
else
  echo "  FAIL: MBC5 header got \$0147=\$$MBC_BYTE \$0148=\$$ROMSIZE_BYTE file=$FILESIZE bytes"
  FAIL=$((FAIL + 1))
fi

python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/mbc5-test.gb" \
  --check C100=C0 --check C101=FF --check C102=EE --check C103=42
HARNESS_RC=$?
if [ $HARNESS_RC -eq 0 ]; then
  echo "  PASS: MBC5 BANK_SWITCH(3) payload copy produced C0FFEE42 at \$C100"
  PASS=$((PASS + 1))
else
  echo "  FAIL: MBC5 bank-3 read did not produce expected payload"
  FAIL=$((FAIL + 1))
fi

# --- MBC3 bank switching test (Round 6 item 2) ------------------------------
echo ""
echo "11. Compiling mbc3-test.c (ROM bank 5 access via BANK_SWITCH, --mbc3)..."
"$CLANG" --target=sm83-unknown-none -ffreestanding -O1 \
  -c "$SCRIPT_DIR/mbc3-test.c" -o "$TMPDIR/mbc3-test.o"
check "mbc3-test.o has .romx.bank5 section" \
  "'$OBJDUMP' -h '$TMPDIR/mbc3-test.o' 2>&1 | grep -q '\\.romx\\.bank5'"

"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/mbc3-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" \
  -o "$TMPDIR/mbc3-test.elf"
check "mbc3-test.elf built" test -f "$TMPDIR/mbc3-test.elf"

# 8 banks (128 KB) so bank 5 (LMA $14000) is in-file.
python3 "$MAKEROM" "$TMPDIR/mbc3-test.elf" -o "$TMPDIR/mbc3-test.gb" \
  --mbc3 --rom-banks 8 >/dev/null

MBC_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/mbc3-test.gb','rb').read()[0x147])")
ROMSIZE_BYTE=$(python3 -c "print('%02x' % open('$TMPDIR/mbc3-test.gb','rb').read()[0x148])")
FILESIZE=$(stat -c%s "$TMPDIR/mbc3-test.gb")
if [ "$MBC_BYTE" = "13" ] && [ "$ROMSIZE_BYTE" = "02" ] && [ "$FILESIZE" -eq 131072 ]; then
  echo "  PASS: MBC3 header (\$0147=\$13, \$0148=\$02) and 128 KB file"
  PASS=$((PASS + 1))
else
  echo "  FAIL: MBC3 header got \$0147=\$$MBC_BYTE \$0148=\$$ROMSIZE_BYTE file=$FILESIZE bytes"
  FAIL=$((FAIL + 1))
fi

python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/mbc3-test.gb" \
  --check C100=DE --check C101=AD --check C102=BE --check C103=EF
HARNESS_RC=$?
if [ $HARNESS_RC -eq 0 ]; then
  echo "  PASS: BANK_SWITCH(5) payload copy produced DEADBEEF at \$C100"
  PASS=$((PASS + 1))
else
  echo "  FAIL: MBC3 bank-5 read did not produce expected payload"
  FAIL=$((FAIL + 1))
fi

# --- Far-call test (Round 7 item 1) ----------------------------------------
echo ""
echo "12. Compiling farcall-test.c (inline far calls to bank 2 + bank 30)..."
"$CLANG" --target=sm83-unknown-none -ffreestanding -O1 \
  -c "$SCRIPT_DIR/farcall-test.c" -o "$TMPDIR/farcall-test.o"
check "farcall-test.o has .romx.bank2 section" \
  "'$OBJDUMP' -h '$TMPDIR/farcall-test.o' 2>&1 | grep -q '\\.romx\\.bank2'"
check "farcall-test.o has .romx.bank30 section" \
  "'$OBJDUMP' -h '$TMPDIR/farcall-test.o' 2>&1 | grep -q '\\.romx\\.bank30'"

"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/farcall-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" \
  -o "$TMPDIR/farcall-test.elf"
check "farcall-test.elf built" test -f "$TMPDIR/farcall-test.elf"

# MBC3 with 32 banks (512 KB) so bank 30's LMA at $78000 is in-file.
python3 "$MAKEROM" "$TMPDIR/farcall-test.elf" -o "$TMPDIR/farcall-test.gb" \
  --mbc3 --rom-banks 32 >/dev/null

# Confirm bank-30 callee lives in the bank window ($4000-$7FFF VMA).
CALLEE30_ADDR=$("$BUILD/bin/llvm-nm" "$TMPDIR/farcall-test.elf" 2>/dev/null \
  | awk '$3 == "add_hundred" { print $1 }')
if [ -n "$CALLEE30_ADDR" ] && \
   [ "$((0x$CALLEE30_ADDR))" -ge $((0x4000)) ] && \
   [ "$((0x$CALLEE30_ADDR))" -lt $((0x8000)) ]; then
  echo "  PASS: add_hundred at \$$CALLEE30_ADDR (bank window VMA)"
  PASS=$((PASS + 1))
else
  echo "  FAIL: add_hundred at '\$$CALLEE30_ADDR' is outside bank window"
  FAIL=$((FAIL + 1))
fi

python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/farcall-test.gb" \
  --check C100=37 --check C101=69
HARNESS_RC=$?
if [ $HARNESS_RC -eq 0 ]; then
  echo "  PASS: Far calls to banks 2 and 30 returned correct results (\$37 + \$69)"
  PASS=$((PASS + 1))
else
  echo "  FAIL: Far-call results at \$C100 did not match expected"
  FAIL=$((FAIL + 1))
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
