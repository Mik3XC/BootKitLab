const lessons = {
  contract: {
    eyebrow: "Step 01",
    title: "Safety Contract",
    body:
      "Start by proving what this class artifact refuses to do: no disk writes, firmware writes, persistent installer, live ACPI/AML edits, boot file patches, driver patches, or staged ASM execution.",
  },
  mock: {
    eyebrow: "Step 02",
    title: "Mock Installer",
    body:
      "The would_* phases are teaching checkpoints. They show where a real installer might act, while this classroom controller only records evidence and blocked actions.",
  },
  acpi: {
    eyebrow: "Step 03",
    title: "ACPI Clarification",
    body:
      "Keep ACPI firmware tables, Windows acpi.sys, and the Windows System Service Dispatch Table separate. The ACPI annotation marker labels the concept without modifying ACPI or AML.",
  },
  realmode: {
    eyebrow: "Step 04",
    title: "Real Mode Map",
    body:
      "Use the diagram to teach 0x7C00, segment:offset addressing, stack setup, the interrupt vector table concept, and the MBR to VBR to bootmgr to winload handoff. Disk-read and hook mechanics stay as labeled stubs.",
  },
  detect: {
    eyebrow: "Step 05",
    title: "Detection Mapping",
    body:
      "Every risky historical idea maps to a defensive observation: offline baseline comparison, MBR/VBR/ESP drift checks, Secure Boot state, boot-driver inventory, and rollback evidence.",
  },
};

const demoEvidence = {
  scenario_id: "classroom-acpi-annotation",
  timestamp: "demo",
  vm_id: "win11-uefi-sb-on",
  baseline_hashes: {
    "marker:sim.acpi.annotation.marker": "fnv1a64:2a36f3ae9d830bd1",
    vm_disk_image: "sha256:demo",
  },
  delta_hashes: {},
  events: [
    "mode=baseline",
    "safe_action=Emit a marker-only ACPI hook annotation and mock installer evidence; no ACPI/AML, firmware, disk, bootmgr, or winload writes.",
    "safety_contract=no_disk_writes,no_firmware_writes,no_real_bootmgr_winload_patching,no_persistent_installer,no_live_acpi_aml_modification",
    "mock_installer=would_stage",
    "mock_installer=would_hook",
    "mock_installer=would_chain",
    "mock_installer=would_restore",
    "mock_installer_prohibited_action=disk-write",
    "mock_installer_prohibited_action=firmware-write",
    "mock_installer_prohibited_action=real-bootmgr-patch",
    "mock_installer_prohibited_action=real-winload-patch",
    "mock_installer_prohibited_action=persistent-installer",
    "mock_installer_prohibited_action=live-acpi-aml-modification",
    "stage=acpi-annotation,marker=SIM-ACPI-ANNOTATION-MARKER-V1",
  ],
  alerts: ["baseline:integrity-checkpoint-collected"],
  analyst_notes: "Automated defensive lab run.",
  verdict: "PASS",
};

const lessonEyebrow = document.getElementById("lessonEyebrow");
const lessonTitle = document.getElementById("lessonTitle");
const lessonBody = document.getElementById("lessonBody");
const evidenceJson = document.getElementById("evidenceJson");
const evidenceStatus = document.getElementById("evidenceStatus");
const blockedActions = document.getElementById("blockedActions");
const markerEvidence = document.getElementById("markerEvidence");
const summary = document.getElementById("summary");
const evidencePath = document.getElementById("evidencePath");

document.querySelectorAll("[data-step]").forEach((button) => {
  button.addEventListener("click", () => {
    document.querySelectorAll("[data-step]").forEach((item) => item.classList.remove("active"));
    button.classList.add("active");
    renderLesson(button.dataset.step);
  });
});

document.getElementById("loadEvidence").addEventListener("click", async () => {
  try {
    const response = await fetch(evidencePath.value, { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const evidence = await response.json();
    renderEvidence(evidence, `Loaded ${evidencePath.value}`);
  } catch (error) {
    evidenceStatus.textContent = `Load failed: ${error.message}`;
  }
});

document.getElementById("resetEvidence").addEventListener("click", () => {
  renderEvidence(demoEvidence, "Demo data");
});

function renderLesson(id) {
  const lesson = lessons[id] || lessons.contract;
  lessonEyebrow.textContent = lesson.eyebrow;
  lessonTitle.textContent = lesson.title;
  lessonBody.textContent = lesson.body;
}

function renderEvidence(evidence, statusText) {
  evidenceStatus.textContent = statusText;
  evidenceJson.textContent = JSON.stringify(evidence, null, 2);
  renderBlockedActions(evidence.events || []);
  renderMarkers(evidence);
  renderSummary(evidence);
}

function renderBlockedActions(events) {
  const blocked = events
    .filter((event) => event.startsWith("mock_installer_prohibited_action="))
    .map((event) => event.replace("mock_installer_prohibited_action=", ""));
  blockedActions.replaceChildren(...blocked.map((action) => listItem(action)));
}

function renderMarkers(evidence) {
  const markers = Object.entries(evidence.baseline_hashes || {}).filter(([key]) =>
    key.startsWith("marker:"),
  );
  const nodes = markers.flatMap(([key, value]) => {
    const dt = document.createElement("dt");
    dt.textContent = key;
    const dd = document.createElement("dd");
    dd.textContent = value;
    return [dt, dd];
  });
  markerEvidence.replaceChildren(...nodes);
}

function renderSummary(evidence) {
  const phases = (evidence.events || []).filter((event) => event.startsWith("mock_installer="));
  const stage = (evidence.events || []).find((event) => event.startsWith("stage="));
  summary.innerHTML = "";
  summary.append(
    textBlock(`Scenario: ${evidence.scenario_id || "unknown"}`),
    textBlock(`Verdict: ${evidence.verdict || "unknown"}`),
    textBlock(`Mock phases: ${phases.length}`),
    textBlock(`Marker: ${stage || "not loaded"}`),
  );
}

function listItem(text) {
  const li = document.createElement("li");
  li.textContent = text;
  return li;
}

function textBlock(text) {
  const div = document.createElement("div");
  div.textContent = text;
  return div;
}

renderLesson("contract");
renderEvidence(demoEvidence, "Demo data");
