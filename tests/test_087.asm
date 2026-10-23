; Test 087 [ERROR]: Syntax error: rel8 branch (LOOP) cannot reference external symbol
; Stage: Worklet 1 (parseAssemblySource)
; Expected Error: Syntax Error ... LOOP cannot reference an external symbol (rel8)
extern ext_func
section .text
_start:
    loop ext_func
