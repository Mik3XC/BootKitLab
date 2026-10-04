#ifdef __APPLE__
#define GSYM(x) _##x
#define CSTRING_SECTION .section __TEXT,__cstring,cstring_literals
#else
#define GSYM(x) x
#define CSTRING_SECTION .section .rodata
#endif

CSTRING_SECTION
mbr_marker_string:
    .asciz "SIM-MBR-STAGE-MARKER-V1"

.text
.p2align 2
.globl GSYM(bks_mbr_stage_marker)
GSYM(bks_mbr_stage_marker):
#if defined(__x86_64__)
    lea mbr_marker_string(%rip), %rax
    ret
#elif defined(__aarch64__)
  #ifdef __APPLE__
    adrp x0, mbr_marker_string@PAGE
    add x0, x0, mbr_marker_string@PAGEOFF
  #else
    adrp x0, mbr_marker_string
    add x0, x0, :lo12:mbr_marker_string
  #endif
    ret
#else
  #error Unsupported architecture for stage marker assembly.
#endif
