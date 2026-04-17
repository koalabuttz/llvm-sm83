# SM83 end-to-end test suite

Two backends verify each compiled ROM:

- **`sm83sim.py`** — our in-repo Python simulator. Fast, always available.
  Default backend.
- **mGBA 0.10.2+** via `libmgba` — a cycle-accurate real emulator. Optional;
  enables `--backend=mgba` and `--backend=both` modes.

## Running

```bash
/data/test-sm83.sh                    # lit + sim-backed e2e (default)
/data/test-sm83.sh --backend=mgba     # lit + mGBA-backed e2e
/data/test-sm83.sh --backend=both     # lit + every ROM run under both
```

Or invoke the e2e scripts directly:

```bash
bash tests/e2e/run-c-e2e.sh --backend=both
bash tests/e2e/run-e2e.sh   --backend=mgba
```

## Enabling the mGBA backend

The mGBA backend requires `libmgba-dev` installed at build time so
`build-llvm-sm83.sh` can compile the `mgba-harness` binary against it.

Ubuntu / Debian:
```bash
sudo apt install libmgba-dev mgba-sdl    # libmgba-dev is the critical one
/data/build-llvm-sm83.sh                 # rebuilds; produces bin/mgba-harness
```

If `libmgba-dev` is missing, `build-llvm-sm83.sh` prints
`Skipping mgba-harness (install libmgba-dev to enable --backend=mgba)`
and the sim backend keeps working.

Verify:
```bash
ls /data/llvm-sm83/build-sm83-$(uname -m)/bin/mgba-harness
```

## Divergences between backends

Known cases where sim and mGBA disagree are logged in
[`EMULATOR-BUGS.md`](EMULATOR-BUGS.md) with root cause, status, and
repro. That file is the canonical place to look when a test passes under
one backend but fails under the other.

## Limitations of the mGBA harness

- No `--rtc-start` support yet — MBC3 RTC tests auto-skip under mGBA.
  Those tests pin themselves to the sim backend in `run-c-e2e.sh`.
- No `--sram-load` / `--sram-save` — SRAM persistence tests run under
  sim only. The read-back portion (SRAM → WRAM within one run) *does*
  work under mGBA.
- The harness waits for PC to reach the `__sm83_exit` symbol (exported
  by `crt0.s`). Tests that don't return to crt0's halt loop will time
  out; see `EMULATOR-BUGS.md` BUG-1 for the canonical example.
