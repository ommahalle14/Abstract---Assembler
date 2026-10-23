; Test 062 [ERROR]: Duplicate EQU definition
; Stage: Worklet 3 (splitSource)
; Expected Error: duplicate equ definition of 'BUFFER_SIZE'
BUFFER_SIZE equ 1024
BUFFER_SIZE equ 2048

section .text
_start:
    mov eax, BUFFER_SIZE
