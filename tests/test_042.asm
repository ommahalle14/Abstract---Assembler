; Test 042: String MOVS, CMPS, and SCAS instructions
section .text
global _start
_start:
    ; Move string
    movsb
    movsw
    movsd

    ; Compare string
    cmpsb
    cmpsw
    cmpsd

    ; Scan string
    scasb
    scasw
    scasd
    ret
