; Test 013: Accumulator conversions and flag transfers (CBW, CWD, CWDE, CDQ, LAHF, SAHF)
section .text
global _start
_start:
    ; Sign conversions
    cbw         ; AL -> AX
    cwd         ; AX -> DX:AX
    cwde        ; AX -> EAX
    cdq         ; EAX -> EDX:EAX

    ; Flag transfers
    lahf        ; AH <- flags
    sahf        ; flags <- AH
    ret
