; Test 079 [ERROR]: Lexical error: label name is a reserved register keyword
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... label name 'eax' is a reserved word
section .text
eax:
    nop
