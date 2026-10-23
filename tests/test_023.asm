; Test 023: INC and DEC unary operations
section .text
global _start
_start:
    ; 32-bit INC / DEC (single-byte 40+r / 48+r opcodes)
    inc eax
    inc ecx
    inc edx
    inc ebx
    inc esp
    inc ebp
    inc esi
    inc edi

    dec edi
    dec esi
    dec ebp
    dec esp
    dec ebx
    dec edx
    dec ecx
    dec eax

    ; 16-bit INC / DEC
    inc ax
    inc cx
    dec dx
    dec bx

    ; 8-bit INC / DEC
    inc al
    inc ah
    dec bl
    dec bh

    ; Memory INC / DEC
    inc dword ptr [ebx]
    dec dword ptr [esi + 4]
    inc word ptr [ecx]
    dec byte ptr [edx]
    ret
