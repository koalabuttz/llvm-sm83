; RUN: llc -march=sm83 < %s | FileCheck %s

; Test i8 load and store through global address.

@g = global i8 0

define i8 @load_global() {
; CHECK-LABEL: load_global:
; CHECK:       ld hl, g
; CHECK-NEXT:  ld a, [hl]
; CHECK-NEXT:  ret
  %v = load i8, ptr @g
  ret i8 %v
}

define void @store_global(i8 %v) {
; CHECK-LABEL: store_global:
; CHECK:       ld hl, g
; CHECK-NEXT:  ld [hl], a
; CHECK-NEXT:  ret
  store i8 %v, ptr @g
  ret void
}

; Test i16 load and store through global address.

@g16 = global i16 0

define i16 @load_global_i16() {
; CHECK-LABEL: load_global_i16:
; CHECK:       ld hl,
; CHECK:       ret
  %v = load i16, ptr @g16
  ret i16 %v
}

define void @store_global_i16(i16 %v) {
; CHECK-LABEL: store_global_i16:
; CHECK:       ld hl,
; CHECK:       ret
  store i16 %v, ptr @g16
  ret void
}
