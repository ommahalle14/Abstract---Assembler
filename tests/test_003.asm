; Test 003: Memory addressing with [base] and [base + disp]
section .text
global _start
_start:
    ; Simple base register addressing
    mov eax, [ebx]
    mov ecx, [esi]
    mov edx, [edi]
    mov ebx, [ebp]
    mov esi, [esp]

    ; Base + positive displacement (disp8 and disp32)
    mov eax, [ebx + 4]
    mov ecx, [esi + 128]
    mov edx, [edi + 0x1000]

    ; Base + negative displacement
    mov eax, [ebp - 4]
    mov ebx, [ebp - 8]
    mov ecx, [esp + 16]
    ret
