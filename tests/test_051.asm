; Test 051: TIMES repeat directive in .data section
section .data
zeros_byte:  times 10 db 0
ones_dword:  times 4 dd 0xFFFFFFFF
pattern:     times 8 dw 0xAAAA

section .text
global _start
_start:
    mov eax, dword ptr [zeros_byte]
    mov ebx, dword ptr [ones_dword]
    ret
