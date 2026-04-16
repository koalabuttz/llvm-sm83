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
OBJDUMP="$BUILD/bin/llvm-objdump"
LINKER_SCRIPT="$LLVM_SRC/sm83.ld"
CRT0="$BUILD/sm83-crt0.o"
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
"$LLD" -T "$LINKER_SCRIPT" "$TMPDIR/c-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" -o "$TMPDIR/c-test.elf"
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
"$LLD" -T "$LINKER_SCRIPT" "$TMPDIR/hram-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" \
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

"$LLD" -T "$LINKER_SCRIPT" "$TMPDIR/c-vblank-test.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" \
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

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
