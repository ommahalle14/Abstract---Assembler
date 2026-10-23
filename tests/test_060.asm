; Test 060: Comprehensive realistic pipeline program
global _start

section .data
msg:     db "Hello, x86-32!", 10
len equ  14

section .bss
res_buf: resb 32

section .text
_start:
    ; Function prologue
    push ebp
    mov ebp, esp

    ; Setup loop to copy msg into res_buf
    lea esi, msg
    lea edi, res_buf
    mov ecx, len

copy_loop:
    lodsb
    stosb
    loop copy_loop

    ; Write syscall: eax=4, ebx=1, ecx=res_buf, edx=len
    mov eax, 4
    mov ebx, 1
    lea ecx, res_buf
    mov edx, len
    int 0x80

    ; Exit syscall: eax=1, ebx=0
    leave
    mov eax, 1
    xor ebx, ebx
    int 0x80
