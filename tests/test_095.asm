; Test 095 [ERROR]: Syntax error: label cannot be subtracted in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... a label cannot be subtracted in a memory operand
section .text
my_target: nop
_start:
    mov eax, [ebx-my_target]
