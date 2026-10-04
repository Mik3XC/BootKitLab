#include "bootkitstudio/yaml_like_parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "bootkitstudio/util.hpp"

namespace bootkitstudio {
namespace {

std::string StripComment(const std::string& line) {
  const size_t comment = line.find('#');
  if (comment == std::string::npos) {
    return line;
  }
  return line.substr(0, comment);
}

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::string Unquote(const std::string& value) {
  if (value.size() >= 2) {
    if ((value.front() == '"' && value.back() == '"') ||
        (value.front() == '\'' && value.back() == '\'')) {
      return value.substr(1, value.size() - 2);
    }
  }
  return value;
}

std::vector<std::string> ParseInlineList(std::string value) {
  value = Trim(value);
  if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
    value = value.substr(1, value.size() - 2);
  }
  std::vector<std::string> output;
  for (const std::string& token : Split(value, ',')) {
    if (!token.empty()) {
      output.push_back(Unquote(Trim(token)));
    }
  }
  return output;
}

std::map<std::string, int> ParseInlineIntMap(std::string value) {
  value = Trim(value);
  if (value.size() >= 2 && value.front() == '{' && value.back() == '}') {
    value = value.substr(1, value.size() - 2);
  }

  std::map<std::string, int> output;
  for (const std::string& pair : Split(value, ',')) {
    const size_t sep = pair.find(':');
    if (sep == std::string::npos) {
      continue;
    }
    const std::string key = Unquote(Trim(pair.substr(0, sep)));
    const std::string val = Unquote(Trim(pair.substr(sep + 1)));
    output[key] = std::stoi(val);
  }
  return output;
}

std::pair<std::string, std::string> ParseKeyValue(const std::string& line) {
  const size_t sep = line.find(':');
  if (sep == std::string::npos) {
    return {"", ""};
  }
  std::string key = Trim(line.substr(0, sep));
  std::string value = Trim(line.substr(sep + 1));
  return {key, value};
}

bool ContainsString(const std::vector<std::string>& values, const std::string& expected) {
  for (const std::string& value : values) {
    if (ToLower(value) == ToLower(expected)) {
      return true;
    }
  }
  return false;
}

void AssignScenarioField(ScenarioSpec* scenario, const std::string& key, const std::string& value) {
  if (key == "id") {
    scenario->scenario_id = Unquote(value);
  } else if (key == "platform") {
    scenario->platform = Unquote(value);
  } else if (key == "preconditions") {
    scenario->preconditions = ParseInlineList(value);
  } else if (key == "safe_action") {
    scenario->safe_action = Unquote(value);
  } else if (key == "expected_detection") {
    scenario->expected_detection = ParseInlineList(value);
  } else if (key == "expected_hardening") {
    scenario->expected_hardening = ParseInlineList(value);
  } else if (key == "evidence_required") {
    scenario->evidence_required = ParseInlineList(value);
  } else if (key == "pass_fail_criteria") {
    scenario->pass_fail_criteria = ParseInlineList(value);
  } else if (key == "vm_profile") {
    scenario->vm_profile = Unquote(value);
  } else if (key == "vm_disk_image") {
    scenario->vm_disk_image = Unquote(value);
  } else if (key == "stage_simulators") {
    scenario->stage_simulators = ParseInlineList(value);
  } else if (key == "drift_action") {
    scenario->drift_action = Unquote(value);
  } else if (key == "rollback_policy") {
    scenario->rollback_policy = Unquote(value);
  } else if (key == "timeouts") {
    scenario->timeouts = ParseInlineIntMap(value);
  }
}

void AssignManifestField(StageSimulatorManifest* manifest, const std::string& key, const std::string& value) {
  if (key == "module_id") {
    manifest->module_id = Unquote(value);
  } else if (key == "boot_stage") {
    manifest->boot_stage = Unquote(value);
  } else if (key == "lang") {
    manifest->lang = Unquote(value);
  } else if (key == "input_contract") {
    manifest->input_contract = Unquote(value);
  } else if (key == "marker_contract") {
    manifest->marker_contract = Unquote(value);
  } else if (key == "allowed_side_effects") {
    manifest->allowed_side_effects = ParseInlineList(value);
  }
}

void AssignMockInstallerField(MockInstallerManifest* manifest, const std::string& key, const std::string& value) {
  if (key == "non_operational") {
    manifest->non_operational = ToLower(Unquote(value)) == "true";
  } else if (key == "phases") {
    manifest->phases = ParseInlineList(value);
  } else if (key == "prohibited_actions") {
    manifest->prohibited_actions = ParseInlineList(value);
  } else if (key == "evidence_contract") {
    manifest->evidence_contract = Unquote(value);
  }
}

std::vector<std::string> ReadCleanLines(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("Unable to open file: " + path);
  }

  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) {
    const std::string stripped = Trim(StripComment(line));
    if (!stripped.empty()) {
      lines.push_back(stripped);
    }
  }
  return lines;
}

}  // namespace

