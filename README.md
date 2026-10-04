# Boot Chain Lab

Pre-OS integrity, taught and demonstrated end to end. Everything that runs before an
operating system does — firmware, boot sector, loader — is where the quietest attacks live.
This repo **measures** that ground instead of weaponizing it, and teaches how it works from
the first instruction up.

## Pages

- **`index.html`** — an interactive walkthrough (the boot chain, the ChainLoader
  snapshot→boot→evidence→rollback cycle, and the 512-byte Forth OS with a live byte inspector).
  Static; works anywhere, including GitHub Pages.
- **`live.html`** — boots the real `10biForthOS` floppy in your browser with the v86 x86
  emulator. Upload `simple.asm` / `hello-world.asm` and watch it compile and run over serial.
  Serve the folder first: `python3 -m http.server`, then open `http://localhost:8000/live.html`.

## What's inside

- **`ChainLoader/`** — defensive boot-chain integrity simulator (BootKitStudio): marker-only,
  guardrailed, QEMU snapshot/rollback, JSON evidence. Build: `cmake -S . -B build && cmake --build build`.
- **`10biForthOS/`** — a Forth OS in a 512-byte boot sector, driven over serial. Build + run:
  `make floppy && ./boot`, then `./send examples/simple.asm`.

## Teaching tracks (next)

1. **BootKit ASM** — how the boot chain and stage markers work, instruction by instruction.
2. **The 10biForth** — the serial compile/execute protocol and the sector, taken apart.
3. **Hello World ASM** — from five lines of assembly to bytes on a screen.

Original lessons, built on the examples in this repo.

## Safety

Defensive by design: no boot-sector or firmware writes, markers not payloads, VM-only, every
run snapshot/rollback-guarded. Learn the offense to build the thing that catches it.
