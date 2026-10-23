; Test 001: MOV between registers (32-bit, 16-bit, and 8-bit)
section .text
global _start
_start:
    ; 32-bit register to register
    mov eax, ebx
    mov ecx, edx
    mov esi, edi
    mov ebp, esp

    ; 16-bit register to register
    mov ax, bx
    mov cx, dx
    mov si, di
    mov bp, sp

    ; 8-bit register to register (low and high)
    mov al, bl
    mov cl, dl
    mov ah, bh
    mov ch, dh
    mov al, ah
    mov bl, ch
    ret
