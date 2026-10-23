; Test 041: String LODS and STOS instructions
section .text
global _start
_start:
    ; Specific byte, word, and dword string instructions
    lodsb
    lodsw
    lodsd

    stosb
    stosw
    stosd

    ; Generic LODS with register/size operand
    lods al
    lods ax
    lods eax
    lods byte ptr [esi]
    lods word ptr [esi]
    lods dword ptr [esi]

    ; Generic STOS with register/size operand
    stos al
    stos ax
    stos eax
    stos byte ptr [edi]
    stos word ptr [edi]
    stos dword ptr [edi]
    ret
