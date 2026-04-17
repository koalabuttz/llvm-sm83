; smoke.ll — Minimal Game Boy program for end-to-end toolchain validation.
;
; Writes a checkerboard tile pattern to VRAM, enables the LCD, and halts.
; When booted in an emulator, a single 8x8 tile should be visible at the
; top-left corner of the screen.
;
; Build:
;   llc -march=sm83 -filetype=obj smoke.ll -o smoke.o
;   ld.lld -T ../../sm83.ld smoke.o $BUILD/sm83-crt0.o $BUILD/sm83-runtime.o -o smoke.elf
;   python3 ../../make-gb-rom.py smoke.elf -o smoke.gb

; --- Hardware register addresses ---
; $FF40 = LCDC (LCD control)
; $FF44 = LY   (current scanline, read-only)
; $8000 = start of VRAM tile data

define void @main() {
entry:
  ; Wait for VBlank: poll LY ($FF44) until it reaches 144.
  br label %wait_vblank

wait_vblank:
  %ly_ptr = inttoptr i16 65348 to ptr  ; 0xFF44
  %ly = load volatile i8, ptr %ly_ptr
  %done = icmp eq i8 %ly, -112          ; 144 as signed i8
  br i1 %done, label %lcd_off, label %wait_vblank

lcd_off:
  ; Turn off LCD so we can safely write to VRAM.
  %lcdc_ptr = inttoptr i16 65344 to ptr ; 0xFF40
  store volatile i8 0, ptr %lcdc_ptr

  ; Write a checkerboard tile (16 bytes) to VRAM tile 0 at $8000.
  ; Each row is 2 bytes: low-plane, high-plane.
  ; Pattern: alternating $AA/$55 gives a checkerboard.
  %vram = inttoptr i16 -32768 to ptr    ; 0x8000
  store volatile i8 -86, ptr %vram       ; row 0 lo: $AA
  %v1 = getelementptr i8, ptr %vram, i16 1
  store volatile i8 85, ptr %v1          ; row 0 hi: $55
  %v2 = getelementptr i8, ptr %vram, i16 2
  store volatile i8 85, ptr %v2          ; row 1 lo: $55
  %v3 = getelementptr i8, ptr %vram, i16 3
  store volatile i8 -86, ptr %v3         ; row 1 hi: $AA
  %v4 = getelementptr i8, ptr %vram, i16 4
  store volatile i8 -86, ptr %v4         ; row 2 lo: $AA
  %v5 = getelementptr i8, ptr %vram, i16 5
  store volatile i8 85, ptr %v5          ; row 2 hi: $55
  %v6 = getelementptr i8, ptr %vram, i16 6
  store volatile i8 85, ptr %v6          ; row 3 lo: $55
  %v7 = getelementptr i8, ptr %vram, i16 7
  store volatile i8 -86, ptr %v7         ; row 3 hi: $AA
  ; (remaining 4 rows left as 0 — blank bottom half)

  ; Write tile index 0 to the tile map at $9800 (top-left of BG).
  %tilemap = inttoptr i16 -26624 to ptr ; 0x9800
  store volatile i8 0, ptr %tilemap

  ; Turn LCD back on: bit 7 (enable) + bit 0 (BG display).
  store volatile i8 -127, ptr %lcdc_ptr  ; $81

  ; Return; crt0's __sm83_exit halt loop terminates the program.
  ; (Do NOT use a `br label %self` spin here — see tests/e2e/EMULATOR-BUGS.md
  ; BUG-1/BUG-2: sm83sim treats JP-to-self as halt, but mGBA correctly
  ; spins forever.)
  ret void
}
