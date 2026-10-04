#pragma once

#include <string>
#include <vector>

#include "bootkitstudio/types.hpp"

namespace bootkitstudio {

std::vector<ScenarioSpec> ParseScenarioMatrix(const std::string& path);
std::vector<StageSimulatorManifest> ParseStageSimulatorManifest(const std::string& path);
MockInstallerManifest ParseMockInstallerManifest(const std::string& path);
GuardrailConfig ParseGuardrailConfig(const std::string& path);

bool ValidateScenarioSpec(const ScenarioSpec& scenario, std::string* reason);
bool ValidateStageManifest(const StageSimulatorManifest& manifest, std::string* reason);
bool ValidateMockInstallerManifest(const MockInstallerManifest& manifest, std::string* reason);

}  // namespace bootkitstudio
