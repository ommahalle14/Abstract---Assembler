; Test 038: Flag conditional jumps (JS, JNS, JO, JNO, JP/JPE, JNP/JPO)
section .text
global _start
_start:
    test eax, eax
    js lbl_sign
    jns lbl_nosign

    add eax, 1
    jo lbl_overflow
    jno lbl_nooverflow

    test al, 0xFF
    jp lbl_parity
    jpe lbl_parity
    jnp lbl_noparity
    jpo lbl_noparity

lbl_sign:
lbl_nosign:
lbl_overflow:
lbl_nooverflow:
lbl_parity:
lbl_noparity:
    ret
