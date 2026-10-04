# ChainLoader Master Guide

This file is the master teaching guide for the ChainLoader / BootKitStudio repository.
It explains what the project does, how the defensive simulator is wired together,
and how to read each assembly file line by line.

The short version: this repository is not an operating bootkit and does not write
to boot sectors or firmware. The working C++ program is a defensive lab simulator.
It reads scenario definitions, enforces lab guardrails, resolves tiny assembly
stage-marker functions, hashes VM disk images, and writes JSON evidence bundles.

## Files Reviewed

Markdown files:

- `Playbook.md`: Defines the defensive-only mission, lab phases, scenario matrix,
  detection controls, hardening controls, and production promotion policy.
- `BootKitStudio-Architecture.md`: Explains the C++ orchestration core, assembly
  stage simulators, runtime contracts, GUI, guardrails, runner modes, and evidence.
- `IR-Boot-Chain-Runbook.md`: Provides triage, containment, recovery, and closure
  workflows for boot-chain drift alerts.
- `GUI.md`: Explains the local GUI, artifact dropper, ASM stager, evidence viewer,
  and API endpoints.
- `Classroom-PoC-Guide.md`: Defines the non-operational class PoC, mock
  installer phases, ACPI terminology boundary, detection mapping, and rubric.

Assembly and assembly-like files:

- `bootkitstudio/asm/mbr_stage_marker.S`: Safe MBR marker function.
- `bootkitstudio/asm/vbr_stage_marker.S`: Safe VBR marker function.
- `bootkitstudio/asm/uefi_stage_marker.S`: Safe UEFI marker function.
- `bootkitstudio/asm/acpi_annotation_marker.S`: Safe ACPI annotation marker
  function for classroom evidence only.
- `ChainLoad-PIT-Beep.s`: Linux x86_64 port-I/O speaker experiment. It is marked
  deprecated/non-viable in `Source-Claims.yaml` and is not part of the core
  defensive runner.
- `gui/dropzone/asm-staged/20260321T195114Z.ChainLoad-PIT-Beep.s`: A staged copy
  of `ChainLoad-PIT-Beep.s`; the content is identical.
- `Chain-Loader-SetStack-2.txt`: A 16-bit real-mode chain-load sketch. It is a
  conceptual note, not a complete boot sector.
- `ChainLoadVBR.txt`: A historical VBR-chainloading note. It is useful for boot
  vocabulary, but this guide treats it as defensive background, not an execution
  recipe.

Supporting project files:

- `CMakeLists.txt`: Builds the C++ runner, tests, core library, and assembly
  marker object files.
- `Scenario-Matrix.yaml`: Defines the three lab scenarios.
- `bootkitstudio/manifests/Stage-Simulator-Manifest.yaml`: Defines the marker
  contract for each assembly simulator.
- `bootkitstudio/manifests/Mock-Installer-Manifest.yaml`: Defines the
  non-operational classroom mock installer phases and prohibited actions.
- `bootkitstudio/config/lab_config.yaml`: Defines lab-only guardrails.
- `Source-Claims.yaml`: Normalizes the original source corpus into validated,
  needs-lab-validation, and deprecated/non-viable claims.
- `Risk-Register.yaml`: Maps boot-stage risks to defensive controls and owners.

## Safety Boundary

Boot-chain research can become destructive very quickly because real MBR, VBR,
ESP, BCD, and firmware state are part of machine startup. This repository's
current implementation avoids that path.

The safe boundary in this repo is:

- No physical disk writes.
- No firmware writes.
- No deployment of staged assembly.
- No boot-sector patching from the runner or GUI.
- No real `bootmgr` or `winload` patching.
- No persistent installer.
- No live ACPI or AML modification.
- Lab mode required through `BKS_LAB_MODE=1`.
- VM disk image paths must stay under `lab/vms`.
- Evidence is emitted as JSON under `output/evidence`.

The older notes discuss MBR/VBR/bootmgr vocabulary and risks. The implemented
program converts that topic into marker-only simulation and evidence generation.

## Boot Chain Vocabulary

Legacy BIOS / MBR path:

```text
BIOS -> MBR -> VBR -> bootmgr -> winload
```

