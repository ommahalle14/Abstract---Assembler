; Test 037: Unsigned comparison jumps (JB/JNAE/JC, JAE/JNB/JNC, JBE/JNA, JA/JNBE)
section .text
global _start
_start:
    cmp eax, 20
    jb lbl_below
    jnae lbl_below
    jc lbl_below

    jae lbl_ae
    jnb lbl_ae
    jnc lbl_ae

    jbe lbl_be
    jna lbl_be

    ja lbl_a
    jnbe lbl_a

lbl_below:
lbl_ae:
lbl_be:
lbl_a:
    ret
