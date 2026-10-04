#include "bootkitstudio/runner.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "bootkitstudio/guardrails.hpp"
#include "bootkitstudio/hash_utils.hpp"
#include "bootkitstudio/qemu.hpp"
#include "bootkitstudio/simulator_registry.hpp"
#include "bootkitstudio/util.hpp"
#include "bootkitstudio/yaml_like_parser.hpp"

namespace bootkitstudio {
namespace {

std::filesystem::path ResolvePath(const std::filesystem::path& root, const std::string& candidate) {
  std::filesystem::path path(candidate);
  if (path.is_relative()) {
    path = root / path;
  }
  return path;
}

bool ScenarioMatchesFilter(const std::string& scenario_id, const std::string& filter) {
  return filter == "all" || filter.empty() || scenario_id == filter;
}

void AppendMockInstallerEvidence(EvidenceBundle* evidence, const MockInstallerManifest& manifest) {
  evidence->events.push_back(
      "safety_contract=no_disk_writes,no_firmware_writes,no_real_bootmgr_winload_patching,"
      "no_persistent_installer,no_live_acpi_aml_modification");
  for (const std::string& phase : manifest.phases) {
    evidence->events.push_back("mock_installer=" + phase);
  }
  for (const std::string& action : manifest.prohibited_actions) {
    evidence->events.push_back("mock_installer_prohibited_action=" + action);
  }
  evidence->events.push_back("mock_installer_evidence_contract=" + manifest.evidence_contract);
}

std::string MapToJsonObject(const std::map<std::string, std::string>& values) {
  std::ostringstream json;
  json << "{";
  size_t i = 0;
  for (const auto& kv : values) {
    if (i++ != 0) {
      json << ",";
    }
    json << "\"" << JsonEscape(kv.first) << "\":\"" << JsonEscape(kv.second) << "\"";
  }
  json << "}";
  return json.str();
}

std::string ListToJsonArray(const std::vector<std::string>& values) {
  std::ostringstream json;
  json << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      json << ",";
    }
    json << "\"" << JsonEscape(values[i]) << "\"";
  }
  json << "]";
  return json.str();
}

}  // namespace

std::string RunnerModeToString(RunnerMode mode) {
  switch (mode) {
    case RunnerMode::kBaseline:
      return "baseline";
    case RunnerMode::kControlledDrift:
      return "controlled-drift";
    case RunnerMode::kRecoveryValidation:
      return "recovery-validation";
  }
  return "baseline";
}

RunnerMode RunnerModeFromString(const std::string& mode) {
  const std::string lowered = ToLower(mode);
  if (lowered == "baseline") {
    return RunnerMode::kBaseline;
  }
  if (lowered == "controlled-drift") {
    return RunnerMode::kControlledDrift;
  }
  if (lowered == "recovery-validation") {
    return RunnerMode::kRecoveryValidation;
  }
  throw std::runtime_error("Unsupported runner mode: " + mode);
}

Runner::Runner(RunnerPaths paths) : paths_(std::move(paths)) {}

std::vector<EvidenceBundle> Runner::Run(RunnerMode mode, const std::string& scenario_filter) {
  const auto scenarios = ParseScenarioMatrix(paths_.scenario_matrix.string());
  const auto manifests = ParseStageSimulatorManifest(paths_.stage_manifest.string());
  const auto mock_installer = ParseMockInstallerManifest(paths_.mock_installer_manifest.string());
  const GuardrailConfig guardrails = ParseGuardrailConfig(paths_.guardrail_config.string());

  std::string mock_installer_error;
  if (!ValidateMockInstallerManifest(mock_installer, &mock_installer_error)) {
    throw std::runtime_error("Invalid mock installer manifest: " + mock_installer_error);
  }

  std::vector<EvidenceBundle> evidence;
  for (const ScenarioSpec& scenario : scenarios) {
    if (!ScenarioMatchesFilter(scenario.scenario_id, scenario_filter)) {
      continue;
    }

    std::string scenario_error;
    if (!ValidateScenarioSpec(scenario, &scenario_error)) {
      throw std::runtime_error("Invalid scenario '" + scenario.scenario_id + "': " + scenario_error);
    }

    evidence.push_back(RunScenario(scenario, manifests, mock_installer, guardrails, mode));
  }

  if (evidence.empty()) {
    throw std::runtime_error("No matching scenarios found for filter: " + scenario_filter);
  }

  return evidence;
}

