bits 32
section .multiboot
    align 4
    dd 0x1BADB002              ; Multiboot magic number
    dd 0x00                    ; Flags
    dd - (0x1BADB002 + 0x00)   ; Checksum

section .text
global _start
extern kernel_main

_start:
    cli                        ; Disable interrupts
    mov esp, stack_top         ; Set up stack pointer
    call kernel_main           ; Jump into C++ Kernel
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
resb 16384                     ; 16 KB Kernel Stack
stack_top:
