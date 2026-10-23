; Test 083 [ERROR]: Syntax error: missing operand after size directive
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... missing operand after size directive
section .text
_start:
    mov eax, dword ptr
