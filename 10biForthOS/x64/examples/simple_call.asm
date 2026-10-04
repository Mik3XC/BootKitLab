    bits 64

    mov rax, 'Y'
    call putchar

    mov r14, r15      ; reset r14 to start of code

    ret

putchar:
    push rax          ; use the stack as a temp buffer
    mov rax, 1        ; sys_write
    mov rdi, 1        ; stdout
    mov rsi, rsp      ; buffer to store character
    mov rdx, 1        ; write one byte
    syscall
    pop rax

    ret
