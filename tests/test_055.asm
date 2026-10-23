; Test 055: Data section relocations (pointers to code and data)
section .data
target_var: dd 100
ptr_var:    dd target_var
ptr_func:   dd my_func
ptr_offset: dd target_var + 4

section .text
global _start
_start:
    mov eax, dword ptr [ptr_var]
    call my_func
    ret

my_func:
    xor eax, eax
    ret
