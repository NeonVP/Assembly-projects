default rel 

section .data
    hex_table db '0123456789ABCDEF', 0

section .bss
    Buffer: resb 256
    BufferEnd equ Buffer + 256
    NumBuffer: resb 64          ; for convertation numbers

section .text
global my_printf

; ==========================================================
; my_printf
; ==========================================================
my_printf:
    ; trampline
    push rbp
    push r12
    push r13        
    push r14

    push r9
    push r8
    push rcx
    push rdx
    push rsi

    mov rbp, rsp
    mov r12, rdi            ; ptr to fmt string
    mov r13, rbp            ; ptr to curr argument
    lea r14, [Buffer]       ; curr ptr in buffer

.next_char:
    mov al, [r12]
    inc r12
    test al, al
    jz .done

    cmp al, '%'
    je .handle_format

    movzx rdi, al
    call print_char
    jmp .next_char

.handle_format:
    mov al, [r12]
    inc r12
    test al, al
    jz .done

    cmp al, '%'
    jne .check_jump_table

    movzx rdi, al
    call print_char
    jmp .next_char

.check_jump_table:
    movzx rax, al
    sub rax, 'b'
    cmp rax, 'x' - 'b'
    ja .next_char

    lea rbx, [jump_table]
    mov rbx, [rbx + rax * 8]
    test rbx, rbx
    jz .next_char
    
    call rbx
    jmp .next_char

.done:
    call flush_buffer
    lea rsp, [rbp + 40]
    pop r14
    pop r13
    pop r12
    pop rbp
    ret

; ==========================================================
; Jump table
; ==========================================================
section .data
jump_table:
    dq handle_b                     ; b
    dq handle_c                     ; c
    dq handle_d                     ; d
    times ('r' - 'e' + 1) dq 0      ; e..r
    dq handle_s                     ; s
    times ('w' - 't' + 1) dq 0      ; t..w
    dq handle_x                     ; x

; ==========================================================
; Handlers
; ==========================================================
section .text

handle_c:
    call get_arg
    mov rdi, rax
    call print_char
    ret

handle_s:
    call get_arg
    mov rdi, rax
    call print_string
    ret

handle_d:
    call get_arg
    movsxd rax, eax
    test rax, rax
    jns .positive
    
    push rax
    mov rdi, '-'
    call print_char
    pop rax
    neg rax
    
.positive:
    mov rcx, 10
    call print_number_base
    ret

handle_b:
    call get_arg
    mov rcx, 2
    call print_number_base
    ret

handle_x:
    call get_arg
    mov rcx, 16
    call print_number_base
    ret

; ==========================================================
; Utils
; ==========================================================
get_arg:
    mov rax, [r13]
    add r13, 8
    lea rbx, [rbp + 40] 
    cmp r13, rbx
    jne .ok
    add r13, 40         ; r14(8) + r13(8) + r12(8) + rbp(8) + RetAddr(8) = 40
.ok:
    ret

print_char:
    call put_char_buffered
    ret

print_string:
    mov rsi, rdi
.loop:
    mov al, [rsi]
    test al, al
    jz .out
    mov dil, al
    
    push rsi
    call put_char_buffered
    pop rsi
    
    inc rsi
    jmp .loop
.out:
    ret

print_number_base:
    lea rsi, [NumBuffer + 63]
    mov byte [rsi], 0
    
    test rax, rax
    jnz .loop
    dec rsi
    mov byte [rsi], '0'
    jmp .print_it

.loop:
    test rax, rax
    jz .print_it
    xor rdx, rdx
    div rcx
    
    lea rbx, [hex_table]
    mov dl, [rbx + rdx]
    dec rsi
    mov [rsi], dl
    jmp .loop

.print_it:
    mov rdi, rsi
    call print_string
    ret

; ==========================================================
; Buffer function
; ==========================================================
put_char_buffered:
    mov [r14], dil
    inc r14

    lea rax, [BufferEnd]
    cmp r14, rax
    je flush_buffer
    ret

flush_buffer:
    lea rsi, [Buffer]
    mov rdx, r14
    sub rdx, rsi
    jz .empty

    mov rax, 1                  ; write
    mov rdi, 1                  ; stdout
    syscall

    lea r14, [Buffer]       ; reset ptr on start
.empty:
    ret
