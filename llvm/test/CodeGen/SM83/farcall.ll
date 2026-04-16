; RUN: llc -march=sm83 < %s | FileCheck %s
;
; Round 7: inline far calls. When the callee's section attribute places
; it in .romx.bankN, LowerCall emits a BANK_SWITCH(N) store before the
; CALL and a BANK_SWITCH(1) store after the return-value copy. No new
; relocation, no runtime helper — the bank number is a compile-time
; constant parsed from the section name.

declare i8 @near_callee(i8)

;===------------------------------------------------------------------------===;
; Callees placed in switchable banks
;===------------------------------------------------------------------------===;

define i8 @bank2_callee(i8 %x) section ".romx.bank2" {
  %r = add i8 %x, 1
  ret i8 %r
}

define void @bank3_void_callee() section ".romx.bank3" {
  ret void
}

define i8 @bank30_callee(i8 %x) section ".romx.bank30" {
  %r = add i8 %x, 2
  ret i8 %r
}

;===------------------------------------------------------------------------===;
; Near call — unchanged behavior.
;===------------------------------------------------------------------------===;

; CHECK-LABEL: test_near:
; CHECK-NOT: ld {{.*}}, 8192
; CHECK: jp near_callee
define i8 @test_near(i8 %x) {
  %r = call i8 @near_callee(i8 %x)
  ret i8 %r
}

;===------------------------------------------------------------------------===;
; Far call with i8 return — must emit bank-select 2 and bank-restore 1.
;===------------------------------------------------------------------------===;

; CHECK-LABEL: test_far_i8:
; CHECK: ld a, 2
; CHECK: ld [8192], a
; CHECK: call bank2_callee
; CHECK: ld a, 1
; CHECK: ld [8192], a
; CHECK: ret
define i8 @test_far_i8(i8 %x) {
  %r = call i8 @bank2_callee(i8 %x)
  ret i8 %r
}

;===------------------------------------------------------------------------===;
; Far call with void return — same envelope, no return-value routing.
;===------------------------------------------------------------------------===;

; CHECK-LABEL: test_far_void:
; CHECK: ld a, 3
; CHECK: ld [8192], a
; CHECK: call bank3_void_callee
; CHECK: ld a, 1
; CHECK: ld [8192], a
; CHECK: ret
define void @test_far_void() {
  call void @bank3_void_callee()
  ret void
}

;===------------------------------------------------------------------------===;
; High-bank far call — proves bank-number parsing handles > 15 (pre-
; Round 6's linker expansion, the old cap).
;===------------------------------------------------------------------------===;

; CHECK-LABEL: test_far_bank30:
; CHECK: call bank30_callee
; CHECK: ret
define i8 @test_far_bank30(i8 %x) {
  %r = call i8 @bank30_callee(i8 %x)
  ret i8 %r
}

;===------------------------------------------------------------------------===;
; Round 8: MBC5 high-bank far call. Bank numbers < 256 only touch $2000
; (the upper-bit register $3000 is already 0 from reset or from a prior
; bank-≥256 call's epilogue restore). Banks 128..255 (low 8 bits ≥ 128)
; prove the linker-script expansion works; banks ≥ 256 additionally
; touch $3000.
;===------------------------------------------------------------------------===;

define i8 @bank200_callee(i8 %x) section ".romx.bank200" {
  %r = add i8 %x, 3
  ret i8 %r
}

define i8 @bank300_callee(i8 %x) section ".romx.bank300" {
  %r = add i8 %x, 4
  ret i8 %r
}

; Bank 200 (0xC8 low byte, high bit 0) — single $2000 write.
; CHECK-LABEL: test_far_bank200:
; CHECK: ld [8192], a
; CHECK-NOT: ld [12288],
; CHECK: call bank200_callee
; CHECK: ld [8192], a
; CHECK: ret
define i8 @test_far_bank200(i8 %x) {
  %r = call i8 @bank200_callee(i8 %x)
  ret i8 %r
}

; Bank 300 (0x12C low byte 0x2C = 44, high bit 1) — writes to both $2000 and $3000.
; CHECK-LABEL: test_far_bank300:
; CHECK-DAG: ld {{.*}}, 44
; CHECK: ld [8192], a
; CHECK: ld [12288], a
; CHECK: call bank300_callee
; CHECK: ld [8192], a
; CHECK: ld [12288], a
; CHECK: ret
define i8 @test_far_bank300(i8 %x) {
  %r = call i8 @bank300_callee(i8 %x)
  ret i8 %r
}
