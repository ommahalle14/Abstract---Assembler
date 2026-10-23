; Test 046: EQU constant definitions and substitutions
CONST_A equ 10
CONST_B equ 0x20
CONST_C equ 100

section .text
global _start
_start:
    mov eax, CONST_A
    add eax, CONST_B
    cmp eax, CONST_C
    mov dword ptr [ebx + CONST_A], eax
    ret
