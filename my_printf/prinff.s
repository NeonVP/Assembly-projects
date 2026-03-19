section .code

global _start

_start:
    mov eax, 3      ; read
    mov ebx, 0      ; standart input
    mov ecx, Buffer
    mov edx, BufLen
    int 0x80

    mov eax, 4         ; write (ebx, ecx, edx)
    mov ebx, 1         ; stdout
    mov ecx, Buffer
    mov edx, BufLen    ; strlen (Msg)
    int 0x80
    
    mov eax, 1         ; exit (ebx)
    xor ebx, ebx
    int 0x80


section .bss
    Buffer: resb 256
    BufLen  equ 1024