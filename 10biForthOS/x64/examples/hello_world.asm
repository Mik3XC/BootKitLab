    bits 64

    mov rax, 1         ; sys_write
    mov rdi, 1         ; stdout

    mov rsi, r15       ; msg: relative to r15
    add rsi, msg       ; in the allocated memory

    mov rdx, msglen    ; msg size
    syscall

    mov r14, r15       ; reset r14 to start of code

    ret

msg: db "Hello, world!", 10
msglen: equ $ - msg
