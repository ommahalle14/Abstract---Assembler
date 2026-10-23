; Test 022: TEST non-destructive logical comparison
section .text
global _start
_start:
    ; Testing registers
    test eax, eax
    test eax, ebx
    test ecx, 1
    test edx, 0x100

    ; Testing memory
    test dword ptr [ebx], 0x80
    test eax, [esi]
    test [edi], eax

    ; 16-bit and 8-bit TEST
    test ax, bx
    test cx, 0x01
    test word ptr [ebx], 0x10
    test al, al
    test ah, 0x80
    test byte ptr [edx], 0x01
    ret
