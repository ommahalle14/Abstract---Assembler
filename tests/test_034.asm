; Test 034: Function CALL and RET
section .text
global _start
_start:
    call helper_func
    call eax
    call dword ptr [ebx + 4]
    ret

helper_func:
    xor eax, eax
    ret
