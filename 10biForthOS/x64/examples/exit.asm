    bits 64

    mov rdi, 42          ; the program return code
    mov rax, 60          ; load the EXIT syscall number into rax
    syscall
