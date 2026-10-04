#pragma once

#include <map>
#include <string>
#include <vector>

namespace bootkitstudio {

enum class RunnerMode {
  kBaseline,
  kControlledDrift,
  kRecoveryValidation,
};

struct ScenarioSpec {
  std::string scenario_id;
  std::string platform;
  std::vector<std::string> preconditions;
  std::string safe_action;
  std::vector<std::string> expected_detection;
  std::vector<std::string> expected_hardening;
  std::vector<std::string> evidence_required;
  std::vector<std::string> pass_fail_criteria;

  std::string vm_profile;
  std::string vm_disk_image;
  std::vector<std::string> stage_simulators;
  std::string drift_action;
  std::string rollback_policy;
  std::map<std::string, int> timeouts;
};

struct StageSimulatorManifest {
  std::string module_id;
  std::string boot_stage;
  std::string lang;
  std::string input_contract;
  std::string marker_contract;
  std::vector<std::string> allowed_side_effects;
};

struct MockInstallerManifest {
  bool non_operational = true;
  std::vector<std::string> phases;
  std::vector<std::string> prohibited_actions;
  std::string evidence_contract;
};

struct EvidenceBundle {
  std::string scenario_id;
  std::string timestamp;
  std::string vm_id;
  std::map<std::string, std::string> baseline_hashes;
  std::map<std::string, std::string> delta_hashes;
  std::vector<std::string> events;
  std::vector<std::string> alerts;
  std::string analyst_notes;
  std::string verdict;
};

struct RiskRegisterEntry {
  std::string risk_id;
  std::string boot_stage;
  std::string threat;
  std::string control;
  std::string coverage;
  std::string residual_risk;
  std::string owner;
};

struct GuardrailConfig {
  std::string vm_root;
  std::vector<std::string> allow_extensions;
  bool require_lab_mode = true;
  std::vector<std::string> required_checks;
};

struct ClaimEntry {
  std::string claim_id;
  std::string source;
  std::string status;
  std::vector<std::string> tags;
  std::string statement;
};

std::string RunnerModeToString(RunnerMode mode);
RunnerMode RunnerModeFromString(const std::string& mode);

}  // namespace bootkitstudio
