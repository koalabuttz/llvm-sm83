/*===---- string.h — SM83 minimal libc string routines --------------------===
 *
 * Declarations for memcpy/memset/memmove provided by sm83-runtime.s.
 * The SM83 calling convention passes arg0 in BC and remaining 16-bit args
 * on the stack; the runtime is hand-tuned for that ABI. Do not call these
 * functions from crt0 before BSS init — they use no static state, but also
 * assume SP points into WRAM.
 *
 *===----------------------------------------------------------------------===
 */
#ifndef __SM83_STRING_H
#define __SM83_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *__dst, const void *__src, size_t __n);
void *memmove(void *__dst, const void *__src, size_t __n);
void *memset(void *__dst, int __val, size_t __n);

#ifdef __cplusplus
}
#endif

#endif /* __SM83_STRING_H */
