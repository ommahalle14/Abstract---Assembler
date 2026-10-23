; Test 008: PUSH and POP with all 32-bit and 16-bit registers
section .text
global _start
_start:
    ; 32-bit register PUSH and POP
    push eax
    push ecx
    push edx
    push ebx
    push esp
    push ebp
    push esi
    push edi

    pop edi
    pop esi
    pop ebp
    pop esp
    pop ebx
    pop edx
    pop ecx
    pop eax

    ; 16-bit register PUSH and POP
    push ax
    push cx
    push dx
    push bx
    push sp
    push bp
    push si
    push di

    pop di
    pop si
    pop bp
    pop sp
    pop bx
    pop dx
    pop cx
    pop ax
    ret
