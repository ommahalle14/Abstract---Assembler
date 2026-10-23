; Test 047: EQU with arithmetic expressions (+, -, *, /)
VAL_BASE  equ 10
VAL_ADD   equ VAL_BASE + 5
VAL_SUB   equ 20 - 4
VAL_MULT  equ 8 * 4
VAL_DIV   equ 100 / 2

section .text
global _start
_start:
    mov eax, VAL_ADD
    mov ebx, VAL_SUB
    mov ecx, VAL_MULT
    mov edx, VAL_DIV
    ret