- BIOS starts the machine and loads the first disk sector to memory.
- MBR means Master Boot Record. In legacy boot, it contains small bootstrap code,
  a partition table, and the `0x55AA` boot signature.
- VBR means Volume Boot Record. It is the first sector of a partition and usually
  continues the handoff toward the operating system loader.
- `bootmgr` is the Windows Boot Manager used in modern Windows legacy boot flows.
- `winload` is the Windows OS loader stage.

UEFI path:

```text
Firmware -> ESP -> bootmgfw.efi -> winload.efi
```

- UEFI firmware uses boot entries and an EFI System Partition, often called ESP.
- `bootmgfw.efi` is the Windows Boot Manager executable for UEFI boot.
- Secure Boot can enforce signature policy before pre-OS components execute.

BootKitStudio does not implement these boot chains directly. It models them as
scenarios and stage-marker events so defenders can validate controls safely.

## What The Working Program Does

The executable is `build/bootkitstudio`, built from `bootkitstudio/src/main.cpp`.

At runtime, the program:

1. Parses command-line flags such as `--mode`, `--scenario`, `--repo-root`,
   `--scenario-matrix`, `--manifest`, `--guardrails`, and `--output`.
2. Loads scenarios from `Scenario-Matrix.yaml`.
3. Loads stage simulator metadata from
   `bootkitstudio/manifests/Stage-Simulator-Manifest.yaml`.
4. Loads the non-operational mock installer manifest from
   `bootkitstudio/manifests/Mock-Installer-Manifest.yaml`.
5. Loads lab guardrails from `bootkitstudio/config/lab_config.yaml`.
6. Verifies that lab mode and scenario preconditions are present.
7. Rejects physical disk paths and VM disk images outside `lab/vms`.
8. Hashes the selected VM disk image.
9. Calls assembly marker functions through `ResolveStageMarker`.
10. Emits mock-installer evidence events such as `would_stage`, `would_hook`,
    `would_chain`, and `would_restore`.
11. Writes evidence as JSON.

The three runner modes are:

- `baseline`: collect marker and disk-image baseline evidence only.
- `controlled-drift`: write a harmless `controlled_drift.flag` under the VM
  profile directory and emit expected detection alerts.
- `recovery-validation`: remove the drift flag and emit expected hardening alerts.

## Main Data Flow

```text
CLI flags
  |
  v
main.cpp
  |
  v
Runner::Run()
  |
  +--> ParseScenarioMatrix(Scenario-Matrix.yaml)
  +--> ParseStageSimulatorManifest(Stage-Simulator-Manifest.yaml)
  +--> ParseGuardrailConfig(lab_config.yaml)
  |
  v
Runner::RunScenario()
  |
  +--> ValidateScenarioSpec()
  +--> ValidateStageManifest()
  +--> ValidateScenarioGuardrails()
  +--> Sha256File(vm_disk_image)
  +--> ResolveStageMarker(module_id)
  +--> Fnv1a64(marker_string)
  |
  v
EvidenceToJson()
  |
  v
output/evidence/<scenario>.<mode>.<timestamp>.json
```

## Scenario Matrix

`Scenario-Matrix.yaml` defines three lab profiles:

- `win10-bios-mbr`: Legacy BIOS/MBR profile with MBR and VBR marker simulators.
- `win11-uefi-sb-off`: UEFI profile with Secure Boot off and UEFI marker
  simulator.
- `win11-uefi-sb-on`: UEFI profile with Secure Boot on and UEFI marker
  simulator.

Each scenario includes:

- `preconditions`: Required safety gates such as snapshot, rollback, evidence
  capture, and lab mode.
- `safe_action`: Human-readable statement of what the lab run is allowed to do.
- `expected_detection`: Detection alerts expected during drift simulation.
- `expected_hardening`: Hardening assertions expected during recovery validation.
- `vm_disk_image`: VM image path used for hashing.
- `stage_simulators`: Assembly marker module IDs.
- `drift_action`: The simulated drift concept.
- `rollback_policy`: The expected lab rollback behavior.
- `timeouts`: Boot and evidence timing metadata.

## Stage Simulator Manifest