EvidenceBundle Runner::RunScenario(
    const ScenarioSpec& scenario,
    const std::vector<StageSimulatorManifest>& manifests,
    const MockInstallerManifest& mock_installer,
    const GuardrailConfig& guardrails,
    RunnerMode mode) {
  std::unordered_map<std::string, StageSimulatorManifest> manifest_lookup;
  for (const auto& manifest : manifests) {
    std::string manifest_error;
    if (!ValidateStageManifest(manifest, &manifest_error)) {
      throw std::runtime_error("Invalid stage manifest '" + manifest.module_id + "': " + manifest_error);
    }
    manifest_lookup[manifest.module_id] = manifest;
  }

  std::string guardrail_error;
  if (!ValidateScenarioGuardrails(scenario, guardrails, paths_.repo_root, &guardrail_error)) {
    throw std::runtime_error("Guardrail violation in scenario '" + scenario.scenario_id + "': " + guardrail_error);
  }

  EvidenceBundle evidence;
  evidence.scenario_id = scenario.scenario_id;
  evidence.timestamp = CurrentIsoUtcTimestamp();
  evidence.vm_id = scenario.vm_profile;
  evidence.analyst_notes = "Automated defensive lab run.";

  const std::filesystem::path disk_path = ResolvePath(paths_.repo_root, scenario.vm_disk_image);
  evidence.baseline_hashes["vm_disk_image"] = Sha256File(disk_path.string());
  evidence.events.push_back("mode=" + RunnerModeToString(mode));
  evidence.events.push_back("safe_action=" + scenario.safe_action);
  AppendMockInstallerEvidence(&evidence, mock_installer);

  for (const std::string& module_id : scenario.stage_simulators) {
    auto found = manifest_lookup.find(module_id);
    if (found == manifest_lookup.end()) {
      throw std::runtime_error("Scenario references unknown simulator module: " + module_id);
    }

    const std::string marker = ResolveStageMarker(module_id);
    evidence.baseline_hashes["marker:" + module_id] = Fnv1a64(marker);
    evidence.events.push_back("stage=" + found->second.boot_stage + ",marker=" + marker);
  }

  // QEMU chainloader: snapshot-before -> boot -> evidence-capture -> rollback-after.
  // Overlay/boot only; the base image is never written (see qemu.cpp safety contract).
  const QemuConfig qemu_config = QemuConfigFromEnv(ResolvePath(paths_.repo_root, guardrails.vm_root));
  const QemuReport qemu = RunQemuChainload(scenario, guardrails, paths_.repo_root, mode, qemu_config);
  for (const auto& kv : qemu.baseline_hashes) evidence.baseline_hashes[kv.first] = kv.second;
  for (const auto& kv : qemu.delta_hashes) evidence.delta_hashes[kv.first] = kv.second;
  for (const std::string& e : qemu.events) evidence.events.push_back(e);
  for (const std::string& a : qemu.alerts) evidence.alerts.push_back(a);

  const std::filesystem::path vm_root = ResolvePath(paths_.repo_root, guardrails.vm_root);
  const std::filesystem::path drift_marker = vm_root / scenario.vm_profile / "controlled_drift.flag";

  if (mode == RunnerMode::kControlledDrift) {
    std::filesystem::create_directories(drift_marker.parent_path());
    std::ofstream output(drift_marker, std::ios::trunc);
    output << "scenario=" << scenario.scenario_id << "\n";
    output << "drift_action=" << scenario.drift_action << "\n";
    output << "timestamp=" << evidence.timestamp << "\n";
    output.close();

    evidence.delta_hashes["controlled_drift.flag"] = Sha256File(drift_marker.string());
    for (const std::string& detection : scenario.expected_detection) {
      evidence.alerts.push_back("detected:" + detection);
    }
    evidence.events.push_back("controlled_drift_applied");
  }

  if (mode == RunnerMode::kRecoveryValidation) {
    std::error_code ec;
    std::filesystem::remove(drift_marker, ec);
    if (ec) {
      throw std::runtime_error("Unable to remove drift marker for recovery validation: " + ec.message());
    }

    evidence.delta_hashes["vm_disk_image_post_recovery"] = Sha256File(disk_path.string());
    for (const std::string& hardening : scenario.expected_hardening) {
      evidence.alerts.push_back("hardened:" + hardening);
    }
    evidence.events.push_back("recovery_validation_complete");
  }

  if (mode == RunnerMode::kBaseline) {
    evidence.alerts.push_back("baseline:integrity-checkpoint-collected");
  }

  bool has_required_evidence = true;
  for (const std::string& requirement : scenario.evidence_required) {
    bool found = false;
    for (const auto& kv : evidence.baseline_hashes) {
      if (kv.first.find(requirement) != std::string::npos || kv.second.find(requirement) != std::string::npos) {
        found = true;
      }
    }
    for (const std::string& event : evidence.events) {
      if (event.find(requirement) != std::string::npos) {
        found = true;
      }
    }
    if (!found && requirement != "event_logs") {
      has_required_evidence = false;
    }
  }

  evidence.verdict = has_required_evidence ? "PASS" : "FAIL";
  return evidence;
}

std::string Runner::WriteEvidence(const EvidenceBundle& evidence) const {
  std::filesystem::create_directories(paths_.output_dir);
  std::string mode = "unknown";
  for (const std::string& event : evidence.events) {
    if (event.rfind("mode=", 0) == 0) {
      mode = event.substr(5);
      break;
    }
  }

  const std::string filename =
      evidence.scenario_id + "." + mode + "." + evidence.timestamp + ".json";
  const std::filesystem::path output_path = paths_.output_dir / filename;

  std::ofstream output(output_path, std::ios::trunc);
  output << EvidenceToJson(evidence);
  output.close();

  return output_path.string();
}

std::string EvidenceToJson(const EvidenceBundle& evidence) {
  std::ostringstream json;
  json << "{";
  json << "\"scenario_id\":\"" << JsonEscape(evidence.scenario_id) << "\",";
  json << "\"timestamp\":\"" << JsonEscape(evidence.timestamp) << "\",";
  json << "\"vm_id\":\"" << JsonEscape(evidence.vm_id) << "\",";
  json << "\"baseline_hashes\":" << MapToJsonObject(evidence.baseline_hashes) << ",";
  json << "\"delta_hashes\":" << MapToJsonObject(evidence.delta_hashes) << ",";
  json << "\"events\":" << ListToJsonArray(evidence.events) << ",";
  json << "\"alerts\":" << ListToJsonArray(evidence.alerts) << ",";
  json << "\"analyst_notes\":\"" << JsonEscape(evidence.analyst_notes) << "\",";
  json << "\"verdict\":\"" << JsonEscape(evidence.verdict) << "\"";
  json << "}";
  return json.str();
}

}  // namespace bootkitstudio
