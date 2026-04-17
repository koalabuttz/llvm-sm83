// RUN: %clang_cc1 -triple sm83-unknown-none -ffreestanding \
// RUN:   -internal-isystem %S/../../../lib/Headers \
// RUN:   -internal-isystem %S/../../../lib/Headers/sm83 \
// RUN:   -emit-llvm -o - %s | FileCheck %s
//
// Exercise the freestanding triad (stdint.h, stddef.h, stdbool.h) on
// SM83. Mainline Clang ships these in clang/lib/Headers/; the SM83
// toolchain (clang/lib/Driver/ToolChains/SM83.cpp) prepends
// <resource-dir>/include/sm83 ahead of them, but since no SM83-specific
// override exists, the mainline freestanding versions resolve.
//
// This test is the Round-9 Item-2 anchor: if a future patch adds an
// sm83/stdint.h shim that gets the type widths wrong, this test breaks
// first.

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- stdint.h width sanity -------------------------------------------------
// SM83 is 8-bit with 16-bit pointers; clang's target info should yield:
//   int8_t  = signed char        (1 byte)
//   int16_t = short              (2 bytes)
//   int32_t = int                (on SM83 int is 16-bit, so this should be long)
//   intptr_t = 16-bit            (matches pointer width)

_Static_assert(sizeof(int8_t)   == 1, "int8_t must be 1 byte");
_Static_assert(sizeof(uint8_t)  == 1, "uint8_t must be 1 byte");
_Static_assert(sizeof(int16_t)  == 2, "int16_t must be 2 bytes");
_Static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
_Static_assert(sizeof(int32_t)  == 4, "int32_t must be 4 bytes");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
_Static_assert(sizeof(int64_t)  == 8, "int64_t must be 8 bytes");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");

_Static_assert(sizeof(intptr_t)  == sizeof(void *), "intptr_t must match ptr");
_Static_assert(sizeof(uintptr_t) == sizeof(void *), "uintptr_t must match ptr");
_Static_assert(sizeof(void *) == 2, "SM83 pointers are 16-bit");

// Spot-check the "fast" / "least" variants exist and are non-pathological.
_Static_assert(sizeof(int_fast8_t)   >= 1, "");
_Static_assert(sizeof(int_fast16_t)  >= 2, "");
_Static_assert(sizeof(int_least8_t)  == 1, "");
_Static_assert(sizeof(int_least16_t) == 2, "");

// INT*_MAX sanity.
_Static_assert(INT8_MAX  == 127,           "");
_Static_assert(UINT8_MAX == 255,           "");
_Static_assert(INT16_MAX == 32767,         "");
_Static_assert(UINT16_MAX == 65535u,       "");

// --- stddef.h basics --------------------------------------------------------

_Static_assert(sizeof(size_t)    == 2, "size_t is 16-bit on SM83");
_Static_assert(sizeof(ptrdiff_t) == 2, "ptrdiff_t is 16-bit on SM83");
_Static_assert((void *)0 == NULL,       "NULL is 0");

// offsetof() must compile.
struct S { char a; int b; };
_Static_assert(offsetof(struct S, b) >= 1, "offsetof works");

// --- stdbool.h -------------------------------------------------------------

_Static_assert(sizeof(bool) == 1,  "bool is 1 byte on SM83");
_Static_assert(true  == 1,         "true  == 1");
_Static_assert(false == 0,         "false == 0");

// --- Codegen smoke: ensure the types lower to expected LLVM IR widths -----
// These emit globals whose types are locked down by FileCheck below.

uint8_t  g_u8   = 0x42;
uint16_t g_u16  = 0xBEEF;
uint32_t g_u32  = 0xDEADBEEFu;
size_t   g_size = sizeof(g_u32);

// CHECK: @g_u8   = {{(dso_local )?}}global i8 66
// CHECK: @g_u16  = {{(dso_local )?}}global i16 -16657
// CHECK: @g_u32  = {{(dso_local )?}}global i32 -559038737
// CHECK: @g_size = {{(dso_local )?}}global i16 4

int main(void) { return 0; }