`bootkitstudio/manifests/Stage-Simulator-Manifest.yaml` maps module IDs to marker
contracts:

- `sim.mbr.marker` returns `SIM-MBR-STAGE-MARKER-V1`.
- `sim.vbr.marker` returns `SIM-VBR-STAGE-MARKER-V1`.
- `sim.uefi.marker` returns `SIM-UEFI-STAGE-MARKER-V1`.
- `sim.acpi.annotation.marker` returns `SIM-ACPI-ANNOTATION-MARKER-V1`.

The important safety field is `allowed_side_effects: [memory-only-marker]`.
That is exactly what the assembly marker modules do: return a pointer to a
constant string.

## Mock Installer Manifest

`bootkitstudio/manifests/Mock-Installer-Manifest.yaml` is the classroom
replacement for a real installer. It is intentionally non-operational and
requires four evidence-only phases:

- `would_stage`
- `would_hook`
- `would_chain`
- `would_restore`

It also records prohibited actions:

- `disk-write`
- `firmware-write`
- `real-bootmgr-patch`
- `real-winload-patch`
- `persistent-installer`
- `live-acpi-aml-modification`

The runner appends these fields to evidence events so the class can discuss
installer structure without producing an installer.

## Guardrails

`bootkitstudio/config/lab_config.yaml` says:

```yaml
vm_root: lab/vms
allow_extensions: [.img, .qcow2, .vhd, .vmdk]
require_lab_mode: true
required_checks: [snapshot-before, rollback-after, evidence-capture]
```

`bootkitstudio/src/guardrails.cpp` enforces those rules:

- `IsLabModeEnabled()` reads `BKS_LAB_MODE`.
- `ValidateScenarioGuardrails()` fails if lab mode is required but missing.
- It checks that required scenario preconditions exist.
- It rejects `/dev/...` and `\\.\PhysicalDrive...` physical disk paths.
- It resolves the VM disk image and VM root.
- It verifies the disk image stays under the configured VM root.
- It verifies the file extension is allowed.

## CMake Build Wiring

`CMakeLists.txt` declares the project as both C++ and ASM:

```cmake
project(BootKitStudio LANGUAGES CXX ASM)
```

The assembly marker files are built as an object library:

```cmake
add_library(stage_simulators OBJECT
  bootkitstudio/asm/mbr_stage_marker.S
  bootkitstudio/asm/vbr_stage_marker.S
  bootkitstudio/asm/uefi_stage_marker.S
)
```

Then the object files are linked into `bootkitstudio_core`:

```cmake
$<TARGET_OBJECTS:stage_simulators>
```

That is why C++ can declare and call these functions:

```cpp
extern "C" const char* bks_mbr_stage_marker();
extern "C" const char* bks_vbr_stage_marker();
extern "C" const char* bks_uefi_stage_marker();
```

`extern "C"` prevents C++ name mangling so the linker can match the plain
assembly symbol names.

## Assembly Syntax Primer

There are three assembly styles in this repo:

- GNU assembler with C preprocessing: `.S` files under `bootkitstudio/asm`.
- GNU assembler in AT&T syntax: `ChainLoad-PIT-Beep.s`.
- NASM-style 16-bit sketch: `Chain-Loader-SetStack-2.txt` and the sample in
  `ChainLoadVBR.txt`.

Important differences:

- `.S` with uppercase `S` is preprocessed before assembly, so `#ifdef` and
  `#define` work.
- `.s` with lowercase `s` is normally raw assembler input.
- AT&T syntax uses source first, destination second, such as `mov $1, %rax`.
- Intel/NASM syntax usually uses destination first, source second, such as
  `mov ax, 1`.
- On x86_64 Linux, syscall arguments go in `rdi`, `rsi`, `rdx`, `r10`, `r8`,
  and `r9`; the syscall number goes in `rax`; the return value comes back in
  `rax`.
- On x86_64 C calling conventions, pointer return values come back in `rax`.
- On AArch64 C calling conventions, pointer return values come back in `x0`.

Register naming on x86 is layered:

- `rax`: 64-bit register.
- `eax`: low 32 bits of `rax`.
- `ax`: low 16 bits of `rax`.
- `ah`: high 8 bits of `ax`.
- `al`: low 8 bits of `ax`.

