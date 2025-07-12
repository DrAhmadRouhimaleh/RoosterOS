// arch/x86/runtime/go_startup/entry.go
// +build baremetal

// Bare-metal Go runtime startup for RoosterOS on x86.
//   • Clears BSS
//   • Initializes Go heap allocator
//   • Sets up scheduler & GC
//   • Calls C kernel_main(uint32, uintptr)
//   • Halts on return

package main

/*
#include <stdint.h>
// Provided by your C kernel (exported via cgo)
extern void kernel_main(uint32_t magic, uintptr_t mbi);
*/
import "C"
import (
    "unsafe"
    "runtime"
)

// Symbols defined in your linker script (e.g. go.linker.ld)
var (
    bssStart  uintptr // start of .bss (zero-init data)
    bssEnd    uintptr // end of .bss
    heapStart uintptr // start of heap region
    heapEnd   uintptr // end of heap region
)

//export runtime_start
//go:nosplit
func runtime_start(magic C.uint32_t, mbi C.uintptr_t) {
    const multibootMagic = 0x2BADB002

    // 1) Validate bootloader magic
    if uint32(magic) != multibootMagic {
        // Bad handoff: spin halt
        for {
            asmHLT()
        }
    }

    // 2) Clear BSS segment
    zeroBSS()

    // 3) Initialize Go heap allocator
    initHeap()

    // 4) Configure scheduler & garbage collector
    runtime.GOMAXPROCS(1)
    runtime.GC()

    // 5) Call global init functions (auto-run before main)
    //    In pure Go, init() funcs already run before main.

    // 6) Enter C kernel
    C.kernel_main(magic, mbi)

    // 7) Should never return; halt CPU
    for {
        asmHLT()
    }
}

// zeroBSS sets all bytes in [.bss] to zero.
//go:nosplit
func zeroBSS() {
    start := bssStart
    end   := bssEnd
    length := end - start
    // Create a byte slice pointing to BSS
    mem := unsafe.Slice((*byte)(unsafe.Pointer(start)), length)
    for i := range mem {
        mem[i] = 0
    }
}

// initHeap hands off a contiguous memory region to Go's runtime allocator.
//go:nosplit
func initHeap() {
    start := heapStart
    size  := heapEnd - heapStart
    // This uses an internal runtime function.
//go:linkname runtime_initMem runtime.initMem
    runtime_initMem(unsafe.Pointer(start), size)
    // Mark the heap origin for debugging
    runtime.MemStats{} // force allocation metadata setup
}

// asmHLT executes the HLT instruction to wait for the next interrupt.
//go:nosplit
func asmHLT()

func main() {
    // Prevent Go toolchain from stripping the runtime_start symbol.
}
