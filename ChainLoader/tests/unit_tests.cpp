#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "bootkitstudio/guardrails.hpp"
#include "bootkitstudio/hash_utils.hpp"
#include "bootkitstudio/runner.hpp"
#include "bootkitstudio/simulator_registry.hpp"
#include "bootkitstudio/source_claims.hpp"
#include "bootkitstudio/yaml_like_parser.hpp"

#ifndef BKS_REPO_ROOT
#error "BKS_REPO_ROOT is not defined"
#endif

namespace {

int g_failures = 0;

void Check(bool condition, const std::string& message) {
  if (!condition) {
    ++g_failures;
    std::cerr << "FAIL: " << message << "\n";
  }
}

std::filesystem::path RepoRoot() {
  return std::filesystem::path(BKS_REPO_ROOT);
}

bool ContainsValue(const std::vector<std::string>& values, const std::string& expected) {
  for (const std::string& value : values) {
    if (value == expected) {
      return true;
    }
  }
  return false;
}

bool ContainsPrefix(const std::vector<std::string>& values, const std::string& prefix) {
  for (const std::string& value : values) {
    if (value.rfind(prefix, 0) == 0) {
      return true;
    }
  }
  return false;
}

bootkitstudio::RunnerPaths BuildPaths(const std::filesystem::path& output_path) {
  return bootkitstudio::RunnerPaths{
      RepoRoot(),
      RepoRoot() / "Scenario-Matrix.yaml",
      RepoRoot() / "bootkitstudio/manifests/Stage-Simulator-Manifest.yaml",
      RepoRoot() / "bootkitstudio/manifests/Mock-Installer-Manifest.yaml",
      RepoRoot() / "bootkitstudio/config/lab_config.yaml",
      output_path,
  };
}

void TestScenarioParsingAndValidation() {
  const auto scenarios = bootkitstudio::ParseScenarioMatrix((RepoRoot() / "Scenario-Matrix.yaml").string());
  Check(scenarios.size() == 4, "expected 4 scenarios in Scenario-Matrix.yaml");

  for (const auto& scenario : scenarios) {
    std::string reason;
    Check(bootkitstudio::ValidateScenarioSpec(scenario, &reason),
          "scenario validation failed: " + scenario.scenario_id + " reason=" + reason);
  }

  const auto manifests = bootkitstudio::ParseStageSimulatorManifest(
      (RepoRoot() / "bootkitstudio/manifests/Stage-Simulator-Manifest.yaml").string());
  Check(manifests.size() == 4, "expected 4 stage simulator manifests");

  for (const auto& manifest : manifests) {
    std::string reason;
    Check(bootkitstudio::ValidateStageManifest(manifest, &reason),
          "manifest validation failed: " + manifest.module_id + " reason=" + reason);
  }

  const auto mock_installer = bootkitstudio::ParseMockInstallerManifest(
      (RepoRoot() / "bootkitstudio/manifests/Mock-Installer-Manifest.yaml").string());
  std::string reason;
  Check(bootkitstudio::ValidateMockInstallerManifest(mock_installer, &reason),
        "mock installer validation failed: " + reason);
  Check(ContainsValue(mock_installer.phases, "would_stage"), "mock installer should include would_stage");
  Check(ContainsValue(mock_installer.phases, "would_hook"), "mock installer should include would_hook");
  Check(ContainsValue(mock_installer.phases, "would_chain"), "mock installer should include would_chain");
  Check(ContainsValue(mock_installer.phases, "would_restore"), "mock installer should include would_restore");
  Check(ContainsValue(mock_installer.prohibited_actions, "live-acpi-aml-modification"),
        "mock installer should prohibit live ACPI/AML modification");
}

void TestHashAndDeltaGeneration() {
  const std::filesystem::path temp = std::filesystem::temp_directory_path() / "bks_hash_test.tmp";
  {
    std::ofstream output(temp, std::ios::trunc);
    output << "v1\n";
  }
  const std::string h1 = bootkitstudio::Sha256File(temp.string());

  {
    std::ofstream output(temp, std::ios::trunc);
    output << "v2\n";
  }
  const std::string h2 = bootkitstudio::Sha256File(temp.string());

  Check(!h1.empty(), "hash should not be empty");
  Check(!h2.empty(), "hash should not be empty");
  Check(h1 != h2, "hashes should change when file content changes");

  std::error_code ec;
  std::filesystem::remove(temp, ec);
}

void TestEvidenceSerialization() {
  bootkitstudio::EvidenceBundle evidence;
  evidence.scenario_id = "unit-scenario";
  evidence.timestamp = "2026-03-13T00:00:00Z";
  evidence.vm_id = "vm-1";
  evidence.baseline_hashes = {{"vm_disk_image", "sha256:abc"}};
  evidence.delta_hashes = {{"delta", "sha256:def"}};
  evidence.events = {"event-a", "event-b"};
  evidence.alerts = {"alert-a"};
  evidence.analyst_notes = "notes";
  evidence.verdict = "PASS";

  const std::string json = bootkitstudio::EvidenceToJson(evidence);
  Check(json.find("\"scenario_id\":\"unit-scenario\"") != std::string::npos,
        "evidence json should include scenario_id");
  Check(json.find("\"verdict\":\"PASS\"") != std::string::npos,
        "evidence json should include verdict");
}

void TestGuardrailEnforcement() {
  auto scenarios = bootkitstudio::ParseScenarioMatrix((RepoRoot() / "Scenario-Matrix.yaml").string());
  const bootkitstudio::GuardrailConfig config =
      bootkitstudio::ParseGuardrailConfig((RepoRoot() / "bootkitstudio/config/lab_config.yaml").string());

  bootkitstudio::ScenarioSpec scenario = scenarios.at(0);
  std::string reason;

  unsetenv("BKS_LAB_MODE");
  Check(!bootkitstudio::ValidateScenarioGuardrails(scenario, config, RepoRoot(), &reason),
        "guardrails should fail when BKS_LAB_MODE is not enabled");

  setenv("BKS_LAB_MODE", "1", 1);
  reason.clear();
  Check(bootkitstudio::ValidateScenarioGuardrails(scenario, config, RepoRoot(), &reason),
        "guardrails should pass for approved VM disk image and lab mode");

  scenario.vm_disk_image = "/dev/sda";
  reason.clear();
  Check(!bootkitstudio::ValidateScenarioGuardrails(scenario, config, RepoRoot(), &reason),
        "guardrails should reject physical disk paths");

  scenario = scenarios.at(0);
  scenario.vm_disk_image = "Chain-Loader.rtf";
  reason.clear();
  Check(!bootkitstudio::ValidateScenarioGuardrails(scenario, config, RepoRoot(), &reason),
        "guardrails should reject paths outside vm_root");
}

void TestSourceClaimCoverage() {
  const auto claims = bootkitstudio::ParseSourceClaims((RepoRoot() / "Source-Claims.yaml").string());
  std::string reason;
  Check(bootkitstudio::ValidateSourceClaimsCoverage(claims, &reason),
        "source claims coverage validation failed: " + reason);
}

void TestAcpiAnnotationAndMockInstallerEvidence() {
  Check(bootkitstudio::ResolveStageMarker("sim.acpi.annotation.marker") == "SIM-ACPI-ANNOTATION-MARKER-V1",
        "ACPI annotation marker should resolve to the expected string");

  setenv("BKS_LAB_MODE", "1", 1);
  bootkitstudio::Runner runner(BuildPaths(RepoRoot() / "output/evidence/unit-mock-installer"));
  const auto results = runner.Run(bootkitstudio::RunnerMode::kBaseline, "classroom-acpi-annotation");

  Check(results.size() == 1, "ACPI annotation scenario should produce one evidence bundle");
  if (!results.empty()) {
    const auto& evidence = results.front();
    Check(evidence.verdict == "PASS", "ACPI annotation scenario should pass");
    Check(evidence.baseline_hashes.find("marker:sim.acpi.annotation.marker") != evidence.baseline_hashes.end(),
          "ACPI marker hash should be present");
    Check(ContainsValue(evidence.events, "mock_installer=would_stage"), "evidence should log would_stage");
    Check(ContainsValue(evidence.events, "mock_installer=would_hook"), "evidence should log would_hook");
    Check(ContainsValue(evidence.events, "mock_installer=would_chain"), "evidence should log would_chain");
    Check(ContainsValue(evidence.events, "mock_installer=would_restore"), "evidence should log would_restore");
    Check(ContainsPrefix(evidence.events, "safety_contract=no_disk_writes"),
          "evidence should include the non-operational safety contract");
    Check(ContainsValue(evidence.events, "mock_installer_prohibited_action=real-bootmgr-patch"),
          "evidence should prohibit real bootmgr patching");
    Check(ContainsValue(evidence.events, "mock_installer_prohibited_action=live-acpi-aml-modification"),
          "evidence should prohibit live ACPI/AML modification");
  }
}

}  // namespace

int main() {
  try {
    TestScenarioParsingAndValidation();
    TestHashAndDeltaGeneration();
    TestEvidenceSerialization();
    TestGuardrailEnforcement();
    TestSourceClaimCoverage();
    TestAcpiAnnotationAndMockInstallerEvidence();
  } catch (const std::exception& error) {
    std::cerr << "Unhandled test exception: " << error.what() << "\n";
    return 1;
  }

  if (g_failures != 0) {
    std::cerr << "Total failures: " << g_failures << "\n";
    return 1;
  }

  std::cout << "All unit tests passed.\n";
  return 0;
}
