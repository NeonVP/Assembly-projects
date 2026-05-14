"HashTableContains(HashTable const*, char const*)":
        sub     rsp, 24
        mov     eax, -1
        xor     edx, edx
        crc32   rax, QWORD PTR [rsi]
        mov     QWORD PTR [rsp+16], rbp
        mov     rbp, rsi
        crc32   rax, QWORD PTR [rsi+8]
        crc32   rax, QWORD PTR [rsi+16]
        crc32   rax, QWORD PTR [rsi+24]
        div     QWORD PTR [rdi+16]
        mov     rax, QWORD PTR [rdi]
        mov     esi, DWORD PTR [rax+rdx*4]
        cmp     esi, -1
        je      .L30
        mov     QWORD PTR [rsp+8], rbx
        mov     rbx, rdi
        jmp     .L28
.L37:
        mov     esi, DWORD PTR [rax+32]
        cmp     esi, -1
        je      .L36
.L28:
        mov     rdi, QWORD PTR [rbx+8]
        call    "CFListGetNode(CFList const*, int)"
        vmovdqu ymm0, [rax]
        vpcmpeqb ymm0, ymm0, [rbp]
        vpmovmskb edx, ymm0

        cmp     edx, -1
        jne     .L37
        mov     rbx, QWORD PTR [rsp+8]
        mov     rbp, QWORD PTR [rsp+16]
        mov     eax, 1
        add     rsp, 24
        ret
.L36:
        mov     rbx, QWORD PTR [rsp+8]
.L30:
        mov     rbp, QWORD PTR [rsp+16]
        xor     eax, eax
        add     rsp, 24
        ret
.LC0:
        .long   0
        .long   1138753536