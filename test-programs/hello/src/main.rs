//! Trivial no_std smoke-test for the SM83 (Game Boy) backend.
//!
//! Writes 0x42 to VRAM address $8000 as a visible side-effect, then loops.
//! Build with:
//!   rustc --target sm83-unknown-none.json -C panic=abort \
//!         --edition 2021 -o hello.o src/main.rs
//!   (then link with a GB linker script to produce a .gb ROM)

#![no_std]
#![no_main]
#![allow(unused)]

#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn main() -> ! {
    unsafe {
        // Write a sentinel byte to VRAM as a smoke test.
        let vram = 0x8000 as *mut u8;
        vram.write_volatile(0x42);
    }
    loop {}
}
