#pragma once

// QEMU chainloader orchestration (DEFENSIVE).
//
// Implements the snapshot-before -> boot -> evidence-capture -> rollback-after
// preconditions that every scenario declares, using QEMU against a VM disk image.
//
// Hard safety contract (enforced in qemu.cpp, not just documented):
//   * the base image is NEVER written: snapshot is a qcow2 overlay with the base as a
//     read-only backing file; the base is only ever hashed or used as `-b` backing;
//   * QEMU boots the OVERLAY only — the base is never given to qemu-system as a drive;
//   * physical disks (/dev/*, \\.\PhysicalDrive*) are refused, same as the guardrail layer;
//   * overlays live only under the configured vm_root and are deleted on rollback;
//   * the guest gets NO network (-nic none) and NO device passthrough;
//   * requires BKS_LAB_MODE=1 (re-checked here, fail-closed);
//   * when QEMU is absent or the image is a placeholder, the step runs in SIMULATED mode
//     and says so — it never fabricates a boot.

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "bootkitstudio/types.hpp"

namespace bootkitstudio {

struct QemuConfig {
  std::string qemu_img;            // BKS_QEMU_IMG      (default: "qemu-img")
  std::string qemu_system;         // BKS_QEMU_SYSTEM   (default: derived per scenario arch)
  std::string ovmf_code;           // BKS_OVMF_CODE     (UEFI firmware; empty disables real UEFI boot)
  std::string ovmf_vars_secboot;   // BKS_OVMF_VARS_SECBOOT   (Secure Boot ON vars template)
  std::string ovmf_vars_nosecboot; // BKS_OVMF_VARS_NOSECBOOT (Secure Boot OFF vars template)
  bool force_simulate = false;     // BKS_QEMU_FORCE_SIMULATE=1
  long min_image_bytes = 1 << 20;  // BKS_QEMU_MIN_IMAGE_BYTES (below this => placeholder => simulate)
  int max_boot_seconds = 180;      // BKS_QEMU_MAX_BOOT (clamp on the scenario boot timeout)
  std::filesystem::path work_root; // overlay scratch dir, always under vm_root
};

QemuConfig QemuConfigFromEnv(const std::filesystem::path& vm_root);

struct QemuReport {
  bool attempted = false;          // the step ran
  bool real_boot = false;          // true = qemu-system actually launched; false = simulated
  std::string mode = "simulated";  // "real" | "simulated"
  std::string reason;              // why simulated, when it is
  bool rollback_confirmed = false; // base image hash unchanged pre vs post
  std::vector<std::string> events; // merged into evidence.events (qemu_* prefixed)
  std::vector<std::string> alerts; // merged into evidence.alerts
  std::map<std::string, std::string> baseline_hashes;  // merged into evidence.baseline_hashes
  std::map<std::string, std::string> delta_hashes;     // merged into evidence.delta_hashes
};

// Orchestrate the four preconditions for one scenario. Never writes the base image.
// Throws only on a true safety breach (e.g. a physical-disk path slipping through).
QemuReport RunQemuChainload(const ScenarioSpec& scenario,
                            const GuardrailConfig& guardrails,
                            const std::filesystem::path& repo_root,
                            RunnerMode mode,
                            const QemuConfig& config);

}  // namespace bootkitstudio
