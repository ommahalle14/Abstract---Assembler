; Test 050: String literals in .data (double and single quotes)
section .data
str_hello: db "Hello, World!", 0
str_quote: db 'Assembly language is fast', 10, 0

section .text
global _start
_start:
    lea esi, str_hello
    lea edi, str_quote
    lodsb
    ret