std::vector<ScenarioSpec> ParseScenarioMatrix(const std::string& path) {
  const std::vector<std::string> lines = ReadCleanLines(path);
  std::vector<ScenarioSpec> scenarios;

  bool in_scenarios = false;
  bool has_current = false;
  ScenarioSpec current;

  for (const std::string& line : lines) {
    if (line == "scenarios:") {
      in_scenarios = true;
      continue;
    }
    if (!in_scenarios) {
      continue;
    }

    if (StartsWith(line, "- ")) {
      if (has_current) {
        scenarios.push_back(current);
      }
      current = ScenarioSpec{};
      has_current = true;
      const auto kv = ParseKeyValue(Trim(line.substr(2)));
      AssignScenarioField(&current, kv.first, kv.second);
      continue;
    }

    const auto kv = ParseKeyValue(line);
    if (!kv.first.empty() && has_current) {
      AssignScenarioField(&current, kv.first, kv.second);
    }
  }

  if (has_current) {
    scenarios.push_back(current);
  }

  return scenarios;
}

std::vector<StageSimulatorManifest> ParseStageSimulatorManifest(const std::string& path) {
  const std::vector<std::string> lines = ReadCleanLines(path);
  std::vector<StageSimulatorManifest> manifests;

  bool in_manifests = false;
  bool has_current = false;
  StageSimulatorManifest current;

  for (const std::string& line : lines) {
    if (line == "manifests:") {
      in_manifests = true;
      continue;
    }
    if (!in_manifests) {
      continue;
    }

    if (StartsWith(line, "- ")) {
      if (has_current) {
        manifests.push_back(current);
      }
      current = StageSimulatorManifest{};
      has_current = true;
      const auto kv = ParseKeyValue(Trim(line.substr(2)));
      AssignManifestField(&current, kv.first, kv.second);
      continue;
    }

    const auto kv = ParseKeyValue(line);
    if (!kv.first.empty() && has_current) {
      AssignManifestField(&current, kv.first, kv.second);
    }
  }

  if (has_current) {
    manifests.push_back(current);
  }

  return manifests;
}

MockInstallerManifest ParseMockInstallerManifest(const std::string& path) {
  const std::vector<std::string> lines = ReadCleanLines(path);
  MockInstallerManifest manifest;
  bool in_mock_installer = false;

  for (const std::string& line : lines) {
    if (line == "mock_installer:") {
      in_mock_installer = true;
      continue;
    }
    if (!in_mock_installer) {
      continue;
    }

    const auto kv = ParseKeyValue(line);
    if (!kv.first.empty()) {
      AssignMockInstallerField(&manifest, kv.first, kv.second);
    }
  }

  return manifest;
}

GuardrailConfig ParseGuardrailConfig(const std::string& path) {
  const std::vector<std::string> lines = ReadCleanLines(path);
  GuardrailConfig config;

  for (const std::string& line : lines) {
    const auto kv = ParseKeyValue(line);
    if (kv.first == "vm_root") {
      config.vm_root = Unquote(kv.second);
    } else if (kv.first == "allow_extensions") {
      config.allow_extensions = ParseInlineList(kv.second);
    } else if (kv.first == "require_lab_mode") {
      config.require_lab_mode = ToLower(Unquote(kv.second)) == "true";
    } else if (kv.first == "required_checks") {
      config.required_checks = ParseInlineList(kv.second);
    }
  }

  return config;
}

bool ValidateScenarioSpec(const ScenarioSpec& scenario, std::string* reason) {
  if (scenario.scenario_id.empty() || scenario.platform.empty()) {
    if (reason != nullptr) {
      *reason = "scenario is missing id or platform";
    }
    return false;
  }

  if (scenario.vm_profile.empty() || scenario.vm_disk_image.empty()) {
    if (reason != nullptr) {
      *reason = "scenario is missing vm_profile or vm_disk_image";
    }
    return false;
  }

  if (scenario.stage_simulators.empty()) {
    if (reason != nullptr) {
      *reason = "scenario has no stage_simulators";
    }
    return false;
  }

  return true;
}

bool ValidateMockInstallerManifest(const MockInstallerManifest& manifest, std::string* reason) {
  if (!manifest.non_operational) {
    if (reason != nullptr) {
      *reason = "mock installer must be non_operational";
    }
    return false;
  }

  const std::vector<std::string> required_phases = {
      "would_stage", "would_hook", "would_chain", "would_restore"};
  for (const std::string& phase : required_phases) {
    if (!ContainsString(manifest.phases, phase)) {
      if (reason != nullptr) {
        *reason = "mock installer is missing required phase: " + phase;
      }
      return false;
    }
  }

  const std::vector<std::string> required_prohibitions = {
      "disk-write",
      "firmware-write",
      "real-bootmgr-patch",
      "real-winload-patch",
      "persistent-installer",
      "live-acpi-aml-modification"};
  for (const std::string& prohibition : required_prohibitions) {
    if (!ContainsString(manifest.prohibited_actions, prohibition)) {
      if (reason != nullptr) {
        *reason = "mock installer is missing prohibited action: " + prohibition;
      }
      return false;
    }
  }

  if (manifest.evidence_contract.empty()) {
    if (reason != nullptr) {
      *reason = "mock installer is missing evidence_contract";
    }
    return false;
  }

  return true;
}

bool ValidateStageManifest(const StageSimulatorManifest& manifest, std::string* reason) {
  if (manifest.module_id.empty() || manifest.boot_stage.empty() || manifest.lang.empty()) {
    if (reason != nullptr) {
      *reason = "manifest is missing module_id, boot_stage, or lang";
    }
    return false;
  }
  if (manifest.marker_contract.empty()) {
    if (reason != nullptr) {
      *reason = "manifest is missing marker_contract";
    }
    return false;
  }
  return true;
}

}  // namespace bootkitstudio
