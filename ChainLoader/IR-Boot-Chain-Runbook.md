# IR Boot-Chain Runbook

## Purpose
Provide a repeatable triage path for boot-chain drift alerts validated through BootKitStudio lab scenarios.

## Trigger Conditions
- Boot-entry drift alert.
- BCD/ESP/loader integrity drift alert.
- Secure Boot state-change alert.
- Early-boot anomaly alert.

## Triage Workflow
1. Validate alert metadata and scenario provenance.
2. Confirm evidence bundle exists in `output/evidence/`.
3. Verify `verdict`, baseline hashes, and delta hashes.
4. Review alert-to-control mapping in [Scenario-Matrix.yaml](/Users/premise/Documents/github/ChainLoader/Scenario-Matrix.yaml).
5. Determine severity based on environment class (lab, staging, production).

## Containment Workflow
1. Freeze boot configuration changes.
2. Restore known-good boot state from approved baseline.
3. Re-run matching BootKitStudio scenario in lab for reproducibility.
4. Compare observed alerts with expected detection controls.

## Recovery Workflow
1. Re-enable hardening controls (Secure Boot, integrity baselines, change-control policy).
2. Execute `recovery-validation` scenario mode.
3. Confirm PASS verdict and archive evidence in incident record.

## Closure Criteria
- Root cause documented.
- Detection and hardening mappings updated.
- False-positive or true-positive classification recorded.
- Owner sign-off from Detection Engineering and Incident Response.
