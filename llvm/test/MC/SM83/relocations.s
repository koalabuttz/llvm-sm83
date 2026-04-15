; RUN: llvm-mc -triple sm83 -filetype=obj %s -o %t.o
; RUN: llvm-readelf -r %t.o | FileCheck %s

; Test relocation emission for external symbols.
; SM83 reuses i386 relocation numbers (type 20 = R_386_16).
; llvm-readelf shows them as "Unknown" since SM83 has no registered names.

	.text
	.globl test_relocs
test_relocs:
	call external_func
	jp external_func
	ld hl, external_data
	ld bc, external_data
	ret

; CHECK: Relocation section '.rel.text'
; CHECK-DAG: {{.*}}external_func
; CHECK-DAG: {{.*}}external_data
