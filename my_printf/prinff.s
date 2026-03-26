default rel 

extern printf

section .data
    hex_table db '0123456789ABCDEF', 0
    save_registers: dq 0, 0, 0, 0, 0, 0

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
    ; save original registers to memory first
    lea rax, [save_registers]
    mov [rax], rdi
    mov [rax + 8], rsi
    mov [rax + 16], rdx
    mov [rax + 24], rcx
    mov [rax + 32], r8
    mov [rax + 40], r9

    push rbp
    push r12
    push r13        
    push r14

    ; trampline
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
    call put_char_buffered
    jmp .next_char

.handle_format:
    mov al, [r12]
    inc r12
    test al, al
    jz .done

    cmp al, '%'
    jne .check_jump_table

.check_jump_table:
    movzx rcx, al
    movzx rax, al
    
    cmp al, 'b'
    jb .next_char
    cmp al, 'x'
    ja .next_char

    lea rbx, [jump_table]
    jmp [rbx + (rax - 'b') * 8]

.done:
    call flush_buffer
    lea rsp, [rbp + 40]
    pop r14
    pop r13
    pop r12
    pop rbp
    
    lea rax, [save_registers]
    mov rdi, [rax]
    mov rsi, [rax + 8]
    mov rdx, [rax + 16]
    mov rcx, [rax + 24]
    mov r8,  [rax + 32]
    mov r9,  [rax + 40]
    
    xor al, al
    jmp printf wrt ..plt

; ==========================================================
; Jump table
; ==========================================================
section .data
jump_table:
    dq handle_b                                  ; b
    dq handle_c                                  ; c
    dq handle_d                                  ; d
    times ('n' - 'e' + 1) dq handle_default      ; e..n
    dq handle_o                                  ; o
    times ('r' - 'p' + 1) dq handle_default      ; p..r
    dq handle_s                                  ; s
    times ('w' - 't' + 1) dq handle_default      ; t..w
    dq handle_x                                  ; x

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
    call print_number_decimal
    jmp my_printf.next_char

handle_b:
    call get_arg
    mov rcx, 1
    call print_number_power2
    jmp my_printf.next_char

handle_x:
    call get_arg
    mov rcx, 4
    call print_number_power2
    jmp my_printf.next_char

handle_o:
    call get_arg
    mov rcx, 3
    call print_number_power2
    jmp my_printf.next_char

handle_default:
    mov rdi, '%'
    call put_char_buffered
    mov rdi, rcx
    call put_char_buffered
    jmp my_printf.next_char

; ==========================================================
; Utils
; ==========================================================

; --------------------------------------------------------------------------
; uint64_t get_arg()
; --------------------------------------------------------------------------
; * Description: Fetches the next argument from the trampoline on the stack.
;                Handles transition from register-based args to stack-based.
; * Arguments:   None (Uses R13 state)
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


; --------------------------------------------------------------------------
; void print_string(const char *str)
; --------------------------------------------------------------------------
; * Description: Copies a null-terminated string to the output buffer.
; * Arguments:   RDI - Pointer to the string
; * Preserves:   R12, R13, R14
; * Destroys:    RAX, RSI, RDI, FLAGS
; --------------------------------------------------------------------------
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
; void print_number_decimal(int64_t val)
; --------------------------------------------------------------------------
; * Description: Converts a signed 64-bit integer to a decimal string 
;                and sends it to the internal buffer.
; * Arguments:   RAX - Signed value to convert
; * Returns:     None
; * Preserves:   R12, R13, R14
; * Destroys:    RAX, RBX, RCX, RDX, RSI, RDI, FLAGS
; --------------------------------------------------------------------------
print_number_decimal:
    ; --- Handle sign ---
    test rax, rax
    jns .prepare_buffer      ; If positive, go to conversion
    
    push rax                 ; Save original value
    mov rdi, '-'
    call put_char_buffered
    pop rax
    neg rax                  ; Make value positive 

.prepare_buffer:
    lea rsi, [NumBuffer + 63] ; Point to the end of string buffer
    mov byte [rsi], 0         ; Null-terminator
    mov rcx, 10               ; Divisor for decimal system

    ; --- Special case for zero ---
    test rax, rax
    jnz .loop
    dec rsi
    mov byte [rsi], '0'
    jmp .print_it

.loop:
    test rax, rax
    jz .print_it
    xor rdx, rdx              ; Clear RDX for 64-bit division
    div rcx                   ; RAX = quotient, RDX = remainder (0-9)
    
    add dl, '0'               ; Convert remainder to ASCII ('0'-'9')
    dec rsi                   ; Move buffer pointer to the left
    mov [rsi], dl             ; Store character
    jmp .loop

.print_it:
    mov rdi, rsi
    call print_string
    ret


; --------------------------------------------------------------------------
; void print_number_power2(uint64_t val, uint64_t bits)
; --------------------------------------------------------------------------
; * Description: Fast conversion of an unsigned 64-bit integer to string 
;                for bases 2, 8, or 16 using bitwise shifts and masks.
; * Arguments:   RAX - Unsigned value to convert
;                RCX - Bits per digit (1=bin, 3=oct, 4=hex)
; * Returns:     None
; * Preserves:   R12, R13, R14
; * Destroys:    RAX, RBX, RCX, RDX, RSI, RDI, R8, FLAGS
; --------------------------------------------------------------------------
print_number_power2:
    lea rsi, [NumBuffer + 63]
    mov byte [rsi], 0
    
    ; RCX=4 (hex) -> 1 << 4 = 16, 16 - 1 = 15 (0xF)
    mov r8, 1
    shl r8, cl
    dec r8                  ; R8 = bitmask (e.g., 0xF for hex)
    
    test rax, rax
    jnz .loop
    dec rsi
    mov byte [rsi], '0'
    jmp .print_it

.loop:
    test rax, rax
    jz .print_it
    
    mov rdx, rax
    and rdx, r8
    
    lea rbx, [hex_table]
    mov dl, [rbx + rdx]
    dec rsi
    mov [rsi], dl
    
    shr rax, cl             ; RAX = RAX >> bits
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
