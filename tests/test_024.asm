; Test 024: NOT and NEG unary operations
section .text
global _start
_start:
    ; 32-bit NOT and NEG
    not eax
    neg eax
    not ebx
    neg ecx

    ; 16-bit NOT and NEG
    not ax
    neg bx

    ; 8-bit NOT and NEG
    not al
    neg ah

    ; Memory operands
    not dword ptr [ebx]
    neg dword ptr [esi + 4]
    not word ptr [ecx]
    neg byte ptr [edx]
    ret
