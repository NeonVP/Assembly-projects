default rel 

; 8 глава - что такое put_char
extern printf

section .data
    hex_table db '0123456789ABCDEF', 0

    inf_str db 'inf', 0
    nan_str db 'nan', 0
    float_1M dq 1000000.0
    float_half dq 0.5
    xmm_counter dq 0
    float_abs_mask equ 0x7FFFFFFFFFFFFFFF

    xmm_arg_idx db 0        ; counter for XMM arguments

section .bss
    buffer: resb 256
    buffer_end equ buffer + 256
    num_buffer: resb 64          ; for convertation numbers

    save_registers: resq 6
    save_xmm: resq 8


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
    ; --- Save GP registers ---
    lea rax, [save_registers]
    mov [rax], rdi
    mov [rax + 8], rsi
    mov [rax + 16], rdx
    mov [rax + 24], rcx
    mov [rax + 32], r8
    mov [rax + 40], r9

    ; --- Save XMM registers ---
    lea rax, [save_xmm]
    movq [rax + 0],  xmm0
    movq [rax + 8],  xmm1
    movq [rax + 16], xmm2
    movq [rax + 24], xmm3
    movq [rax + 32], xmm4
    movq [rax + 40], xmm5
    movq [rax + 48], xmm6
    movq [rax + 56], xmm7

    mov qword [xmm_counter], 0

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
    lea r14, [buffer]       ; curr ptr in buffer

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
    
    ; --- Restore Registers ---
    lea rax, [save_registers]
    mov rdi, [rax]
    mov rsi, [rax + 8]
    mov rdx, [rax + 16]
    mov rcx, [rax + 24]
    mov r8,  [rax + 32]
    mov r9,  [rax + 40]

    ; --- Restore XMM Registers ---
    lea rax, [save_xmm]
    movq xmm0, [rax + 0]
    movq xmm1, [rax + 8]
    movq xmm2, [rax + 16]
    movq xmm3, [rax + 24]
    movq xmm4, [rax + 32]
    movq xmm5, [rax + 40]
    movq xmm6, [rax + 48]
    movq xmm7, [rax + 56]
    
    mov al, 8               ; Tell original printf that 8 XMM regs are used
    jmp printf wrt ..plt

; ==========================================================
; Jump table
; ==========================================================
section .data
jump_table:
    dq handle_b                                  ; b
    dq handle_c                                  ; c
    dq handle_d                                  ; d
    dq handle_default                            ; e
    dq handle_f                                  ; f
    times ('n' - 'g' + 1) dq handle_default      ; g..n
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

handle_f:
    call get_float_arg
    call print_float
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
; double get_float_arg()
; --------------------------------------------------------------------------
; * Description: Fetches the next floating point argument. Reads from 
;                saved XMM registers first, then falls back to the stack.
; * Returns:     XMM0 - Argument value
; --------------------------------------------------------------------------
get_float_arg:
    mov rax, [xmm_counter]
    cmp rax, 8
    jge .stack_arg
    
    lea rcx, [save_xmm]
    movsd xmm0, [rcx + rax * 8]
    inc rax
    mov [xmm_counter], rax
    ret
    
.stack_arg:
    movq xmm0, [r13]
    add r13, 8
    lea rbx, [rbp + 40] 
    cmp r13, rbx
    jne .ok
    add r13, 40         ; Перепрыгиваем сохраненные R14, R13, R12, RBP, RetAddr
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
    lea rsi, [num_buffer + 63]
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
    lea rsi, [num_buffer + 63] ; Point to the end of string buffer
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
; void print_float(double xmm0)
; --------------------------------------------------------------------------
; * Description: Prints a 64-bit IEEE-754 float. Handles negatives, 
;                INF, NAN, denormals, and rounds to 6 decimal places.
; * Arguments:   XMM0 - double to print
; * Destroys:    RAX, RBX, RCX, RDX, RSI, RDI, XMM0, XMM1, XMM2, FLAGS
; --------------------------------------------------------------------------
print_float:
    movq rax, xmm0
    
    ; Extracrt exponent
    mov rcx, rax
    shr rcx, 52
    and rcx, 0x7FF          ; 11-bit Exponent mask
    
    ; Extract mantissa
    mov rdx, rax
    mov r8, 0xFFFFFFFFFFFFF
    and rdx, r8             ; 52-bit Mantissa mask
    
    ; Check sign
    test rax, rax           
    jns .positive           ; positive -> skip '-'
    
    push rax
    push rcx
    push rdx
    mov rdi, '-'
    call put_char_buffered
    pop rdx
    pop rcx
    pop rax
    
    mov r8, float_abs_mask
    and rax, r8
    movq xmm0, rax
    
.positive:
    ; Exponent = 0x7FF -> INF or NaN
    cmp rcx, 0x7FF
    jne .normal_number
    
    test rdx, rdx
    jnz .is_nan
.is_inf:
    lea rdi, [inf_str]
    call print_string
    ret
.is_nan:
    lea rdi, [nan_str]
    call print_string
    ret
    
.normal_number:
    cvttsd2si rax, xmm0     ; RAX = decimal
    cvtsi2sd xmm1, rax      ; XMM1 = decimal in double
    
    movsd xmm2, xmm0
    subsd xmm2, xmm1        ; XMM2 = 0.xxxxxx
    
    movsd xmm1, [float_1M]
    mulsd xmm2, xmm1        ; multiply on million
    
    movsd xmm1, [float_half]
    addsd xmm2, xmm1        ; Add 0.5 (round half up) for round
    
    cvttsd2si rbx, xmm2     ; RBX = round fraction
    
    ; Check for fractional part overflow (0.9999999 -> 1.000000)
    cmp rbx, 1000000
    jl .print_parts
    inc rax                 ; Add 1 for decimal part
    sub rbx, 1000000        ; Reset the fraction
    
.print_parts:
    push rbx                ; Save fraction
    
    call print_number_decimal
    
    mov rdi, '.'
    call put_char_buffered
    
    pop rax
    add rax, 1000000        ; Add 1M to keep the zeros in front (5 -> 1000005)
    
    mov rcx, 10
    lea rsi, [num_buffer + 63]
    mov byte [rsi], 0
    
    mov r8, 6               ; Six numbers in fraction
.frac_loop:
    xor rdx, rdx
    div rcx
    add dl, '0'
    dec rsi
    mov [rsi], dl
    dec r8
    jnz .frac_loop
    
    mov rdi, rsi
    call print_string
    ret


; ==========================================================
; buffer function
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

    lea rax, [buffer_end]
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
    lea rsi, [buffer]
    mov rdx, r14
    sub rdx, rsi
    jz .empty

    mov rax, 1                  ; write
    mov rdi, 1                  ; stdout
    syscall

    lea r14, [buffer]       ; reset ptr on start
.empty:
    ret
