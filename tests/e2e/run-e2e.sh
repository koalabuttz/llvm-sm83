#!/bin/bash
set -euo pipefail

# End-to-end smoke test for the SM83 toolchain.
# Compiles LLVM IR → object → links with lld → converts to .gb ROM.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LLVM_SRC="/data/llvm-sm83"
ARCH="$(uname -m)"
BUILD="$LLVM_SRC/build-sm83-${ARCH}"
TMPDIR="${TMPDIR:-/tmp}/sm83-e2e-$$"
PASS=0
FAIL=0

mkdir -p "$TMPDIR"
trap 'rm -rf "$TMPDIR"' EXIT

LLC="$BUILD/bin/llc"
LLD="$BUILD/bin/ld.lld"
# --no-check-sections: all ROMX banks share VMA $4000-$7FFF (MBC1 overlay
# semantics). LLD's default VMA-overlap check rejects this; silence it.
LLDFLAGS="--no-check-sections"
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

echo "=== SM83 End-to-End Smoke Test ==="
echo ""

# Step 1: Compile smoke.ll to object
echo "1. Compiling smoke.ll..."
"$LLC" -march=sm83 -filetype=obj "$SCRIPT_DIR/smoke.ll" -o "$TMPDIR/smoke.o"
check "smoke.o created" test -f "$TMPDIR/smoke.o"
check "smoke.o has main symbol" "'$OBJDUMP' -t '$TMPDIR/smoke.o' 2>&1 | grep -q main"

# Step 2: Link with lld
echo "2. Linking with ld.lld..."
"$LLD" $LLDFLAGS -T "$LINKER_SCRIPT" "$TMPDIR/smoke.o" "$CRT0" "$RUNTIME" "$RUNTIME_ASM" -o "$TMPDIR/smoke.elf"
check "smoke.elf created" test -f "$TMPDIR/smoke.elf"
check "ELF has _start symbol" "'$OBJDUMP' -t '$TMPDIR/smoke.elf' 2>&1 | grep -q _start"
check "ELF has main symbol" "'$OBJDUMP' -t '$TMPDIR/smoke.elf' 2>&1 | grep -q main"

# Step 3: Convert to Game Boy ROM
echo "3. Converting to .gb ROM..."
python3 "$MAKEROM" "$TMPDIR/smoke.elf" -o "$TMPDIR/smoke.gb"
check "smoke.gb created" test -f "$TMPDIR/smoke.gb"

# Step 4: Validate ROM structure
echo "4. Validating ROM..."
check "ROM size is 32768 bytes" test "$(stat -c%s "$TMPDIR/smoke.gb")" -eq 32768
check "Checksums valid" python3 "$MAKEROM" --check-only "$TMPDIR/smoke.gb"

# Check entry point: 00 C3 50 01 at offset $0100
ENTRY=$(xxd -p -s 0x100 -l 4 "$TMPDIR/smoke.gb")
check "Entry point NOP;JP \$0150" test "$ENTRY" = "00c35001"

# Check Nintendo logo at offset $0104 (first 4 bytes)
LOGO=$(xxd -p -s 0x104 -l 4 "$TMPDIR/smoke.gb")
check "Nintendo logo present" test "$LOGO" = "ceed6666"

# Check _start is at $0150 (code begins there)
START_BYTE=$(xxd -p -s 0x150 -l 1 "$TMPDIR/smoke.gb")
check "_start code at \$0150" test "$START_BYTE" = "f3"  # DI instruction

# Step 5: Behavioral verification via SM83 simulator
echo "5. Running in SM83 simulator..."
if python3 "$SCRIPT_DIR/run-harness.py" "$TMPDIR/smoke.gb" \
  --check 8000=AA --check 8001=55 --check 9800=00 --check FF40=81 2>&1; then
  echo "  PASS: Simulator verification"
  PASS=$((PASS + 1))
else
  echo "  FAIL: Simulator verification"
  FAIL=$((FAIL + 1))
fi

echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
