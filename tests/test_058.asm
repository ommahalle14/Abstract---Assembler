; Test 058: Register mix across 32, 16, and 8-bit sizes in pipeline
section .text
global _start
_start:
    mov eax, 0x11223344
    mov ax, 0x5566
    mov al, 0x77
    mov ah, 0x88

    mov ebx, 0x99AABBCC
    mov bx, 0xDDEE
    mov bl, 0xFF
    mov bh, 0x00

    mov ecx, eax
    mov cx, bx
    mov cl, al
    mov ch, bh
    ret
