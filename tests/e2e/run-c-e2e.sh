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
echo "=== Results: $PASS passed, $FAIL failed ==="
[ "$FAIL" -eq 0 ]
