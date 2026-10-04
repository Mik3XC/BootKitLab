const modeSelect = document.getElementById("mode");
const scenarioSelect = document.getElementById("scenario");
const runBtn = document.getElementById("runBtn");
const refreshScenariosBtn = document.getElementById("refreshScenariosBtn");
const runOutput = document.getElementById("runOutput");

const fileInput = document.getElementById("fileInput");
const uploadBtn = document.getElementById("uploadBtn");
const dropZone = document.getElementById("dropZone");
const dropOutput = document.getElementById("dropOutput");

const asmFileInput = document.getElementById("asmFileInput");
const asmUploadBtn = document.getElementById("asmUploadBtn");
const asmRefreshBtn = document.getElementById("asmRefreshBtn");
const asmDropZone = document.getElementById("asmDropZone");
const asmList = document.getElementById("asmList");
const asmOutput = document.getElementById("asmOutput");

const refreshEvidenceBtn = document.getElementById("refreshEvidenceBtn");
const evidenceList = document.getElementById("evidenceList");
const evidenceContent = document.getElementById("evidenceContent");

let droppedFile = null;
let droppedAsmFile = null;

function print(el, payload) {
  el.textContent = typeof payload === "string" ? payload : JSON.stringify(payload, null, 2);
}

async function loadScenarios() {
  const res = await fetch("/api/scenarios");
  const data = await res.json();
  scenarioSelect.innerHTML = "";

  const allOpt = document.createElement("option");
  allOpt.value = "all";
  allOpt.textContent = "all scenarios";
  scenarioSelect.appendChild(allOpt);

  (data.scenarios || []).forEach((scenario) => {
    const option = document.createElement("option");
    option.value = scenario.scenario_id;
    option.textContent = `${scenario.scenario_id} (${scenario.platform})`;
    scenarioSelect.appendChild(option);
  });
}

async function runScenario() {
  runBtn.disabled = true;
  print(runOutput, "Running...");

  try {
    const res = await fetch("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        mode: modeSelect.value,
        scenario: scenarioSelect.value,
      }),
    });

    const data = await res.json();
    print(runOutput, data);
    await loadEvidence();
  } catch (err) {
    print(runOutput, `Run error: ${err}`);
  } finally {
    runBtn.disabled = false;
  }
}

function acceptDroppedFile(file) {
  droppedFile = file;
  dropZone.innerHTML = `Selected: <strong>${file.name}</strong><br /><span>${file.size} bytes</span>`;
}

function acceptAsmFile(file) {
  droppedAsmFile = file;
  asmDropZone.innerHTML = `Selected: <strong>${file.name}</strong><br /><span>${file.size} bytes</span>`;
}

async function uploadDroppedFile() {
  const file = droppedFile || fileInput.files[0];
  if (!file) {
    print(dropOutput, "Choose or drop a file first.");
    return;
  }

  const text = await file.text();

  const res = await fetch("/api/drop", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ filename: file.name, content: text }),
  });

  const data = await res.json();
  print(dropOutput, data);
}

function bytesToBase64(bytes) {
  const chunkSize = 0x8000;
  let binary = "";
  for (let i = 0; i < bytes.length; i += chunkSize) {
    binary += String.fromCharCode(...bytes.subarray(i, i + chunkSize));
  }
  return btoa(binary);
}

async function stageAsmFile() {
  const file = droppedAsmFile || asmFileInput.files[0];
  if (!file) {
    print(asmOutput, "Choose or drop an ASM module first.");
    return;
  }

  const bytes = new Uint8Array(await file.arrayBuffer());
  const contentB64 = bytesToBase64(bytes);

  const res = await fetch("/api/asm-stage", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      filename: file.name,
      content_b64: contentB64,
      note: "staged via gui",
    }),
  });

  const data = await res.json();
  print(asmOutput, data);
  await loadAsmStaged();
}

async function loadAsmStaged() {
  const res = await fetch("/api/asm-staged");
  const data = await res.json();
  asmList.innerHTML = "";

  (data.items || []).forEach((item) => {
    const li = document.createElement("li");
    li.textContent = `${item.name} (${item.size} bytes)`;
    asmList.appendChild(li);
  });
}

async function loadEvidence() {
  const res = await fetch("/api/evidence");
  const data = await res.json();
  evidenceList.innerHTML = "";

  (data.items || []).forEach((item) => {
    const li = document.createElement("li");
    const btn = document.createElement("button");
    btn.textContent = item.name;
    btn.addEventListener("click", () => loadEvidenceFile(item.name));
    li.appendChild(btn);
    evidenceList.appendChild(li);
  });
}

async function loadEvidenceFile(fileName) {
  const res = await fetch(`/api/evidence?file=${encodeURIComponent(fileName)}`);
  const data = await res.json();
  print(evidenceContent, data);
}

runBtn.addEventListener("click", runScenario);
refreshScenariosBtn.addEventListener("click", loadScenarios);
uploadBtn.addEventListener("click", uploadDroppedFile);
asmUploadBtn.addEventListener("click", stageAsmFile);
asmRefreshBtn.addEventListener("click", loadAsmStaged);
refreshEvidenceBtn.addEventListener("click", loadEvidence);

fileInput.addEventListener("change", () => {
  if (fileInput.files[0]) acceptDroppedFile(fileInput.files[0]);
});

asmFileInput.addEventListener("change", () => {
  if (asmFileInput.files[0]) acceptAsmFile(asmFileInput.files[0]);
});

["dragenter", "dragover"].forEach((eventName) => {
  dropZone.addEventListener(eventName, (event) => {
    event.preventDefault();
    dropZone.classList.add("dragging");
  });
});

["dragleave", "drop"].forEach((eventName) => {
  dropZone.addEventListener(eventName, (event) => {
    event.preventDefault();
    dropZone.classList.remove("dragging");
  });
});

dropZone.addEventListener("drop", (event) => {
  if (event.dataTransfer.files[0]) acceptDroppedFile(event.dataTransfer.files[0]);
});

["dragenter", "dragover"].forEach((eventName) => {
  asmDropZone.addEventListener(eventName, (event) => {
    event.preventDefault();
    asmDropZone.classList.add("dragging");
  });
});

["dragleave", "drop"].forEach((eventName) => {
  asmDropZone.addEventListener(eventName, (event) => {
    event.preventDefault();
    asmDropZone.classList.remove("dragging");
  });
});

asmDropZone.addEventListener("drop", (event) => {
  if (event.dataTransfer.files[0]) acceptAsmFile(event.dataTransfer.files[0]);
});

(async () => {
  await loadScenarios();
  await loadAsmStaged();
  await loadEvidence();
})();
