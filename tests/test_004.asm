; Test 004: SIB memory addressing [base + index*scale + disp]
section .text
global _start
_start:
    ; Base + index (scale 1 implicit or explicit)
    mov eax, [ebx + ecx]
    mov edx, [esi + edi*1]

    ; Base + index with scale factors: 2, 4, 8
    mov eax, [ebx + edx*2]
    mov ecx, [esi + edi*4]
    mov edx, [ebp + eax*8]

    ; Base + index*scale + displacement
    mov eax, [ebx + ecx*4 + 16]
    mov ebx, [esi + edx*8 + 0x20]
    mov edx, [ebp + edi*2 - 8]
    mov ecx, [esp + eax*4 + 12]
    ret
