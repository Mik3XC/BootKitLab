;;; Starting from https://research.h4x.cz/html/2019/2019-10-22--linux--minimal_viable_x86_elf64_static_binary.html
;;; $ nasm smallest_elf64.asm
;;;
;;; $ ls -l ./smallest_elf64
;;; -rwxr-xr-x 1 root root 120 2022-04-26 06:45:22 smallest_elf64*
;;;
;;; Extend it with a full 2 instructions Forth interpreter
;;; $ nasm 10biForth.asm && chmod a+x 10biForth
;;;
;;; $ ls -l ./10biForth
;;; 217 ./10biForth

BITS 64

    ;; MMAP constant
    MEM_SIZE    equ 4096       ; 4KB
    MEM_PROT    equ 7          ; PROT_READ | PROT_WRITE | PROT_EXEC
    MEM_FLAGS   equ 0x22       ; MAP_PRIVATE | MAP_ANONYMOUS

    org     0x0000000000400000

ehdr:                          ; Elf64_Ehdr
    db      0x7F, "ELF"        ;   e_ident[EI_MAG]
    db      2                  ;   e_ident[EI_CLASS]     = 64 bit ELF
    db      1                  ;   e_ident[EI_DATA]      = little endian
    db      1                  ;   e_ident[EI_VERSION]   = ELF version
    db      0                  ;   e_ident[EI_OSABI]     = SysV ABI
; Code:
_start:
    ;; Following https://elixir.bootlin.com/linux/v6.13.6/source/arch/x86/include/asm/elf.h#L156
    ;; all register are zeroed at program startup
    ;;xor     r8, r8           ; start of mmap syscall setting: anonymous memory
    dec     r8                 ; equiv: mov r8, -1
    mov     r10b, MEM_FLAGS    ; pages will not be shared
    jmp     init               ; Not enough space in the header: jump to init
    ;; db 0                       ; padding to not crash the ELF header
; End of Code
    dw      2                  ;   e_type        = executable
    dw      0x3e               ;   e_machine     = x86-64
    dd      1                  ;   e_version     = ELF version
    dq      _start             ;   e_entry
    dq      phdr - $$          ;   e_phoff
    dq      0                  ;   e_shoff
    dd      0                  ;   e_flags
    dw      ehdr_size          ;   e_ehsize
    dw      phdr_size          ;   e_phentsize
    dw      1                  ;   e_phnum
    dw      0                  ;   e_shentsize
    dw      0                  ;   e_shnum
    dw      0                  ;   e_shstrndx
ehdr_size       equ     $ - ehdr
phdr:                              ; Elf64_Phdr
    dd      1                  ;   p_type     = PT_LOAD
    dd      0x05               ;   p_flags    = PF_X | PF_R = rx
    dq      0                  ;   p_offset
    dq      $$                 ;   p_vaddr
    dq      $$                 ;   p_paddr
    dq      file_size          ;   p_filesz
    dq      file_size          ;   p_memsz
    dq      0x200000           ;   p_align
phdr_size       equ     $ - phdr

;;; Register usage
;;;   r15    always code memory base pointer
;;;   r14    current code memory pointer

init:
    ;;  Allocate some memory with mmap to store the code
    ;;xor     r8,r8            ; anonymous memory - no file descriptor
    ;;dec     r8               ; equiv: mov r8, -1
    ;;mov     rax, 9           ; mmap syscall number
    ;;xor     rdi, rdi         ; operating system will choose mapping destination
    add     al, 9              ; mov rax, 9   ; mmap syscall number
    mov     rsi, MEM_SIZE      ; page size
    mov     dl, MEM_PROT       ; new memory region will be marked rwe
    ;;mov     r10b, MEM_FLAGS    ; pages will not be shared
    ;;xor     r9, r9           ; offset (unused)
    syscall                    ; now rax will point to mapped location
                               ; no error check: will segfault in this case

    mov     r15, rax           ; store the code buffer address in r15
                               ; for all the program duration

    mov     r14, r15           ; r14 point to the start of code

loop:                          ; outer interpreter loop
    call    getchar

    cmp     al, 'c'
    je      compile

    cmp     al, 'e'
    je      execute

    jmp loop


compile:
    call    getdigit           ; get the first byte digit
    sal     al, 4
    mov     r12b, al

    call    getdigit           ; get the second byte digit
    or      al, r12b

    mov     byte [r14], al     ; compile the byte in r14 address
    inc     r14                ; r14 point to the next code address

    jmp     loop


execute:
    call    r15                ; call at start of compiled code

    jmp     loop

;;; Read character from keyboard in al
getchar:
    push    rax                ; use the stack as a temp buffer
    xor     rax, rax           ; sys_read
    xor     rdi, rdi           ; stdin
    mov     rsi, rsp           ; buffer to store character
    mov     rdx, 1             ; read one byte
    syscall
    pop     rax                ; restore the stack
    ret

getdigit:                      ; get a digit in al
    call    getchar
    cmp     al, 'A'
    jl      .DIGIT
    add     al, 9
 .DIGIT:
    and     al, 0x0F
    ret

file_size       equ     $ - $$
