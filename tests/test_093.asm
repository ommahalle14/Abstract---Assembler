; Test 093 [ERROR]: Syntax error: at most two registers allowed in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... at most two registers (base + index) are allowed in a memory operand
section .text
_start:
    mov eax, [eax+ebx+ecx]
