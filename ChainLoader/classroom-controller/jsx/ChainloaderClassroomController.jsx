import React, { useMemo, useState } from "react";

const fallbackModel = {
  scenario: {
    id: "classroom-acpi-annotation",
    platform: "Classroom ACPI annotation lab",
    safeAction:
      "Emit a marker-only ACPI hook annotation and mock installer evidence; no ACPI/AML, firmware, disk, bootmgr, or winload writes.",
    expectedDetection: [
      "acpi-sys-boot-driver-baseline-alert",
      "acpi-firmware-table-inventory-alert",
      "secure-boot-state-change-alert",
      "rollback-evidence-required",
    ],
    expectedHardening: [
      "offline-baseline-comparison",
      "rollback-evidence-validated",
      "no-live-acpi-aml-modification",
      "boot-driver-integrity-baseline",
    ],
  },
  mockInstaller: {
    nonOperational: true,
    phases: ["would_stage", "would_hook", "would_chain", "would_restore"],
    prohibitedActions: [
      "disk-write",
      "firmware-write",
      "real-bootmgr-patch",
      "real-winload-patch",
      "persistent-installer",
      "live-acpi-aml-modification",
    ],
    evidenceContract:
      "Emit classroom evidence only; never deploy, persist, patch boot files, write firmware, or modify ACPI/AML.",
  },
  markers: [
    {
      moduleId: "sim.acpi.annotation.marker",
      bootStage: "acpi-annotation",
      markerContract: "SIM-ACPI-ANNOTATION-MARKER-V1",
      inputContract: "No ACPI/AML writes; marker-only classroom annotation.",
    },
  ],
  runnerStatus: null,
};

const teachingSteps = [
  {
    id: "contract",
    title: "Safety Contract",
    copy:
      "The class starts by proving what this controller refuses to do: no disk writes, firmware writes, persistent installer, live ACPI/AML edits, or boot file patching.",
  },
  {
    id: "stages",
    title: "Mock Installer Phases",
    copy:
      "The would_* phases are discussion checkpoints. They describe where a real installer might act, while this PoC only emits evidence.",
  },
  {
    id: "marker",
    title: "Marker Assembly",
    copy:
      "The assembly module returns a constant marker string. It does not hook, patch, relocate, or deploy anything.",
  },
  {
    id: "acpi",
    title: "ACPI Clarification",
    copy:
      "ACPI firmware SSDT, Windows acpi.sys, and the Windows System Service Dispatch Table are separate concepts. The class should keep them separate.",
  },
  {
    id: "detections",
    title: "Defensive Evidence",
    copy:
      "The output is a defensive evidence story: what would be inventoried, baselined, detected, and rolled back in a lab.",
  },
];

export default function ChainloaderClassroomController({ controllerModel, evidence }) {
  const model = controllerModel ?? fallbackModel;
  const [activeStep, setActiveStep] = useState(teachingSteps[0].id);

  const activeLesson = useMemo(
    () => teachingSteps.find((step) => step.id === activeStep) ?? teachingSteps[0],
    [activeStep],
  );

  const evidencePreview = evidence ?? {
    scenario_id: model.scenario.id,
    verdict: "PASS",
    events: [
      "safety_contract=no_disk_writes,no_firmware_writes,no_real_bootmgr_winload_patching,no_persistent_installer,no_live_acpi_aml_modification",
      ...model.mockInstaller.phases.map((phase) => `mock_installer=${phase}`),
      ...model.markers.map(
        (marker) => `stage=${marker.bootStage},marker=${marker.markerContract}`,
      ),
    ],
  };

  return (
    <main className="chainloader-classroom">
      <header className="controller-header">
        <div>
          <p className="eyebrow">Marker-only ChainLoader controller</p>
          <h1>{model.scenario.id}</h1>
          <p>{model.scenario.platform}</p>
        </div>
        <span className="status-pill">Non-operational</span>
      </header>

      <section className="instruction-panel">
        <h2>What is going on here?</h2>
        <p>{model.scenario.safeAction}</p>
        <div className="step-tabs" role="tablist" aria-label="Teaching steps">
          {teachingSteps.map((step) => (
            <button
              key={step.id}
              type="button"
              className={step.id === activeStep ? "active" : ""}
              onClick={() => setActiveStep(step.id)}
            >
              {step.title}
            </button>
          ))}
        </div>
        <article className="lesson-card">
          <h3>{activeLesson.title}</h3>
          <p>{activeLesson.copy}</p>
        </article>
      </section>

      <section className="flow-grid" aria-label="Mock installer phases">
        {model.mockInstaller.phases.map((phase, index) => (
          <article key={phase} className="phase-card">
            <span>{String(index + 1).padStart(2, "0")}</span>
            <h3>{phase}</h3>
            <p>{phaseExplanation(phase)}</p>
          </article>
        ))}
      </section>

      <section className="two-column">
        <article>
          <h2>Blocked Actions</h2>
          <ul className="check-list">
            {model.mockInstaller.prohibitedActions.map((action) => (
              <li key={action}>{action}</li>
            ))}
          </ul>
        </article>

        <article>
          <h2>Stage Markers</h2>
          <ul className="marker-list">
            {model.markers.map((marker) => (
              <li key={marker.moduleId}>
                <strong>{marker.moduleId}</strong>
                <span>{marker.bootStage}</span>
                <code>{marker.markerContract}</code>
                <p>{marker.inputContract}</p>
              </li>
            ))}
          </ul>
        </article>
      </section>

      <section className="two-column">
        <article>
          <h2>Detection Prompts</h2>
          <ul>
            {model.scenario.expectedDetection.map((detection) => (
              <li key={detection}>{detection}</li>
            ))}
          </ul>
        </article>
        <article>
          <h2>Hardening Prompts</h2>
          <ul>
            {model.scenario.expectedHardening.map((hardening) => (
              <li key={hardening}>{hardening}</li>
            ))}
          </ul>
        </article>
      </section>

      <section className="evidence-panel">
        <h2>Evidence Preview</h2>
        <pre>{JSON.stringify(evidencePreview, null, 2)}</pre>
      </section>
    </main>
  );
}

function phaseExplanation(phase) {
  switch (phase) {
    case "would_stage":
      return "Discusses preparation without writing sectors, files, firmware, or persistent state.";
    case "would_hook":
      return "Labels the conceptual hook point while keeping hook mechanics as non-runnable stubs.";
    case "would_chain":
      return "Shows the boot handoff idea as a diagram and evidence event, not a control transfer.";
    case "would_restore":
      return "Connects rollback and evidence requirements to incident-response practice.";
    default:
      return "Classroom-only phase emitted as evidence.";
  }
}
