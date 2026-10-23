; Test 075 [ERROR]: Division by zero in EQU expression
; Stage: Worklet 3 (splitSource)
; Expected Error: division by zero
DIV_ZERO equ 100 / 0

section .text
_start:
    mov eax, DIV_ZERO
