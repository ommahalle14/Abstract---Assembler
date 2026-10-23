; Test 005: Index*scale + disp and absolute 32-bit displacement
section .text
global _start
_start:
    ; [index*scale] without base
    mov eax, [ecx*2]
    mov ebx, [edx*4]
    mov esi, [edi*8]

    ; [index*scale + disp]
    mov eax, [ecx*2 + 8]
    mov ebx, [edx*4 + 0x100]
    mov edx, [esi*8 - 16]

    ; [disp32] absolute address
    mov eax, [0]
    mov ebx, [0x1000]
    mov ecx, [0x401000]
    ret
