; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 addition.

define i8 @add_i8(i8 %a, i8 %b) {
; CHECK-LABEL: add_i8:
; CHECK:       add a, c
  %result = add i8 %a, %b
  ret i8 %result
}

; Test i16 addition (uses ADD HL, rr).

define i16 @add_i16(i16 %a, i16 %b) {
; CHECK-LABEL: add_i16:
; CHECK:       add hl, de
  %result = add i16 %a, %b
  ret i16 %result
}

; Test i8 add-by-1 uses INC (not ADD through accumulator).

define i8 @inc_i8(i8 %a) {
; CHECK-LABEL: inc_i8:
; CHECK:       inc a
  %result = add i8 %a, 1
  ret i8 %result
}

; Test i8 add-by-negative-1 uses DEC.

define i8 @dec_i8(i8 %a) {
; CHECK-LABEL: dec_i8:
; CHECK:       dec a
  %result = add i8 %a, -1
  ret i8 %result
}

; Test i8 add by immediate (uses ADD A, imm8).

define i8 @add_i8_imm(i8 %a) {
; CHECK-LABEL: add_i8_imm:
; CHECK:       add a, 42
  %result = add i8 %a, 42
  ret i8 %result
}

; Test i8 sub by constant — LLVM canonicalizes sub C to add -C,
; so this also uses ADD A, imm8.

define i8 @sub_i8_imm(i8 %a) {
; CHECK-LABEL: sub_i8_imm:
; CHECK:       add a, -10
  %result = sub i8 %a, 10
  ret i8 %result
}
