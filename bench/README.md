# SM83 codegen benchmark vs GBDK / SDCC

Round 9 item 4 artifact. Measures the code-size gap between our
LLVM-SM83 backend and GBDK 4.5.0 (SDCC 4.5.1 under the hood) on
representative C source. Data feeds the Round 10+ decision on whether
static stacks, additional peephole patterns, or register-allocation
tuning are worth the engineering effort.

## Method

- Benchmarks live in this directory (`state-machine.c`) or are
  re-used from the e2e suite (`c-test.c` → `../tests/e2e/c-test.c`)
  so there's a single source-of-truth for every program.
- Both toolchains build from the same `.c` source files with comparable
  optimisation flags:
  - LLVM-SM83: `clang --target=sm83-unknown-none -ffreestanding -Os -c`
  - GBDK:       `lcc -Wf--max-allocs-per-node50000 -c` (SDCC's maximum
    effort allocation, as recommended by gbdk-2020's README).
- Compile only (`-c`); we compare the compiled `.o` / `.rel` for the user
  code, excluding any crt0/libc/runtime that each toolchain bundles.
  This isolates codegen quality rather than library footprint.
- Sizes come from `llvm-size -A` (`.text`) for ours and the `A _CODE`
  section size in SDCC's relocatable output for GBDK.
- Run with `make`; artifacts land in `bench/build/`.

## Results (2026-04-17, GBDK 4.5.0, SDCC 4.5.1)

| Program | LLVM-SM83 (.text) | GBDK/SDCC (_CODE) | Δ | Winner |
|---|---:|---:|---:|---|
| `c-test.c` (simple stores + calls) | 37 B | 42 B | −5 B (−12%) | **LLVM** |
| `state-machine.c` (128-byte compute) | 165 B | 101 B | +64 B (+63%) | **GBDK** |

## Reading the table

**LLVM wins on call-and-store code.** `c-test.c` is dominated by
`ld a, imm; ld [nn], a` sequences plus a couple of register-pair
function calls. Our peephole passes (INC/DEC folding, redundant-store
elimination, dead flag skip) are competitive here.

**GBDK wins on compute-heavy code.** The 64-byte gap on
`state-machine.c` (a 128-iteration loop with 16-bit arithmetic + XOR
+ rotates) is large. Likely contributors, in decreasing order of
suspicion:

1. **Register allocation on a tight loop.** SDCC is static-stack
   biased and aggressively reuses `A`/`HL`. Ours spills more on
   16-bit ops because SelectionDAG lowers `uint16_t` to paired `i8`s
   early.
2. **16-bit rotate lowering.** `(s << 3) | (s >> 13)` compiles to a
   series of 1-bit shifts and OR pairs on both toolchains, but
   SDCC has dedicated lowering for wide-rotate-by-n patterns.
3. **Loop-counter allocation.** `uint8_t i = 0; i < 128; i++` should
   stay in a register across the loop; if it spills to stack each
   iteration, that's 6 bytes × 2 loops = 12 B of extra code.

A proper root-cause-by-RC (generate side-by-side `.asm` for
`state-machine.c` and diff) would drive the Round 10 peephole list.
Not doing that in this round — the point here is the measurement,
not the fix.

## What this implies for the roadmap

- **Not a blocker for shipping** small programs. `c-test.c`-class code
  (MMIO manipulation, interrupts, simple state transitions) is
  already competitive.
- **Is a blocker for shipping** compute-heavy games. A 40–60% size
  penalty on inner loops translates to hitting ROM ceiling sooner and
  running slower.
- **Top three levers for Round 10+**, in likely leverage order:
  1. Profile-guided peephole additions targeted at 16-bit loops
     (diff the generated asm for `state-machine.c`).
  2. Evaluate static-stacks (from PLAN-SM83 deferred list). SDCC's
     approach suggests this is where a lot of its size win comes
     from on 8-bit.
  3. Tune GR16 allocator preference heuristics for deep-use values
     like loop accumulators.

## Reproducing

```bash
cd /data/llvm-sm83/bench
make clean && make          # builds both, prints table
```

Requires `/tmp/gbdk` populated (download from
<https://github.com/gbdk-2020/gbdk-2020/releases/latest>; use the
`gbdk-linux-arm64` asset on this VM). Override with `GBDK=/path`.
