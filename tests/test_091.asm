; Test 091 [ERROR]: Syntax error: register subtracted in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... a register cannot be subtracted in a memory operand
section .text
_start:
    mov eax, [ebx-ecx]
