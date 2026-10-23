; Test 021: XOR bitwise logic and register clearing
section .text
global _start
_start:
    ; Clearing registers with XOR
    xor eax, eax
    xor ecx, ecx
    xor edx, edx
    xor ebx, ebx
    xor esi, esi
    xor edi, edi

    ; XOR with memory and immediate
    xor eax, [ebx + 4]
    xor [esi], edx
    xor dword ptr [edi], 0xAA55AA55

    ; 16-bit and 8-bit XOR
    xor ax, bx
    xor word ptr [ebx], 0xFFFF
    xor al, bl
    xor byte ptr [edx], 0x0F
    ret
