; Test 048: Multi-section program (.text and .data)
section .data
msg_byte:  db 0x55
msg_word:  dw 0x1234
msg_dword: dd 0xAABBCCDD

section .text
global _start
_start:
    mov al, byte ptr [msg_byte]
    mov ax, word ptr [msg_word]
    mov eax, dword ptr [msg_dword]
    ret
