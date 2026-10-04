#include "bootkitstudio/guardrails.hpp"

#include <cstdlib>
#include <filesystem>

#include "bootkitstudio/util.hpp"

namespace bootkitstudio {
namespace {

bool IsPrefixPath(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  auto root_it = root.begin();
  auto candidate_it = candidate.begin();
  for (; root_it != root.end() && candidate_it != candidate.end(); ++root_it, ++candidate_it) {
    if (*root_it != *candidate_it) {
      return false;
    }
  }
  return root_it == root.end();
}

bool HasAllowedExtension(const std::filesystem::path& path, const std::vector<std::string>& allow_extensions) {
  const std::string ext = ToLower(path.extension().string());
  for (const std::string& allowed : allow_extensions) {
    if (ToLower(allowed) == ext) {
      return true;
    }
  }
  return false;
}

bool IsPhysicalDiskPath(const std::filesystem::path& path) {
  const std::string normalized = ToLower(path.string());
  return normalized.rfind("/dev/", 0) == 0 || normalized.rfind("\\\\.\\physicaldrive", 0) == 0;
}

bool Contains(const std::vector<std::string>& values, const std::string& expected) {
  for (const std::string& value : values) {
    if (ToLower(value) == ToLower(expected)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool IsLabModeEnabled() {
  const char* value = std::getenv("BKS_LAB_MODE");
  return value != nullptr && std::string(value) == "1";
}

bool ValidateScenarioGuardrails(
    const ScenarioSpec& scenario,
    const GuardrailConfig& config,
    const std::filesystem::path& repo_root,
    std::string* reason) {
  if (config.require_lab_mode && !IsLabModeEnabled()) {
    if (reason != nullptr) {
      *reason = "lab mode is disabled; set BKS_LAB_MODE=1";
    }
    return false;
  }

  for (const std::string& required : config.required_checks) {
    if (!Contains(scenario.preconditions, required)) {
      if (reason != nullptr) {
        *reason = "scenario is missing required precondition: " + required;
      }
      return false;
    }
  }

  std::filesystem::path disk_path = scenario.vm_disk_image;
  if (disk_path.is_relative()) {
    disk_path = repo_root / disk_path;
  }

  if (IsPhysicalDiskPath(disk_path)) {
    if (reason != nullptr) {
      *reason = "physical disk paths are not allowed";
    }
    return false;
  }

  std::error_code ec;
  const std::filesystem::path canonical_disk = std::filesystem::weakly_canonical(disk_path, ec);
  if (ec) {
    if (reason != nullptr) {
      *reason = "unable to resolve vm_disk_image path";
    }
    return false;
  }

  std::filesystem::path vm_root = config.vm_root;
  if (vm_root.is_relative()) {
    vm_root = repo_root / vm_root;
  }
  const std::filesystem::path canonical_vm_root = std::filesystem::weakly_canonical(vm_root, ec);
  if (ec) {
    if (reason != nullptr) {
      *reason = "unable to resolve vm_root path";
    }
    return false;
  }

  if (!IsPrefixPath(canonical_vm_root, canonical_disk)) {
    if (reason != nullptr) {
      *reason = "vm_disk_image is outside the configured vm_root";
    }
    return false;
  }

  if (!HasAllowedExtension(canonical_disk, config.allow_extensions)) {
    if (reason != nullptr) {
      *reason = "vm_disk_image extension is not allowed";
    }
    return false;
  }

  return true;
}

}  // namespace bootkitstudio
