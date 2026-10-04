#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include "bootkitstudio/runner.hpp"

namespace {

void PrintUsage() {
  std::cout << "BootKitStudio defensive runner\n"
            << "Usage:\n"
            << "  bootkitstudio [--mode baseline|controlled-drift|recovery-validation]\\\n"
            << "                [--scenario <scenario_id>|all]\\\n"
            << "                [--repo-root <path>]\\\n"
            << "                [--scenario-matrix <path>]\\\n"
            << "                [--manifest <path>]\\\n"
            << "                [--mock-installer-manifest <path>]\\\n"
            << "                [--guardrails <path>]\\\n"
            << "                [--output <path>]\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    std::filesystem::path repo_root = std::filesystem::current_path();
    std::string mode_string = "baseline";
    std::string scenario_filter = "all";

    std::filesystem::path scenario_matrix = "Scenario-Matrix.yaml";
    std::filesystem::path manifest = "bootkitstudio/manifests/Stage-Simulator-Manifest.yaml";
    std::filesystem::path mock_installer_manifest = "bootkitstudio/manifests/Mock-Installer-Manifest.yaml";
    std::filesystem::path guardrails = "bootkitstudio/config/lab_config.yaml";
    std::filesystem::path output = "output/evidence";

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      auto require_value = [&](const std::string& flag) -> std::string {
        if (i + 1 >= argc) {
          throw std::runtime_error("Missing value for flag: " + flag);
        }
        return argv[++i];
      };

      if (arg == "--mode") {
        mode_string = require_value(arg);
      } else if (arg == "--scenario") {
        scenario_filter = require_value(arg);
      } else if (arg == "--repo-root") {
        repo_root = require_value(arg);
      } else if (arg == "--scenario-matrix") {
        scenario_matrix = require_value(arg);
      } else if (arg == "--manifest") {
        manifest = require_value(arg);
      } else if (arg == "--mock-installer-manifest") {
        mock_installer_manifest = require_value(arg);
      } else if (arg == "--guardrails") {
        guardrails = require_value(arg);
      } else if (arg == "--output") {
        output = require_value(arg);
      } else if (arg == "--help" || arg == "-h") {
        PrintUsage();
        return 0;
      } else {
        throw std::runtime_error("Unknown flag: " + arg);
      }
    }

    const bootkitstudio::RunnerPaths paths{
        repo_root,
        std::filesystem::path(repo_root) / scenario_matrix,
        std::filesystem::path(repo_root) / manifest,
        std::filesystem::path(repo_root) / mock_installer_manifest,
        std::filesystem::path(repo_root) / guardrails,
        std::filesystem::path(repo_root) / output,
    };

    bootkitstudio::Runner runner(paths);
    const bootkitstudio::RunnerMode mode = bootkitstudio::RunnerModeFromString(mode_string);
    const auto evidence = runner.Run(mode, scenario_filter);

    std::cout << "BootKitStudio run complete\n";
    std::cout << "mode=" << mode_string << " scenarios=" << evidence.size() << "\n";

    for (const auto& item : evidence) {
      const std::string output_path = runner.WriteEvidence(item);
      std::cout << "  - " << item.scenario_id << " verdict=" << item.verdict
                << " output=" << output_path << "\n";
    }

    return 0;
  } catch (const std::exception& error) {
    std::cerr << "ERROR: " << error.what() << "\n";
    PrintUsage();
    return 1;
  }
}
