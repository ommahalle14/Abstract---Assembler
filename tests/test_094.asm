; Test 094 [ERROR]: Syntax error: multiple labels in memory operand
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... only one label is allowed in a memory operand
section .text
lbl_one: nop
lbl_two: nop
_start:
    mov eax, [lbl_one+lbl_two]