## Stage Marker Assembly: Shared Structure

The files `mbr_stage_marker.S`, `vbr_stage_marker.S`, and `uefi_stage_marker.S`
are nearly identical. They differ only in the label names, exported function
names, and marker strings.

The MBR file uses:

- `mbr_marker_string`
- `SIM-MBR-STAGE-MARKER-V1`
- `bks_mbr_stage_marker`

The VBR file uses:

- `vbr_marker_string`
- `SIM-VBR-STAGE-MARKER-V1`
- `bks_vbr_stage_marker`

The UEFI file uses:

- `uefi_marker_string`
- `SIM-UEFI-STAGE-MARKER-V1`
- `bks_uefi_stage_marker`

The ACPI annotation file uses:

- `acpi_annotation_marker_string`
- `SIM-ACPI-ANNOTATION-MARKER-V1`
- `bks_acpi_annotation_marker`

The function contract is simple:

```c
const char* bks_<stage>_stage_marker();
```

It returns a pointer to a constant, null-terminated marker string.

## Stage Marker Line-By-Line

This section uses the MBR file as the example. Replace `mbr` with `vbr` or
`uefi` for the other two files.

```asm
#ifdef __APPLE__
```

If the assembler is running through the C preprocessor on macOS, this branch is
enabled. Apple platforms use Mach-O object files and normally prefix C symbols
with an underscore.

```asm
#define GSYM(x) _##x
```

Defines a macro named `GSYM`. The `##` operator joins tokens together. On Apple,
`GSYM(bks_mbr_stage_marker)` becomes `_bks_mbr_stage_marker`.

```asm
#define CSTRING_SECTION .section __TEXT,__cstring,cstring_literals
```

Defines the object-file section for C string literals on Mach-O.

```asm
#else
```

Starts the non-Apple branch.

```asm
#define GSYM(x) x
```

On ELF-style platforms, C symbols are usually exported without a leading
underscore. The macro leaves the symbol unchanged.

```asm
#define CSTRING_SECTION .section .rodata
```

Puts constant data into the standard read-only data section on ELF platforms.

```asm
#endif
```

Ends the platform-specific preprocessor block.

```asm
CSTRING_SECTION
```

Expands to the correct string-literal section for the current platform.

```asm
mbr_marker_string:
```

Defines a local label. The bytes after this label are the marker string.

```asm
    .asciz "SIM-MBR-STAGE-MARKER-V1"
```

Emits an ASCII string plus a trailing zero byte. The trailing zero makes it a C
string, so C++ can safely treat the returned address as `const char*`.

```asm
.text
```

Switches to the code section.

```asm
.p2align 2
```

Aligns the next code location to `2^2`, or 4 bytes. This is a small performance
and ABI hygiene measure.

```asm
.globl GSYM(bks_mbr_stage_marker)
```

Exports the function symbol so the linker can connect it to the C++ declaration.
On Apple this exports `_bks_mbr_stage_marker`; elsewhere it exports
`bks_mbr_stage_marker`.

```asm
GSYM(bks_mbr_stage_marker):
```

Defines the function label. When C++ calls `bks_mbr_stage_marker()`, execution
starts here.

```asm
#if defined(__x86_64__)
```

Selects the x86_64 implementation.

```asm
    lea mbr_marker_string(%rip), %rax
```

`lea` means load effective address. This does not read the string bytes; it
computes the address of `mbr_marker_string` using RIP-relative addressing and
places that address in `rax`. On x86_64, `rax` is the return-value register for
pointers.

```asm
    ret
```

Returns to the C++ caller. Because `rax` already contains the pointer, the caller
receives the marker string.

```asm
#elif defined(__aarch64__)
```

Selects the 64-bit ARM implementation.

```asm
  #ifdef __APPLE__
```

Uses Apple-specific AArch64 relocation syntax when building Mach-O objects.

```asm
    adrp x0, mbr_marker_string@PAGE
```

Loads the page address containing `mbr_marker_string` into `x0`. `adrp` is used
because AArch64 addresses are commonly formed in two pieces: page base plus
offset within the page.

```asm
    add x0, x0, mbr_marker_string@PAGEOFF
```

