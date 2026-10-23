; Test 054: EXTERN symbol declarations and external calls
extern external_calc
extern external_data_table

section .text
global _start
_start:
    call external_calc
    mov eax, dword ptr [external_data_table]
    ret
