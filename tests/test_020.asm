; Test 020: OR bitwise logic across all sizes
section .text
global _start
_start:
    ; 32-bit OR
    or eax, ebx
    or ecx, 1
    or edx, 0x80000000
    or eax, [esi]
    or [edi + 8], ebx
    or dword ptr [ebx], 0xFF

    ; 16-bit OR
    or ax, bx
    or cx, 0x8000
    or word ptr [esi], 0x12

    ; 8-bit OR
    or al, bl
    or ah, 0x80
    or byte ptr [edx], 0x02
    ret
