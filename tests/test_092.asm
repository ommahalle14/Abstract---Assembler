; Test 092 [ERROR]: Syntax error: scaled index register subtracted in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... a scaled index register cannot be subtracted
section .text
_start:
    mov eax, [ebx-ecx*4]
