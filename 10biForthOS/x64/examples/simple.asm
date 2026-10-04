    bits 64

    mov rax, 'X'      ; use the stack as a temp buffer
    push rax
    mov rax, 1        ; sys_write
    mov rdi, 1        ; stdout
    mov rsi, rsp      ; buffer to store the character
    mov rdx, 1        ; write one byte
    syscall
    pop rax

    mov r14, r15      ; reset r14 to start of code

    ret
