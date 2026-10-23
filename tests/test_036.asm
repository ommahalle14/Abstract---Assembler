; Test 036: Signed comparison conditional jumps (JL/JNGE, JGE/JNL, JLE/JNG, JG/JNLE)
section .text
global _start
_start:
    cmp eax, 10
    jl lbl_less
    jnge lbl_less

    jge lbl_ge
    jnl lbl_ge

    jle lbl_le
    jng lbl_le

    jg lbl_g
    jnle lbl_g

lbl_less:
lbl_ge:
lbl_le:
lbl_g:
    ret
