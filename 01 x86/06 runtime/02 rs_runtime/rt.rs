// rooster-os/arch/x86/runtime/rs_runtime/rt.rs
#![no_std]
#![no_main]
#![feature(alloc_error_handler)]
#![feature(core_intrinsics)]
#![feature(lang_items)]
#![feature(ptr_internals)]

//! Rust “runtime” for RoosterOS on x86_64.
//!   • Validates Multiboot handoff
//!   • Copies .data from flash to RAM
//!   • Zeroes .bss
//!   • Initializes a bump‐allocator as GlobalAlloc
//!   • Provides panic and alloc‐error handlers
//!   • Transfers control to `kernel_main(magic, mbi) -> !`

use core::panic::PanicInfo;
use core::alloc::{GlobalAlloc, Layout};
use core::intrinsics::copy_nonoverlapping;
use core::ptr::write_bytes;

// Multiboot magic constant
const MULTIBOOT_MAGIC: u32 = 0x2BADB002;

// Symbols provided by the linker script
extern "C" {
    // Kernel entry point (written in Rust or C)
    fn kernel_main(magic: u32, mbi_addr: usize) -> !;

    // Data segment: load‐address, start, end
    static __data_load: u8;
    static mut __data_start: u8;
    static mut __data_end:   u8;

    // BSS segment: start, end
    static mut __bss_start: u8;
    static mut __bss_end:   u8;

    // Heap region: start, end
    static mut __heap_start: u8;
    static mut __heap_end:   u8;
}

/// A simple bump‐pointer allocator
struct BumpAllocator {
    next: usize,
    end:  usize,
}

impl BumpAllocator {
    const fn new() -> Self {
        BumpAllocator { next: 0, end: 0 }
    }

    unsafe fn init(&mut self, heap_start: usize, heap_end: usize) {
        self.next = heap_start;
        self.end  = heap_end;
    }

    #[inline]
    fn align_up(addr: usize, align: usize) -> usize {
        (addr + align - 1) & !(align - 1)
    }
}

unsafe impl GlobalAlloc for BumpAllocator {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let mut ptr = Self::align_up(self.next, layout.align());
        let new_next = ptr.checked_add(layout.size()).unwrap_or(self.end);
        if new_next > self.end {
            return core::ptr::null_mut();
        }
        // update pointer
        let alloc_ptr = ptr as *mut u8;
        // SAFETY: &mut through raw pointer
        let me = &mut *(self as *const _ as *mut BumpAllocator);
        me.next = new_next;
        alloc_ptr
    }

    unsafe fn dealloc(&self, _: *mut u8, _: Layout) {
        // no-op for bump allocator
    }
}

#[global_allocator]
static ALLOCATOR: BumpAllocator = BumpAllocator::new();

/// Entry point called from assembly stub (`entry.S`):
///   EDI = multiboot_magic, ESI = mbi_ptr
#[no_mangle]
pub extern "C" fn rust_start(magic: u32, mbi_addr: usize) -> ! {
    // 1) Validate Multiboot signature
    if magic != MULTIBOOT_MAGIC {
        panic!("Bad multiboot magic: {:#x}", magic);
    }

    unsafe {
        // 2) Copy .data from flash (load‐address) to RAM
        let data_size = (&__data_end as *const _ as usize)
                      - (&__data_start as *const _ as usize);
        copy_nonoverlapping(
            &__data_load as *const u8,
            &mut __data_start as *mut u8,
            data_size,
        );

        // 3) Zero .bss
        let bss_size = (&__bss_end as *const _ as usize)
                     - (&__bss_start as *const _ as usize);
        write_bytes(&mut __bss_start as *mut u8, 0, bss_size);

        // 4) Initialize heap allocator
        ALLOCATOR.init(
            &__heap_start as *const _ as usize,
            &__heap_end   as *const _ as usize,
        );
    }

    // 5) Call the kernel’s main function (does not return)
    unsafe { kernel_main(magic, mbi_addr) }
}

/// Called on allocation failure (out of memory)
#[alloc_error_handler]
fn alloc_error(layout: Layout) -> ! {
    panic!("allocation error: {:?}", layout);
}

/// Panic handler prints info (if you’ve hooked a console) then halts
#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    // You can integrate with your VGA/serial console here, e.g.:
    // console::write_fmt(format_args!("PANIC: {}\n", info)).ok();

    // Fallback: just spin with HLT
    loop {
        unsafe { core::arch::asm!("hlt"); }
    }
}

// Minimal lang-items to satisfy `no_std` linking
#[lang = "eh_personality"] extern fn eh_personality() {}
#[lang = "oom"] fn oom(_: Layout) -> ! { loop { unsafe { core::arch::asm!("hlt"); } } }
