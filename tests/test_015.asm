; Test 015: ADC (Add with Carry) across all operand shapes
section .text
global _start
_start:
    ; 32-bit ADC
    adc eax, ebx
    adc ecx, 1
    adc edx, [ebx + 8]
    adc [esi + 4], eax
    adc dword ptr [edi], 10

    ; 16-bit ADC
    adc ax, bx
    adc cx, 5
    adc dx, [esi]
    adc [edi], ax

    ; 8-bit ADC
    adc al, bl
    adc ah, 1
    adc byte ptr [ebx], 0xFF
    ret
