# ChainLoader Classroom PoC Guide

This guide defines the classroom-safe proof of concept for ChainLoader. The
artifact teaches boot-chain structure, real-mode assembly vocabulary, ACPI
terminology, and defensive evidence collection without building an operational
bootkit, installer, firmware hook, or persistence mechanism.

## Non-Operational Safety Contract

The class PoC must obey these hard rules:

- No disk writes outside existing simulator evidence and drift marker files.
- No firmware writes.
- No real `bootmgr` or `winload` patching.
- No persistent installer.
- No live ACPI or AML modification.
- No MBR, VBR, ESP, BCD, boot-driver, or kernel patch deployment.
- No execution of staged ASM files from the GUI stager.

The implemented runner enforces the classroom posture through:

- `bootkitstudio/manifests/Mock-Installer-Manifest.yaml`
- `bootkitstudio/asm/acpi_annotation_marker.S`
- `sim.acpi.annotation.marker`
- `classroom-acpi-annotation`
- evidence events beginning with `safety_contract=`,
  `mock_installer=`, and `mock_installer_prohibited_action=`

## Mock Installer

The mock installer is a manifest, not a deployer. It models four phases:

```text
would_stage -> would_hook -> would_chain -> would_restore
```

These phases mean:

- `would_stage`: A real installer might prepare bytes or metadata. The class PoC
  only records that this phase was discussed.
- `would_hook`: A real bootkit might intercept a boot-chain handoff. The class
  PoC only records an annotation marker.
- `would_chain`: A real boot-stage program might pass control to the next boot
  link. The class PoC only records the concept.
- `would_restore`: A real workflow might attempt cleanup or rollback. The class
  PoC only records rollback expectations and evidence requirements.

The manifest explicitly prohibits:

- `disk-write`
- `firmware-write`
- `real-bootmgr-patch`
- `real-winload-patch`
- `persistent-installer`
- `live-acpi-aml-modification`

## ACPI Hook Clarification

Do not merge these three topics:

| Topic | Meaning | Classroom handling |
| --- | --- | --- |
| ACPI firmware tables | DSDT, SSDT, FADT, RSDT, XSDT, and AML describe firmware/platform interfaces. | Inventory and terminology only. No table or AML edits. |
| Windows `acpi.sys` | Windows ACPI driver loaded early and positioned near the base of some device stacks. | Baseline as a boot-driver artifact and discuss driver-stack role. |
| Windows SSDT | System Service Dispatch Table used by Windows native system calls. | Explain that this is not the ACPI Secondary System Description Table. No call-table hooks. |

The safe ACPI annotation module is `sim.acpi.annotation.marker`. It returns the
string `SIM-ACPI-ANNOTATION-MARKER-V1` and has no side effects beyond evidence.

## Real-Mode Teaching Map

The real-mode portion of the lesson should teach these concepts:

```text
BIOS handoff
  |
  v
0x0000:0x7C00 boot-sector load address
  |
  +--> segment:offset addressing
  +--> minimal stack setup
  +--> interrupt vector table concept
  +--> disk-read concept as a labeled stub
  |
  v
handoff chain: MBR -> VBR -> bootmgr -> winload
```

Key notes:

- Real mode uses `segment * 16 + offset` addressing.
- `0x7C00` is the conventional legacy BIOS boot-sector load address.
- A stack must be set before relying on calls, pushes, or nested routines.
- The Interrupt Vector Table is a table of real-mode interrupt entry points.
- BIOS disk-read or hook mechanics must remain labeled stubs in the class PoC.

## Detection Mapping

The class PoC maps historical bootkit concepts to defensive observations:

| Concern | Safe evidence or detection |
| --- | --- |
| MBR/VBR drift | Baseline hash and `mbr-vbr-drift-alert` style detection. |
| ESP drift | Baseline hash and `esp-file-drift-alert` style detection. |
| Boot-entry drift | `boot-entry-drift-alert`. |
| `acpi.sys` or boot-driver baseline | `acpi-sys-boot-driver-baseline-alert` and boot-driver inventory. |
| Secure Boot state | `secure-boot-state-change-alert` and hardening status. |
| Rollback evidence | `rollback-evidence-required` and `rollback-evidence-validated`. |
| Staged ASM | Inventory only; no execution or deployment. |

## 2009 BIOS/Vista Assumptions vs Modern Systems

The Rootkit Arsenal is historically useful, but its bootkit sections are framed
around 2009-era assumptions:

- Legacy BIOS and MBR are central.
- Vista-era `bootmgr` and `winload` behavior is the main Windows reference.
- Secure Boot is not treated as a normal default barrier.
- TPM-backed measurements and BitLocker workflows are not the default frame.
- UEFI/GPT/ESP boot is much more common now.
- Modern Windows integrity, driver signing, and platform protections materially
  change the feasibility and detection surface.

For this repo, historical material is only a vocabulary and threat-model source.
The implementation remains marker-only and evidence-only.

## Class Rubric

Students should produce:

- An annotated assembly walkthrough of the marker modules.
- A stage diagram showing BIOS/MBR/VBR and UEFI/ESP paths.
- A real-mode concept map covering registers, stack, `0x7C00`, segment:offset,
  IVT, and handoff chain.
- Evidence JSON from a `baseline` run of `classroom-acpi-annotation`.
- A safety note that repeats the non-operational contract.
- A detection table explaining what would be observed defensively.
- A short paragraph explaining why ACPI firmware SSDT and Windows SSDT are
  different.

Students should not produce:

- A real bootkit installer.
- A persistent loader.
- A patch for `bootmgr`, `winload`, kernel images, boot drivers, MBR, VBR, ESP,
  BCD, firmware, ACPI tables, or AML.
- A working interrupt hook, disk-read hook, or ACPI hook.

## Running The Safe Scenario

```bash
cmake -S . -B build
cmake --build build
BKS_LAB_MODE=1 ./build/bootkitstudio --mode baseline --scenario classroom-acpi-annotation
```

Expected evidence includes:

- `stage=acpi-annotation,marker=SIM-ACPI-ANNOTATION-MARKER-V1`
- `mock_installer=would_stage`
- `mock_installer=would_hook`
- `mock_installer=would_chain`
- `mock_installer=would_restore`
- `mock_installer_prohibited_action=live-acpi-aml-modification`
- `verdict=PASS`

## JSX And Rust Controller

The class-facing controller lives in `classroom-controller/`:

- `classroom-controller/rust`: reads the repo manifests, validates the
  non-operational contract, prints the teaching script, emits JSON for the UI,
  and can run only the safe `baseline` scenario.
- `classroom-controller/jsx`: renders the safety contract, mock phases,
  marker-only ACPI annotation, detection prompts, hardening prompts, and
  evidence preview.

Use the Rust controller to produce a teaching model:

```bash
cargo run --manifest-path classroom-controller/rust/Cargo.toml -- \
  --repo-root . \
  --scenario classroom-acpi-annotation \
  --json
```
