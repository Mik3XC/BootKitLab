#pragma once

#include <filesystem>
#include <string>

#include "bootkitstudio/types.hpp"

namespace bootkitstudio {

bool ValidateScenarioGuardrails(
    const ScenarioSpec& scenario,
    const GuardrailConfig& config,
    const std::filesystem::path& repo_root,
    std::string* reason);

bool IsLabModeEnabled();

}  // namespace bootkitstudio
