; RUN: llc -march=sm83 -O0 < %s | FileCheck %s

; SM83 is single-threaded, so all atomic operations lower to plain loads/stores.

define i8 @atomic_load_i8(ptr %p) {
; CHECK-LABEL: atomic_load_i8:
; CHECK:       ld a, [hl]
; CHECK-NEXT:  ret
  %val = load atomic i8, ptr %p seq_cst, align 1
  ret i8 %val
}

define i16 @atomic_load_i16(ptr %p) {
; CHECK-LABEL: atomic_load_i16:
; CHECK:       ld a, [hl+]
  %val = load atomic i16, ptr %p seq_cst, align 2
  ret i16 %val
}

define void @atomic_store_i8(ptr %p, i8 %val) {
; CHECK-LABEL: atomic_store_i8:
; CHECK:       ld [hl], a
; CHECK-NEXT:  ret
  store atomic i8 %val, ptr %p seq_cst, align 1
  ret void
}
