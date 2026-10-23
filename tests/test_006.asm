; Test 006: Sized memory operands (BYTE PTR, WORD PTR, DWORD PTR)
section .text
global _start
_start:
    ; DWORD PTR
    mov dword ptr [ebx], 5
    mov dword ptr [ebx + 4], eax
    mov eax, dword ptr [ebx + 8]

    ; WORD PTR
    mov word ptr [ecx], 0x1234
    mov word ptr [ecx + 2], ax
    mov ax, word ptr [ecx + 4]

    ; BYTE PTR
    mov byte ptr [edx], 0xFF
    mov byte ptr [edx + 1], al
    mov al, byte ptr [edx + 2]
    ret
