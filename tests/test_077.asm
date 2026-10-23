; Test 077 [ERROR]: Lexical error: invalid immediate literal
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... invalid immediate '12345xyz'
section .text
_start:
    mov eax, 12345xyz
