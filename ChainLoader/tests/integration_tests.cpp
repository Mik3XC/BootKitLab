#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bootkitstudio/runner.hpp"

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

void TestBaselineAcrossMatrix() {
  setenv("BKS_LAB_MODE", "1", 1);

  const std::filesystem::path out = RepoRoot() / "output/evidence/integration-baseline";
  bootkitstudio::Runner runner(BuildPaths(out));
  const auto results = runner.Run(bootkitstudio::RunnerMode::kBaseline, "all");

  Check(results.size() == 4, "baseline run should execute all 4 scenarios");
  for (const auto& result : results) {
    Check(result.verdict == "PASS", "baseline verdict should be PASS for " + result.scenario_id);
    Check(!result.baseline_hashes.empty(), "baseline hashes should be populated");
    const std::string evidence_path = runner.WriteEvidence(result);
    Check(std::filesystem::exists(evidence_path), "evidence file should exist");
  }
}

void TestControlledDriftAcrossMatrix() {
  setenv("BKS_LAB_MODE", "1", 1);

  const std::filesystem::path out = RepoRoot() / "output/evidence/integration-controlled-drift";
  bootkitstudio::Runner runner(BuildPaths(out));
  const auto results = runner.Run(bootkitstudio::RunnerMode::kControlledDrift, "all");

  Check(results.size() == 4, "controlled-drift run should execute all 4 scenarios");
  for (const auto& result : results) {
    Check(result.verdict == "PASS", "controlled-drift verdict should be PASS for " + result.scenario_id);
    Check(!result.delta_hashes.empty(), "controlled-drift should produce delta hashes");
    bool has_detected_alert = false;
    for (const std::string& alert : result.alerts) {
      if (alert.find("detected:") == 0) {
        has_detected_alert = true;
      }
    }
    Check(has_detected_alert, "controlled-drift should emit detected alerts");
  }
}

void TestRecoveryAcrossMatrix() {
  setenv("BKS_LAB_MODE", "1", 1);

  const std::filesystem::path out = RepoRoot() / "output/evidence/integration-recovery";
  bootkitstudio::Runner runner(BuildPaths(out));
  const auto results = runner.Run(bootkitstudio::RunnerMode::kRecoveryValidation, "all");

  Check(results.size() == 4, "recovery run should execute all 4 scenarios");
  for (const auto& result : results) {
    Check(result.verdict == "PASS", "recovery verdict should be PASS for " + result.scenario_id);
    bool has_hardening_alert = false;
    for (const std::string& alert : result.alerts) {
      if (alert.find("hardened:") == 0) {
        has_hardening_alert = true;
      }
    }
    Check(has_hardening_alert, "recovery-validation should emit hardened alerts");
  }
}

void TestReproducibility() {
  setenv("BKS_LAB_MODE", "1", 1);

  const std::filesystem::path out = RepoRoot() / "output/evidence/integration-repro";
  bootkitstudio::Runner runner(BuildPaths(out));

  const auto run_a = runner.Run(bootkitstudio::RunnerMode::kBaseline, "win10-bios-mbr");
  const auto run_b = runner.Run(bootkitstudio::RunnerMode::kBaseline, "win10-bios-mbr");

  Check(run_a.size() == 1 && run_b.size() == 1, "reproducibility test should run one scenario each time");
  Check(run_a[0].verdict == run_b[0].verdict, "verdict should be reproducible");
  Check(run_a[0].baseline_hashes.at("vm_disk_image") == run_b[0].baseline_hashes.at("vm_disk_image"),
        "vm disk baseline hash should be reproducible");
}

}  // namespace

int main() {
  try {
    TestBaselineAcrossMatrix();
    TestControlledDriftAcrossMatrix();
    TestRecoveryAcrossMatrix();
    TestReproducibility();
  } catch (const std::exception& error) {
    std::cerr << "Unhandled integration test exception: " << error.what() << "\n";
    return 1;
  }

  if (g_failures != 0) {
    std::cerr << "Total failures: " << g_failures << "\n";
    return 1;
  }

  std::cout << "All integration tests passed.\n";
  return 0;
}
