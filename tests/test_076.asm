; Test 076 [ERROR]: Lexical error: missing closing bracket ']' in memory operand
; Stage: Worklet 1 (tokenizeSourceLine)
; Expected Error: Lexical Error ... missing closing ']' for memory operand
section .text
_start:
    mov eax, [ebx + 4
