; Test 045: LEAVE instruction for stack frame unwinding
section .text
global _start
_start:
    ; Standard function prologue
    push ebp
    mov ebp, esp
    sub esp, 16

    ; Function body
    mov dword ptr [ebp - 4], 42

    ; Epilogue using LEAVE
    leave
    ret
