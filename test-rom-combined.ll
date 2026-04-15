; test-rom-combined.ll — Self-contained test ROM (no linker needed).
;
; All functions in one module. Compiled to a single ELF object,
; then converted to .gb ROM via make-gb-rom.py (which reads ELF segments).
;
; Tests: i8 add/sub/mul, compare+branch, load/store, function call,
;        AND/OR/XOR, ROM data load.
;
; Expected WRAM results:
;   r0=08 r1=07 r2=2A r3=01 r4=00 r5=AB r6=63 r7=DE r8=AD r9=00
;   r10=FF r11=55 r12=42

target datalayout = "e-p:16:8-i8:8-i16:8-n8-S8"
target triple = "sm83-unknown-none"

; ---------- Result slots (WRAM) ----------
@r0  = global i8 0, align 1
@r1  = global i8 0, align 1
@r2  = global i8 0, align 1
@r3  = global i8 0, align 1
@r4  = global i8 0, align 1
@r5  = global i8 0, align 1
@r6  = global i8 0, align 1
@r7  = global i8 0, align 1
@r8  = global i8 0, align 1
@r9  = global i8 0, align 1
@r10 = global i8 0, align 1
@r11 = global i8 0, align 1
@r12 = global i8 0, align 1

; ---------- Test inputs (WRAM, prevents const folding) ----------
@v3   = global i8 3, align 1
@v5a  = global i8 5, align 1
@v5b  = global i8 5, align 1
@v6   = global i8 6, align 1
@v7   = global i8 7, align 1
@v10  = global i8 10, align 1
@vab  = global i8 171, align 1
@vf0  = global i8 240, align 1
@v0f  = global i8 15, align 1
@vff  = global i8 255, align 1
@vaa  = global i8 170, align 1

; ---------- ROM constants ----------
@rom0 = constant i8 222, align 1
@rom1 = constant i8 173, align 1

; ---------- 8-bit multiply (shift-and-add) ----------
define i8 @__mulqi3(i8 %a, i8 %b) noinline {
entry:
  br label %loop
loop:
  %res  = phi i8 [0, %entry], [%nres, %end]
  %mc   = phi i8 [%a, %entry], [%nmc, %end]
  %mp   = phi i8 [%b, %entry], [%nmp, %end]
  %cnt  = phi i8 [8, %entry], [%ncnt, %end]
  %bit  = and i8 %mp, 1
  %hb   = icmp ne i8 %bit, 0
  br i1 %hb, label %add, label %skip
add:
  %added = add i8 %res, %mc
  br label %end
skip:
  br label %end
end:
  %nres = phi i8 [%added, %add], [%res, %skip]
  %nmc  = shl i8 %mc, 1
  %nmp  = lshr i8 %mp, 1
  %ncnt = sub i8 %cnt, 1
  %done = icmp eq i8 %ncnt, 0
  br i1 %done, label %exit, label %loop
exit:
  ret i8 %nres
}

; ---------- Callee for function call test ----------
define i8 @callee() noinline {
  ret i8 99
}

; ---------- Main test function ----------
define void @main() {
entry:
  %a3  = load volatile i8, ptr @v3
  %a5a = load volatile i8, ptr @v5a
  %a5b = load volatile i8, ptr @v5b
  %a6  = load volatile i8, ptr @v6
  %a7  = load volatile i8, ptr @v7
  %a10 = load volatile i8, ptr @v10

  ; Test 0: add 3+5=8
  %add = add i8 %a3, %a5a
  store i8 %add, ptr @r0

  ; Test 1: sub 10-3=7
  %sub = sub i8 %a10, %a3
  store i8 %sub, ptr @r1

  ; Test 2: mul 6*7=42
  %mul = mul i8 %a6, %a7
  store i8 %mul, ptr @r2

  ; Test 3: cmp eq (5==5 → 1)
  %eq = icmp eq i8 %a5a, %a5b
  br i1 %eq, label %eq_t, label %eq_f
eq_t:
  store i8 1, ptr @r3
  br label %t4
eq_f:
  store i8 0, ptr @r3
  br label %t4

t4:
  ; Test 4: cmp ne (3==5 → 0)
  %ne = icmp eq i8 %a3, %a5a
  br i1 %ne, label %ne_t, label %ne_f
ne_t:
  store i8 1, ptr @r4
  br label %t5
ne_f:
  store i8 0, ptr @r4
  br label %t5

t5:
  ; Test 5: store/load round-trip 0xAB
  %ab = load volatile i8, ptr @vab
  store volatile i8 %ab, ptr @r5
  %ld5 = load volatile i8, ptr @r5
  store i8 %ld5, ptr @r5

  ; Test 6: function call → 99
  %c = call i8 @callee()
  store i8 %c, ptr @r6

  ; Test 7-8: ROM load
  %d0 = load i8, ptr @rom0
  store i8 %d0, ptr @r7
  %d1 = load i8, ptr @rom1
  store i8 %d1, ptr @r8

  ; Test 9: AND 0xF0 & 0x0F = 0
  %af0 = load volatile i8, ptr @vf0
  %a0f = load volatile i8, ptr @v0f
  %av = and i8 %af0, %a0f
  store i8 %av, ptr @r9

  ; Test 10: OR 0xF0 | 0x0F = 0xFF
  %bf0 = load volatile i8, ptr @vf0
  %b0f = load volatile i8, ptr @v0f
  %ov = or i8 %bf0, %b0f
  store i8 %ov, ptr @r10

  ; Test 11: XOR 0xFF ^ 0xAA = 0x55
  %cff = load volatile i8, ptr @vff
  %caa = load volatile i8, ptr @vaa
  %xv = xor i8 %cff, %caa
  store i8 %xv, ptr @r11

  ; Test 12: sentinel 0x42
  store i8 66, ptr @r12

  ret void
}

; ---------- Entry point ----------
define void @_start() naked noreturn nounwind {
entry:
  call void @main()
  br label %halt
halt:
  br label %halt
}
