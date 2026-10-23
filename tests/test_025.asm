; Test 025: MUL and DIV unsigned arithmetic
section .text
global _start
_start:
    ; 32-bit MUL and DIV
    mul ecx
    mul edx
    mul dword ptr [ebx]
    div esi
    div edi
    div dword ptr [ebp - 4]

    ; 16-bit MUL and DIV
    mul cx
    mul word ptr [ebx]
    div dx
    div word ptr [esi]

    ; 8-bit MUL and DIV
    mul cl
    mul byte ptr [edx]
    div bl
    div byte ptr [edi]
    ret
