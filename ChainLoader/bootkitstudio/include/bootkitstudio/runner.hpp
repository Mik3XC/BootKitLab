#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "bootkitstudio/types.hpp"

namespace bootkitstudio {

struct RunnerPaths {
  std::filesystem::path repo_root;
  std::filesystem::path scenario_matrix;
  std::filesystem::path stage_manifest;
  std::filesystem::path mock_installer_manifest;
  std::filesystem::path guardrail_config;
  std::filesystem::path output_dir;
};

class Runner {
 public:
  explicit Runner(RunnerPaths paths);

  std::vector<EvidenceBundle> Run(RunnerMode mode, const std::string& scenario_filter);
  std::string WriteEvidence(const EvidenceBundle& evidence) const;

 private:
  EvidenceBundle RunScenario(
      const ScenarioSpec& scenario,
      const std::vector<StageSimulatorManifest>& manifests,
      const MockInstallerManifest& mock_installer,
      const GuardrailConfig& guardrails,
      RunnerMode mode);

  RunnerPaths paths_;
};

std::string EvidenceToJson(const EvidenceBundle& evidence);

}  // namespace bootkitstudio
