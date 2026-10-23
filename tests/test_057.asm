; Test 057: Complex memory addressing [base + index*scale + label + disp]
section .data
array_base: dd 1, 2, 3, 4, 5, 6, 7, 8

section .text
global _start
_start:
    mov esi, 2
    mov eax, [ebx + esi*4 + array_base + 8]
    mov edx, [ebp + array_base]
    ret
