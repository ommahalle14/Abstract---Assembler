; Test 010: Stack state manipulation (PUSHAD, POPAD, PUSHA, POPA, PUSHFD, POPFD, PUSHF, POPF)
section .text
global _start
_start:
    ; 32-bit stack instructions
    pushad
    popad
    pushfd
    popfd

    ; 16-bit stack instructions (0x66 prefix)
    pusha
    popa
    pushf
    popf
    ret
