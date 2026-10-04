# ChainLoader Defensive Boot-Chain Playbook (BIOS + UEFI)

## Mission
Build and run a defensive-only validation program for boot-chain controls in isolated lab VMs. This playbook explicitly excludes exploit development, persistence payloads, and production endpoint testing.

## Phase 1: Source Normalization
- Source corpus: `*.rtf`, `*.txt`, `*.s` under the repository root.
- Normalized output: [Source-Claims.yaml](/Users/premise/Documents/github/ChainLoader/Source-Claims.yaml).
- Required tags per claim: `architecture`, `hypothesis`, `constraint`, `control`, `risk`.
- Required status per claim: `validated`, `needs-lab-validation`, or `deprecated/non-viable`.
- Acceptance gate:
  - Every claim has one status.
  - Every claim has one claim id.
  - No duplicate claim ids.

## Phase 2: Dual-Path Architecture Map
### Legacy BIOS/MBR Path
`BIOS -> MBR -> VBR -> bootmgr -> winload`

### UEFI Path
`Firmware -> ESP -> bootmgfw.efi -> winload.efi`

### Trust Boundaries and Integrity Checkpoints
- Firmware policy boundary: Secure Boot state and boot-order integrity.
- Loader boundary: boot entry and boot loader artifact integrity.
- OS handoff boundary: early-boot anomaly signaling and attestation handoff.

### Deprecated/Non-Viable Techniques (for defensive scope)
- ACPI/PIT beep-route persistence hypotheses.
- Arbitrary low-memory stashing as durable persistence.
- Any hook-first persistence workflow from offensive contexts.

## Phase 3: Lab Execution Model
### VM Matrix
- `Win10 BIOS/MBR`
- `Win11 UEFI (Secure Boot OFF)`
- `Win11 UEFI (Secure Boot ON)`

### Mandatory Gates
- Snapshot before run.
- Rollback after run.
- Evidence capture required for completion.

### Pass/Fail Inputs
- Baseline integrity hashes captured.
- Stage markers emitted and recorded.
- Expected detection/hardening assertions produced.

## Phase 4: Detection and Hardening Mapping
### Detection Controls
- Unauthorized boot-entry/boot-order drift.
- Boot file integrity drift (BCD/ESP/loader artifacts).
- Secure Boot state drift.
- Early-boot anomaly indicators.

### Hardening Controls
- Secure Boot enforcement policy.
- Restricted boot-configuration change control.
- Integrity baseline policy and drift response.
- Incident triage workflow for boot-chain events.

## Phase 5: Operationalization
### Published Artifacts
- [Playbook.md](/Users/premise/Documents/github/ChainLoader/Playbook.md)
- [Scenario-Matrix.yaml](/Users/premise/Documents/github/ChainLoader/Scenario-Matrix.yaml)
- [IR-Boot-Chain-Runbook.md](/Users/premise/Documents/github/ChainLoader/IR-Boot-Chain-Runbook.md)
- [BootKitStudio-Architecture.md](/Users/premise/Documents/github/ChainLoader/BootKitStudio-Architecture.md)

### Ownership and Cadence
- Detection Engineering: weekly scenario run review.
- Platform Security: monthly hardening policy checkpoint.
- Incident Response: quarterly runbook drill.

### Promotion to Production Policy
- A control graduates from lab to production only after:
  - Reproducible PASS in all relevant VM profiles.
  - False-positive review completed.
  - Ownership and incident handling acceptance documented.
