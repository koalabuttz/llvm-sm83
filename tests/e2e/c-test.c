/*
 * c-test.c — First C program compiled for SM83 (Game Boy).
 *
 * Tests: function calls, arithmetic, volatile stores to WRAM.
 * Avoids arrays/pointers to work within known-good codegen patterns.
 *
 * Build:
 *   clang --target=sm83-unknown-none -ffreestanding -O1 -c c-test.c -o c-test.o
 *   ld.lld -T ../../sm83.ld c-test.o $BUILD/sm83-crt0.o $BUILD/sm83-runtime.o \
 *          -o c-test.elf
 *   python3 ../../make-gb-rom.py c-test.elf -o c-test.gb
 *
 * Verify:
 *   python3 run-harness.py c-test.gb --check C100=1E --check C101=DE --check C102=0A
 *
 * Expected: WRAM[$C100] = 30 (0x1E) = add_three(10, 10, 10)
 *           WRAM[$C101] = 0xDE (marker byte)
 *           WRAM[$C102] = 10 (0x0A) = identity(10)
 */

#define RESULT    (*(volatile unsigned char *)0xC100)
#define MARKER    (*(volatile unsigned char *)0xC101)
#define IDENTITY  (*(volatile unsigned char *)0xC102)

unsigned char add_two(unsigned char a, unsigned char b) {
    return a + b;
}

unsigned char add_three(unsigned char a, unsigned char b, unsigned char c) {
    unsigned char ab = add_two(a, b);
    return add_two(ab, c);
}

unsigned char identity(unsigned char x) {
    return x;
}

void main(void) {
    unsigned char r = add_three(10, 10, 10);
    RESULT = r;
    MARKER = 0xDE;
    IDENTITY = identity(10);
    while (1) {}
}
