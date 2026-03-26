default rel 

section .data
    hex_table db '0123456789ABCDEF', 0

section .bss
    Buffer: resb 256
    BufferEnd equ Buffer + 256
    NumBuffer: resb 64          ; for convertation numbers

section .text
global my_printf

; --------------------------------------------------------------------------
; void my_printf(const char *format, ...)
; --------------------------------------------------------------------------
; * Description: Main output function. Parses the format string and 
;                processes specifiers: %b, %c, %d, %s, %x. 
;                Uses internal buffering to minimize syscalls.
; * Arguments:   RDI - Pointer to the format string
;                RSI, RDX, RCX, R8, R9... - Variadic arguments
; * Preserves:   RBP, R12, R13, R14
; * Destroys:    RAX, RBX, RCX, RDX, RSI, RDI, FLAGS
; --------------------------------------------------------------------------
my_printf:
    push rbp
    push r12            ; r12 = current fmt string pointer
    push r13            ; r13 = current argument pointer
    push r14            ; r14 = current Buffer pointer

    ; trampline
    push r9
    push r8
    push rcx
    push rdx
    push rsi

    mov rbp, rsp
    mov r12, rdi
    mov r13, rbp
    lea r14, [Buffer]

.next_char:
    mov al, [r12]
    inc r12
    test al, al
    jz .done

    cmp al, '%'
    je .handle_format

    movzx rdi, al
    call put_char_buffered
    jmp .next_char

.handle_format:
    mov al, [r12]
    inc r12
    test al, al
    jz .done

    cmp al, '%'
    jne .check_jump_table

    movzx rdi, al
    call put_char_buffered
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
    
    jmp rbx

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
    times ('n' - 'e' + 1) dq 0      ; e..n
    dq handle_o                     ; o
    times ('r' - 'p' + 1) dq 0      ; p..r
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
    call put_char_buffered
    jmp my_printf.next_char

handle_s:
    call get_arg
    mov rdi, rax
    call print_string
    jmp my_printf.next_char

handle_d:
    call get_arg
    movsxd rax, eax
    test rax, rax
    jns .positive
    
    push rax
    mov rdi, '-'
    call put_char_buffered
    pop rax
    neg rax
    
.positive:
    mov rcx, 10
    call print_number_base
    jmp my_printf.next_char

handle_b:
    call get_arg
    mov rcx, 2
    call print_number_base
    jmp my_printf.next_char

handle_x:
    call get_arg
    mov rcx, 16
    call print_number_base
    jmp my_printf.next_char

handle_o:
    call get_arg
    mov rcx, 8
    call print_number_base
    jmp my_printf.next_char



; ==========================================================
; Utils
; ==========================================================

; --------------------------------------------------------------------------
; uint64_t get_arg()
; --------------------------------------------------------------------------
; * Description: Fetches the next argument from the trampoline on the stack.
;                Handles transition from register-based args to stack-based.
; * Arguments:   None (Uses R13)
; * Returns:     RAX - Argument value
; * Preserves:   RDI, RSI, RDX, RCX, R8, R9
; * Destroys:    RAX, RBX, R13, FLAGS
; --------------------------------------------------------------------------
    get_arg:
        mov rax, [r13]
        add r13, 8
        lea rbx, [rbp + 40] 
        cmp r13, rbx
        jne .ok
        add r13, 40         ; r14(8) + r13(8) + r12(8) + rbp(8) + RetAddr(8) = 40
    .ok:
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

; --------------------------------------------------------------------------
; void print_number_base(uint64_t val, uint64_t base)
; --------------------------------------------------------------------------
; * Description: Converts a number to a string using the specified base 
;                (2, 10, or 16) and sends it to the buffer.
; * Arguments:   RAX - Value to convert
;                RCX - Numerical base
; * Preserves:   R12, R13, R14
; * Destroys:    RAX, RBX, RCX, RDX, RSI, RDI, FLAGS
; --------------------------------------------------------------------------
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

; --------------------------------------------------------------------------
; void put_char_buffered(char c)
; --------------------------------------------------------------------------
; * Description: Adds a character to the internal buffer. Triggers 
;                flush_buffer if the buffer reaches capacity (256 bytes).
; * Arguments:   DIL - ASCII character
; * Preserves:   RAX, RBX, RCX, RDX, RSI, RDI
; * Destroys:    R14, FLAGS
; --------------------------------------------------------------------------
put_char_buffered:
    mov [r14], dil
    inc r14

    lea rax, [BufferEnd]
    cmp r14, rax
    je flush_buffer
    ret

; --------------------------------------------------------------------------
; void flush_buffer()
; --------------------------------------------------------------------------
; * Description: Performs a 'write' syscall (stdout) to output the 
;                accumulated data in the buffer and resets the pointer.
; * Arguments:   None
; * Preserves:   RBX, RCX, R8-R15
; * Destroys:    RAX, RDI, RSI, RDX, R11, R14, FLAGS
; --------------------------------------------------------------------------
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
