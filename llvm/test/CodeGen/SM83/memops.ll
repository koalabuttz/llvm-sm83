; RUN: llc -march=sm83 < %s | FileCheck %s

; Memory operations — memcpy/memset lowering.

declare void @llvm.memcpy.p0.p0.i16(ptr, ptr, i16, i1)
declare void @llvm.memset.p0.i16(ptr, i8, i16, i1)

define void @copy_bytes(ptr %dst, ptr %src) {
; CHECK-LABEL: copy_bytes:
; CHECK: ret
  call void @llvm.memcpy.p0.p0.i16(ptr %dst, ptr %src, i16 4, i1 false)
  ret void
}

define void @zero_fill(ptr %dst) {
; CHECK-LABEL: zero_fill:
; CHECK: ret
  call void @llvm.memset.p0.i16(ptr %dst, i8 0, i16 8, i1 false)
  ret void
}
