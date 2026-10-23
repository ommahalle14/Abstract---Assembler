; Test 049: Uninitialized data section (.bss) with RESB, RESW, RESD
section .bss
buffer: resb 64
array:  resw 16
var32:  resd 4

section .text
global _start
_start:
    mov dword ptr [var32], 123
    mov word ptr [array], 456
    mov byte ptr [buffer], 78
    ret
