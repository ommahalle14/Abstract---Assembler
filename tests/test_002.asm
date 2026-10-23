; Test 002: MOV register with all immediate formats (decimal, hex, binary, octal, negative)
section .text
global _start
_start:
    ; Decimal immediates
    mov eax, 100
    mov ebx, -42
    mov ecx, 255d

    ; Hexadecimal formats (0x and 'h' suffix)
    mov edx, 0x12345678
    mov esi, 0FFH
    mov edi, 0x7FFFFFFF

    ; Binary format ('b' suffix)
    mov al, 10101010b
    mov ah, 11110000b

    ; Octal format ('o' and 'q' suffix)
    mov cl, 77o
    mov ch, 177q

    ; Sized 16-bit immediates
    mov ax, 0x1234
    mov bx, -300
    ret
