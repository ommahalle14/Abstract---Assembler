; Test 056: Memory addressing referencing labels with displacement
section .data
data_table:
    dd 10, 20, 30, 40

section .text
global _start
_start:
    mov eax, [data_table]
    mov ebx, [data_table + 4]
    mov ecx, [data_table + 8]
    mov edx, [data_table + 12]
    ret
