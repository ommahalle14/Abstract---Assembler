; Test 088 [ERROR]: Syntax error: bare label cannot be used as an operand of MUL
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... a label cannot be used as an operand of MUL
section .text
_start:
    mul target_label
target_label:
    nop
