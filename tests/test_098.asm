; Test 098 [ERROR]: Syntax error: instruction has more than 3 operands
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... more than 3 operands
section .text
_start:
    mov eax, ebx, ecx, edx
