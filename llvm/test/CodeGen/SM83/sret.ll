; RUN: llc -march=sm83 < %s | FileCheck %s

; Test sret (struct return via hidden pointer).
; LLVM's frontend transforms large struct returns into sret pointer args.
; The SM83 backend handles this transparently — the sret pointer is passed
; as a normal i16 argument (in BC), and the callee writes through it.

%struct.S = type { i8, i8, i8, i8 }

define void @sret_store_one(ptr sret(%struct.S) %result) {
; CHECK-LABEL: sret_store_one:
; CHECK: ld [bc], a
; CHECK: ret
  store i8 42, ptr %result
  ret void
}

define void @sret_store_offset(ptr sret(%struct.S) %result) {
; CHECK-LABEL: sret_store_offset:
; CHECK: ld [bc], a
; CHECK: ret
  %p = getelementptr i8, ptr %result, i16 3
  store i8 99, ptr %p
  ret void
}

define i8 @caller_uses_sret() {
; CHECK-LABEL: caller_uses_sret:
; CHECK: call sret_store_one
; CHECK: ret
  %tmp = alloca %struct.S
  call void @sret_store_one(ptr sret(%struct.S) %tmp)
  %v = load i8, ptr %tmp
  ret i8 %v
}