Adds the string's offset within that page. `x0` now points to the marker string.
On AArch64, `x0` is the return-value register.

```asm
  #else
```

Uses non-Apple AArch64 relocation syntax.

```asm
    adrp x0, mbr_marker_string
```

Loads the page address for `mbr_marker_string` into `x0` on ELF-style AArch64.

```asm
    add x0, x0, :lo12:mbr_marker_string
```

Adds the low 12-bit page offset for the symbol. `x0` now contains the full
address.

```asm
  #endif
```

Ends the Apple vs non-Apple AArch64 relocation branch.

```asm
    ret
```

Returns to the C++ caller with the pointer in `x0`.

```asm
#else
```

Starts the branch used if neither x86_64 nor AArch64 was detected.

```asm
  #error Unsupported architecture for stage marker assembly.
```

Stops the build with a clear error instead of silently producing a broken object.

```asm
#endif
```

Ends the architecture-specific implementation block.

## How C++ Uses The Marker Assembly

`bootkitstudio/src/simulator_registry.cpp` declares the three assembly functions:

```cpp
extern "C" const char* bks_mbr_stage_marker();
extern "C" const char* bks_vbr_stage_marker();
extern "C" const char* bks_uefi_stage_marker();
```

Then it maps scenario module IDs to those functions:

```cpp
if (module_id == "sim.mbr.marker") {
  return std::string(bks_mbr_stage_marker());
}
```

That call path matters:

```text
Scenario-Matrix.yaml stage_simulators
  -> "sim.mbr.marker"
  -> ResolveStageMarker("sim.mbr.marker")
  -> bks_mbr_stage_marker()
  -> assembly returns "SIM-MBR-STAGE-MARKER-V1"
  -> Runner hashes and records the marker
```

## `ChainLoad-PIT-Beep.s` Purpose

`ChainLoad-PIT-Beep.s` is a Linux x86_64 userspace assembly experiment that uses
raw I/O port access to program the PC speaker through PIT-related ports.

It is not part of the core runner. `Source-Claims.yaml` marks it as
`deprecated/non-viable` with this statement:

```text
Raw port-I/O beep experiments are excluded from core defensive validation scenarios.
```

The file is still useful as an assembly teaching example because it demonstrates:

- Linux syscalls in assembly.
- Register use on x86_64.
- Privileged I/O port instructions.
- Local numeric labels such as `1b` and `2f`.
- Byte, long, and quadword register operations.

## `ChainLoad-PIT-Beep.s` Line-By-Line

```asm
# compile: gcc -o beep.o -c beep.s ; ld -o beep beep.o
```

A comment showing a possible Linux build sequence. It assembles to an object file
and links an executable.

```asm
# run: sudo ./beep, non-root userspace code is not allowed to use port IO
```

A comment explaining that raw port I/O requires elevated privileges. This is one
reason the file is not part of normal defensive simulation.

```asm
    .global _start
```

Exports `_start`, the process entry point used when linking without the normal C
runtime startup code.

```asm
    .text
```

Switches to the executable code section.

```asm
_start:
```

Defines the first instruction address for the process.

```asm
    # ioperm(0x42, 32, 1)
```

Comment: the program will call Linux `ioperm` to request permission for a range
of I/O ports.

```asm
    mov     $173, %rax
```

Places Linux syscall number 173 in `rax`. On x86_64 Linux, syscall numbers go in
`rax`. Here 173 is `ioperm`.

```asm
    mov     $0x42, %rdi
```

Places the first syscall argument in `rdi`: starting I/O port `0x42`.

```asm
    mov     $32, %rsi
```

Places the second syscall argument in `rsi`: the number of ports in the range.

```asm
    mov     $1, %rdx
```

Places the third syscall argument in `rdx`: enable permission.

```asm
    syscall
```

Enters the Linux kernel to perform `ioperm(0x42, 32, 1)`.

```asm
    cmpl    $0, %eax
```

Compares the low 32 bits of the return value with zero. Linux syscalls return
zero or a non-negative value on success, and a negative error value on failure.

```asm
    je      2f
```

Jumps forward to the next local label named `2:` if the compare was equal. In
this file, equal means `ioperm` succeeded.

