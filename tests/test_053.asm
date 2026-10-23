; Test 053: GLOBAL symbols export
global _start
global my_exported_function
global my_exported_data

section .data
my_exported_data: dd 42

section .text
_start:
    call my_exported_function
    ret

my_exported_function:
    mov eax, dword ptr [my_exported_data]
    ret
