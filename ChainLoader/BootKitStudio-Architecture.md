# BootKitStudio Architecture (Defensive C++ + Assembly)

## Design Goals
- Deterministic boot-stage simulation for control validation.
- Strict guardrails that fail closed outside approved lab conditions.
- Machine-readable evidence for SIEM and IR workflows.

## Components
### 1) C++ Orchestration Core
- CLI entrypoint: `bootkitstudio/src/main.cpp`
- Scenario and manifest parsing: `bootkitstudio/src/yaml_like_parser.cpp`
- Guardrails: `bootkitstudio/src/guardrails.cpp`
- Evidence runner: `bootkitstudio/src/runner.cpp`
- Hashing utilities: `bootkitstudio/src/hash_utils.cpp`

### 2) Assembly Stage Simulators
- MBR marker module: `bootkitstudio/asm/mbr_stage_marker.S`
- VBR marker module: `bootkitstudio/asm/vbr_stage_marker.S`
- UEFI marker module: `bootkitstudio/asm/uefi_stage_marker.S`
- Registry bridge: `bootkitstudio/src/simulator_registry.cpp`

### 3) Runtime Contracts
- Scenario matrix: [Scenario-Matrix.yaml](/Users/premise/Documents/github/ChainLoader/Scenario-Matrix.yaml)
- Stage simulator manifest: `bootkitstudio/manifests/Stage-Simulator-Manifest.yaml`
- Lab guardrails config: `bootkitstudio/config/lab_config.yaml`
- Evidence schema: `bootkitstudio/schemas/evidence_bundle.schema.json`

### 4) GUI Control Plane (Defensive)
- Web server: `gui/server.py`
- UI assets: `gui/static/index.html`, `gui/static/app.js`, `gui/static/styles.css`
- Launcher: `gui/run_gui.sh`
- BootKit Dropper inbox: `gui/dropzone/inbox/` (defensive artifact intake only)
- ASM Binary Stager inbox: `gui/dropzone/asm-staged/` (staging/inventory only)

## Guardrail Model
- Requires `BKS_LAB_MODE=1`.
- Blocks physical disk paths (`/dev/*`, `\\.\\PhysicalDrive*`).
- Allows only VM disk images under configured `vm_root`.
- Requires scenario preconditions: `snapshot-before`, `rollback-after`, `evidence-capture`.

## Runner Modes
- `baseline`: collect integrity and marker evidence only.
- `controlled-drift`: apply safe drift marker file for detection validation.
- `recovery-validation`: remove drift marker and validate hardening assertions.

## Evidence Output
Evidence bundle JSON fields:
- `scenario_id`
- `timestamp`
- `vm_id`
- `baseline_hashes`
- `delta_hashes`
- `events`
- `alerts`
- `analyst_notes`
- `verdict`

Output path:
- `output/evidence/<scenario>.<timestamp>.json`

## Build and Run
```bash
cmake -S . -B build
cmake --build build
BKS_LAB_MODE=1 ./build/bootkitstudio --mode baseline --scenario all
```

## QEMU Chainloader (snapshot → boot → evidence → rollback)
Implemented in `bootkitstudio/src/qemu.cpp` (`bootkitstudio/include/bootkitstudio/qemu.hpp`),
called from `runner.cpp` per scenario. It turns the declared preconditions
(`snapshot-before`, `rollback-after`, `evidence-capture`) into real QEMU operations:

1. **snapshot-before** — `qemu-img create` makes a qcow2 **overlay** whose backing file is the
   base image. The base is opened read-only as a backing store and is never a write target.
2. **boot** — `qemu-system` boots the **overlay only**, headless, with `-nic none` (no guest
   network), `-no-reboot`, and the scenario's `boot_seconds` as a hard wall-clock timeout
   (clamped by `BKS_QEMU_MAX_BOOT`). A timeout is expected — the lab measures early boot, it
   does not complete a Windows boot. Serial output is captured to a log and hashed.
3. **evidence-capture** — overlay hash, serial-log hash, tool versions, Secure Boot posture,
   and the boot result are added to the evidence bundle (`qemu_*` events, `qemu:*` hashes).
4. **rollback-after** — the overlay and any per-run OVMF vars copy are deleted; the base image
   is re-hashed and `rollback_confirmed` asserts it is byte-identical to before the run.

**Safety contract (enforced in code):** never writes the base image; refuses physical-disk
paths (`/dev/*`, `\\.\PhysicalDrive*`); overlays live only under `vm_root`; requires
`BKS_LAB_MODE=1`. When QEMU is absent or the disk image is a placeholder, the step runs in
**simulated** mode and records that it did — it never fabricates a boot.

### Environment
| Variable | Meaning | Default |
|---|---|---|
| `BKS_QEMU_IMG` | `qemu-img` path | `qemu-img` on PATH |
| `BKS_QEMU_SYSTEM` | `qemu-system-*` path | `qemu-system-x86_64` |
| `BKS_OVMF_CODE` | OVMF firmware (UEFI scenarios); empty disables real UEFI boot | unset |
| `BKS_OVMF_VARS_SECBOOT` / `BKS_OVMF_VARS_NOSECBOOT` | OVMF vars templates per Secure Boot state | unset |
| `BKS_QEMU_FORCE_SIMULATE=1` | force simulated mode even if QEMU is present | off |
| `BKS_QEMU_MIN_IMAGE_BYTES` | below this, treat the image as a placeholder → simulate | 1048576 |
| `BKS_QEMU_MAX_BOOT` | upper clamp on the per-scenario boot timeout (seconds) | 180 |

With the repo's placeholder VM images this runs in simulated mode; drop a real VM image
(≥ `BKS_QEMU_MIN_IMAGE_BYTES`) under `lab/vms/<profile>/` and install QEMU to get a real
snapshot/boot/rollback cycle. Overlays, vars copies and serial logs are written under
`lab/vms/.qemu-work/` (git-ignored).