```asm
    # ioperm() returned error,
    # write(1, message, 13)
```

Comment: the error path writes a message. The `13` in the comment is stale; the
code below writes 37 bytes.

```asm
    mov     $1, %rax
```

Sets syscall number 1, which is `write` on x86_64 Linux.

```asm
    mov     $1, %rdi
```

Sets file descriptor 1, standard output.

```asm
    mov     $message, %rsi
```

Sets the buffer pointer to the address of the `message` label.

```asm
    mov     $37, %rdx
```

Sets the byte count to 37, the length of the message.

```asm
    syscall
```

Calls `write(1, message, 37)`.

```asm
    mov     $1, %rdi
```

Prepares exit status 1 to indicate failure.

```asm
    jmp     _exit
```

Skips the speaker code and jumps to the common exit routine.

```asm
2:  movb    $0xB6, %al
```

Defines local label `2:` and moves byte `0xB6` into `al`. This is the PIT command
byte used to configure channel 2 for square-wave output.

```asm
    outb    %al, $0x43
```

Writes the command byte from `al` to I/O port `0x43`, the PIT command port.

```asm
    # nanosleep to let the IO complete
```

Comment: the following code is not a real `nanosleep` syscall. It is a busy-wait
delay loop.

```asm
    movl    $0x1000, %eax
```

Loads loop counter `0x1000` into `eax`.

```asm
1:  subl    $1, %eax
```

Defines local label `1:` and subtracts 1 from the loop counter.

```asm
    cmpl    $0, %eax
```

Compares the loop counter with zero.

```asm
    jne     1b
```

Jumps backward to the previous `1:` label if the counter is not zero. `1b` means
"local label 1 backward".

```asm
    # set 220Hz, 0x152F == 1193180 / 220
```

Comment: PIT frequency is set through a divisor. `1193180 / 220` is approximately
`0x152F`.

```asm
    movb    $0x2F, %al
```

Loads the low byte of the divisor into `al`.

```asm
    outb    %al, $0x42
```

Writes the low divisor byte to PIT channel 2 data port `0x42`.

```asm
    # nanosleep
```

Comment: another busy-wait delay follows.

```asm
    movl    $0x1000, %eax
1:  subl    $1, %eax
    cmpl    $0, %eax
    jne     1b
```

Repeats the same local-label delay loop. Reusing `1:` is allowed because `1b`
always refers to the nearest previous local label named `1`.

```asm
    movb    $0x15, %al
```

Loads the high byte of the divisor into `al`.

```asm
    outb    %al, $0x42
```

Writes the high divisor byte to PIT channel 2.

```asm
    # nanosleep
    movl    $0x1000, %eax
1:  subl    $1, %eax
    cmpl    $0, %eax
    jne     1b
```

Performs another busy-wait delay.

```asm
    # turn on the speaker
```

Comment: the next instructions modify the speaker control port.

```asm
    inb     $0x61, %al
```

Reads one byte from I/O port `0x61` into `al`. This port controls PC speaker and
timer gate bits on traditional PC hardware.

```asm
    movb    %al, %ah
```

Copies the original control byte into `ah` so the program can restore it later.

```asm
    orb     $0x3, %al
```

Sets bits 0 and 1 in `al`. These bits enable the speaker data path and timer gate.

```asm
    outb    %al, $0x61
```

Writes the modified control byte back to port `0x61`, turning on the speaker.

```asm
    # sleep about 1 sec
```

Comment: this is another busy-wait loop, not a kernel sleep.

```asm
    movl    $0x30000000, %eax
1:  subl    $1, %eax
    cmpl    $0, %eax
    jne     1b
```

Runs a much longer decrement loop to keep the speaker enabled for a noticeable
duration.

```asm
    # turn off thespeaker
```

Comment typo: this means "turn off the speaker".

```asm
    movb    %ah, %al
```

Starts from the original port `0x61` state saved in `ah`.

```asm
    andb    $0xfc, %al
```

Clears bits 0 and 1 by ANDing with binary `11111100`.

```asm
    outb    %al, $0x61
```

Writes the speaker-off state back to port `0x61`.

```asm
    xor     %rdi, %rdi
```

