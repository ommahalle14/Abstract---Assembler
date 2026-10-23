; Test 078 [ERROR]: Lexical error: invalid special character
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... invalid character '@'
section .text
_start:
    mov eax, @
