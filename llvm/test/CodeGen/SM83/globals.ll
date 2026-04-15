; RUN: llc -march=sm83 < %s | FileCheck %s

; Global variable access.

@g8 = global i8 42
@g16 = global i16 1000
@arr = global [4 x i8] [i8 1, i8 2, i8 3, i8 4]

define i8 @load_global_i8() {
; CHECK-LABEL: load_global_i8:
; CHECK: ld hl, g8
; CHECK: ld {{[a-l]}}, [hl]
; CHECK: ret
  %v = load i8, ptr @g8
  ret i8 %v
}

define void @store_global_i8(i8 %v) {
; CHECK-LABEL: store_global_i8:
; CHECK: ld hl, g8
; CHECK: ld [hl], a
; CHECK: ret
  store i8 %v, ptr @g8
  ret void
}

define i16 @load_global_i16() {
; CHECK-LABEL: load_global_i16:
; CHECK: ld hl, g16
; CHECK: ret
  %v = load i16, ptr @g16
  ret i16 %v
}

define i8 @load_array_element(i16 %idx) {
; CHECK-LABEL: load_array_element:
; CHECK: ld hl, arr
; CHECK: ret
  %p = getelementptr [4 x i8], ptr @arr, i16 0, i16 %idx
  %v = load i8, ptr %p
  ret i8 %v
}