Sets `rdi` to zero efficiently. This prepares exit status 0.

```asm
_exit:
```

Defines the common exit label.

```asm
    mov     $60, %rax
```

Sets syscall number 60, which is `exit` on x86_64 Linux.

```asm
    syscall
```

Exits the process with the status code already in `rdi`.

```asm
message:
```

Defines the label for the error message bytes.

```asm
    .ascii  "ioperm() failed, run as root or sudo\n"
```

Emits the message bytes without a trailing zero. That is fine for `write`,
because `write` receives an explicit byte count.

## `Chain-Loader-SetStack-2.txt` Purpose

This file is a 16-bit real-mode sketch of a boot-stage handoff:

```text
load stage 2 from disk -> place it at 0x9000:0x0000 -> jump to it
```

It is not complete source code. Missing pieces include `LOAD_ADDR`, a Disk
Address Packet, BIOS interrupt call setup, error handling, and the final sector
layout.

## `Chain-Loader-SetStack-2.txt` Line-By-Line

```asm
[org 0x7C00]
```

NASM directive: treat addresses as if the code is loaded at physical address
`0x7C00`, the conventional legacy BIOS boot-sector load address.

```asm
bits 16
```

NASM directive: assemble instructions for 16-bit mode.

```asm
start:
```

Defines the entry label for the sketch.

```asm
    ; Set stack, print message, etc.
```

Comment placeholder. Real boot code normally sets segment registers and stack
state early because BIOS handoff state should not be trusted blindly.

```asm
    ; Load the second stage from disk (hardcoded LBA + sector count)
```

Comment: the goal is to read a larger second-stage program from disk.

```asm
    mov bx, LOAD_ADDR
```

Loads a placeholder segment value into `bx`. In the later far jump, the intended
segment is `0x9000`.

```asm
    mov es, bx
```

Copies the segment value into `es`. BIOS disk reads commonly write to `ES:BX`.

```asm
    xor bx, bx
```

Sets `bx` to zero. Combined with `es`, the target buffer becomes `ES:0000`.

```asm
    mov dl, 0x80
```

Sets BIOS drive number `0x80`, commonly the first hard disk in legacy BIOS
conventions.

```asm
    mov ah, 0x42
```

Selects BIOS interrupt `int 0x13` function `AH=0x42`, extended disk read.

```asm
    ; fill in DAP (Disk Address Packet) with LBA of stage2
```

Comment: extended reads require a Disk Address Packet, often passed through
`DS:SI`. The file does not define that packet.

```asm
    ; call int 0x13
```

Comment: the actual BIOS interrupt call is omitted. A complete program would use
`int 0x13` after setting up the DAP.

```asm
    ; check error, handle or bail
```

Comment: BIOS sets the carry flag on disk-read error. Complete code would branch
on carry and avoid jumping into unread memory.

```asm
    jmp 0x9000:0x0000
```

Far jump to segment `0x9000`, offset `0x0000`. In 16-bit real mode, physical
address is `segment * 16 + offset`, so this targets physical address `0x90000`.

```asm
    ; ...
```

Placeholder for omitted code.

```asm
; Partition table @ 0x1BE, signature 0x55AA @ 0x1FE
```

Comment: in a legacy MBR sector, the partition table begins at byte offset
`0x1BE`, and the boot signature is stored at byte offsets `0x1FE` and `0x1FF`.

```asm
; Fill remaining up to 446 bytes (minus code length), then partition table, then 0x55AA
```

Comment: the classic MBR layout allows 446 bytes for bootstrap code before the
64-byte partition table and 2-byte signature. This is context only; BootKitStudio
does not write such sectors.

## `ChainLoadVBR.txt` Teaching Notes

`ChainLoadVBR.txt` is a historical note about VBR patching and chain-loading.
The useful educational pieces are:

- The VBR is the first sector of a partition in legacy BIOS/MBR boot.
- A VBR is a handoff point between MBR bootstrap code and later OS loader stages.
- NTFS boot is too complex to treat as a tiny 512-byte exercise.
- Backup, rollback, and VM-only testing are mandatory when studying boot sectors.
- The implemented repo avoids real VBR modification and uses simulation markers.

