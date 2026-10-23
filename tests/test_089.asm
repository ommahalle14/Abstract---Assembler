; Test 089 [ERROR]: Syntax error: invalid scale multiplier in SIB memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... invalid memory expression '[ebx+ecx*3]'
section .text
_start:
    mov eax, [ebx+ecx*3]
