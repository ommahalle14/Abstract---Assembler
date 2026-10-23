; Test 080 [ERROR]: Lexical error: label name is a reserved instruction mnemonic
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... label name 'mov' is a reserved word
section .text
mov:
    nop