The assembly sample in that file repeats the same broad structure as
`Chain-Loader-SetStack-2.txt`:

```asm
[org 0x7C00]
bits 16
start:
    xor ax, ax
    mov ss, ax
    mov sp, 0x7C00
    mov dl, [boot_drive]
    call read_stage2
    jmp 0x9000:0x0000
times 510-($-$$) db 0
dw 0xAA55
```

Line-by-line:

- `[org 0x7C00]`: Assemble as if loaded at the BIOS boot-sector address.
- `bits 16`: Generate 16-bit instructions.
- `start:`: Entry label.
- `xor ax, ax`: Clear `ax` to zero.
- `mov ss, ax`: Set stack segment to zero.
- `mov sp, 0x7C00`: Set stack pointer near the boot sector load address.
- `mov dl, [boot_drive]`: Load the BIOS drive number from memory.
- `call read_stage2`: Call a missing routine that would read stage-two bytes.
- `jmp 0x9000:0x0000`: Far-jump to the loaded stage-two address.
- `times 510-($-$$) db 0`: Pad the sector to byte offset 510.
- `dw 0xAA55`: Emit the legacy boot signature.

This sample is useful for learning boot-sector structure, but it is not what the
BootKitStudio runner executes.

## Markdown Docs Review

`Playbook.md` is the best high-level mission document. Its strongest design
choice is the explicit defensive scope:

- isolated lab VMs only,
- no exploit development,
- no persistence payloads,
- no production endpoint testing.

`BootKitStudio-Architecture.md` correctly describes the real components:

- C++ orchestration core,
- assembly marker modules,
- manifest and scenario contracts,
- guardrails,
- GUI,
- evidence output.

`IR-Boot-Chain-Runbook.md` is operational rather than technical. It explains how
to respond to drift alerts after evidence exists.

`GUI.md` documents the web control plane. The most important safety detail is
that the dropper and ASM stager are inboxes only. They store files and inventory
metadata; they do not deploy or execute staged binaries.

## Review Observations

The repo's strongest pattern is that risky boot-chain concepts have been
converted into safe contracts:

- Old source claims become normalized entries in `Source-Claims.yaml`.
- Real boot-stage execution becomes marker-only assembly.
- Disk modification becomes VM image hashing.
- Drift becomes a harmless `controlled_drift.flag`.
- Incident handling becomes JSON evidence and an IR runbook.

Important nits and caveats:

- Several Markdown links point at `/Users/premise/Documents/github/ChainLoader`,
  while this workspace is `/Users/premise/Documents/ChainLoader`.
- `ChainLoad-PIT-Beep.s` has comments that call busy-wait loops `nanosleep`; no
  `nanosleep` syscall is used.
- `ChainLoad-PIT-Beep.s` has a stale comment saying `write(..., 13)` while the
  code writes 37 bytes.
- `Chain-Loader-SetStack-2.txt` is pseudocode, not complete boot code.
- `ChainLoadVBR.txt` contains high-risk boot-sector discussion and should remain
  background material, not a build or run guide.

## Build And Run

From the repository root:

```bash
cmake -S . -B build
cmake --build build
BKS_LAB_MODE=1 ./build/bootkitstudio --mode baseline --scenario all
```

Run tests:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Run one scenario:

```bash
BKS_LAB_MODE=1 ./build/bootkitstudio --mode baseline --scenario win10-bios-mbr
```

Run controlled drift in the lab simulator:

```bash
BKS_LAB_MODE=1 ./build/bootkitstudio --mode controlled-drift --scenario all
```

Run recovery validation:

```bash
BKS_LAB_MODE=1 ./build/bootkitstudio --mode recovery-validation --scenario all
```

## Best Mental Model

Think of BootKitStudio as a defensive boot-chain control validator:

```text
Original risky boot-chain ideas
  -> normalized source claims
  -> defensive scenario matrix
  -> lab guardrails
  -> marker-only assembly modules
  -> deterministic evidence JSON
  -> IR and hardening workflow
```

The assembly does not take over boot. It teaches how low-level code can expose a
stage identity to the C++ runner. The C++ runner turns that identity into
evidence for detection, hardening, and incident-response practice.
