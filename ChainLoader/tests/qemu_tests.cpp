// QEMU chainloader tests. Self-contained: the test writes fake qemu-img / qemu-system
// shims to a temp dir and points BKS_QEMU_IMG / BKS_QEMU_SYSTEM at them, so the real
// orchestration path is exercised deterministically with no hypervisor and no root.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <unistd.h>

#include "bootkitstudio/qemu.hpp"
#include "bootkitstudio/hash_utils.hpp"

namespace fs = std::filesystem;

namespace {
int g_failures = 0;
void Check(bool c, const std::string& m) {
  if (!c) { ++g_failures; std::cerr << "FAIL: " << m << "\n"; }
}

void Write(const fs::path& p, const std::string& body, bool exec = false) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::trunc) << body;
  if (exec) fs::permissions(p, fs::perms::owner_all | fs::perms::group_read |
                                   fs::perms::group_exec | fs::perms::others_read |
                                   fs::perms::others_exec);
}

bootkitstudio::ScenarioSpec BiosScenario(const std::string& profile) {
  bootkitstudio::ScenarioSpec s;
  s.scenario_id = "test-" + profile;
  s.platform = "Win10 BIOS/MBR";  // legacy => no OVMF needed
  s.preconditions = {"snapshot-before", "rollback-after", "evidence-capture", "bks-lab-mode"};
  s.vm_profile = profile;
  s.vm_disk_image = "vms/" + profile + "/disk.qcow2";
  s.timeouts["boot_seconds"] = 3;
  return s;
}

bootkitstudio::GuardrailConfig Guardrails(const fs::path& repo) {
  bootkitstudio::GuardrailConfig g;
  g.vm_root = "vms";
  g.allow_extensions = {".img", ".qcow2", ".vhd", ".vmdk"};
  g.require_lab_mode = true;
  g.required_checks = {"snapshot-before", "rollback-after", "evidence-capture"};
  (void)repo;
  return g;
}

// A 2 MiB base image so it passes the placeholder size floor.
void MakeImage(const fs::path& p, long bytes = 2 * 1024 * 1024) {
  fs::create_directories(p.parent_path());
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  std::string chunk(4096, 'B');
  for (long w = 0; w < bytes; w += static_cast<long>(chunk.size())) f.write(chunk.data(), chunk.size());
}

fs::path SetupShims(const fs::path& root, bool slow) {
  const fs::path bin = root / "bin";
  // qemu-img: handle --version and `create ... <overlay>` (overlay is the last arg).
  Write(bin / "qemu-img",
        "#!/bin/sh\n"
        "if [ \"$1\" = \"--version\" ]; then echo 'qemu-img version 0.0-shim'; exit 0; fi\n"
        "for a in \"$@\"; do last=\"$a\"; done\n"
        "if [ \"$1\" = \"create\" ]; then : > \"$last\"; echo shim-overlay > \"$last\"; fi\n"
        "exit 0\n",
        true);
  // qemu-system: write one serial line (from -serial file:PATH), optionally sleep, exit 0.
  std::string sys =
      "#!/bin/sh\n"
      "if [ \"$1\" = \"--version\" ]; then echo 'QEMU emulator version 0.0-shim'; exit 0; fi\n"
      "prev=\"\"\n"
      "for a in \"$@\"; do\n"
      "  if [ \"$prev\" = \"-serial\" ]; then\n"
      "    case \"$a\" in file:*) echo 'SHIM BOOT MARKER' > \"${a#file:}\";; esac\n"
      "  fi\n"
      "  prev=\"$a\"\n"
      "done\n";
  sys += slow ? "sleep 30\n" : "";
  sys += "exit 0\n";
  Write(bin / "qemu-system-x86_64", sys, true);
  return bin;
}
}  // namespace

int main() {
  setenv("BKS_LAB_MODE", "1", 1);
  const fs::path root = fs::temp_directory_path() / ("bks-qemu-" + std::to_string(::getpid()));
  fs::remove_all(root);

  // ---- real path: snapshot -> boot -> rollback ----
  {
    const fs::path repo = root / "real";
    MakeImage(repo / "vms/p/disk.qcow2");
    const fs::path bin = SetupShims(repo, /*slow=*/false);
    setenv("BKS_QEMU_IMG", (bin / "qemu-img").c_str(), 1);
    setenv("BKS_QEMU_SYSTEM", (bin / "qemu-system-x86_64").c_str(), 1);
    unsetenv("BKS_QEMU_FORCE_SIMULATE");

    const std::string base_pre = bootkitstudio::Sha256File((repo / "vms/p/disk.qcow2").string());
    auto cfg = bootkitstudio::QemuConfigFromEnv(repo / "vms");
    auto r = bootkitstudio::RunQemuChainload(BiosScenario("p"), Guardrails(repo), repo,
                                             bootkitstudio::RunnerMode::kBaseline, cfg);
    Check(r.mode == "real", "real image should use real mode (got " + r.mode + ": " + r.reason + ")");
    Check(r.real_boot, "qemu-system shim should have been launched");
    Check(r.rollback_confirmed, "base image must be unchanged after run");
    Check(r.delta_hashes.count("qemu:serial_log_sha256") == 1, "serial log should be captured");
    const std::string base_post = bootkitstudio::Sha256File((repo / "vms/p/disk.qcow2").string());
    Check(base_pre == base_post, "base image bytes must be identical pre/post");
    // overlay must be gone (rollback cleaned it up)
    int overlays = 0;
    const fs::path work = repo / "vms/.qemu-work/p";
    if (fs::exists(work))
      for (auto& e : fs::directory_iterator(work))
        if (e.path().extension() == ".qcow2") ++overlays;
    Check(overlays == 0, "overlay must be discarded on rollback");
  }

  // ---- boot timeout is handled (expected, not a failure) and still rolls back ----
  {
    const fs::path repo = root / "slow";
    MakeImage(repo / "vms/p/disk.qcow2");
    const fs::path bin = SetupShims(repo, /*slow=*/true);
    setenv("BKS_QEMU_IMG", (bin / "qemu-img").c_str(), 1);
    setenv("BKS_QEMU_SYSTEM", (bin / "qemu-system-x86_64").c_str(), 1);
    auto cfg = bootkitstudio::QemuConfigFromEnv(repo / "vms");
    auto r = bootkitstudio::RunQemuChainload(BiosScenario("p"), Guardrails(repo), repo,
                                             bootkitstudio::RunnerMode::kBaseline, cfg);
    Check(r.real_boot, "slow boot still launches");
    Check(r.rollback_confirmed, "timeout path must still roll back");
    bool saw_timeout = false;
    for (const auto& e : r.events)
      if (e.rfind("qemu_boot=timeout-reached", 0) == 0) saw_timeout = true;
    Check(saw_timeout, "a slow boot should be reported as timeout-reached");
  }

  // ---- force-simulate even with shims present ----
  {
    const fs::path repo = root / "forcesim";
    MakeImage(repo / "vms/p/disk.qcow2");
    const fs::path bin = SetupShims(repo, false);
    setenv("BKS_QEMU_IMG", (bin / "qemu-img").c_str(), 1);
    setenv("BKS_QEMU_SYSTEM", (bin / "qemu-system-x86_64").c_str(), 1);
    setenv("BKS_QEMU_FORCE_SIMULATE", "1", 1);
    auto cfg = bootkitstudio::QemuConfigFromEnv(repo / "vms");
    auto r = bootkitstudio::RunQemuChainload(BiosScenario("p"), Guardrails(repo), repo,
                                             bootkitstudio::RunnerMode::kBaseline, cfg);
    Check(r.mode == "simulated", "force-simulate must stay simulated");
    Check(r.rollback_confirmed, "simulated run trivially rolls back");
    unsetenv("BKS_QEMU_FORCE_SIMULATE");
  }

  // ---- placeholder image => simulated ----
  {
    const fs::path repo = root / "placeholder";
    MakeImage(repo / "vms/p/disk.qcow2", 32);  // tiny, below the floor
    const fs::path bin = SetupShims(repo, false);
    setenv("BKS_QEMU_IMG", (bin / "qemu-img").c_str(), 1);
    setenv("BKS_QEMU_SYSTEM", (bin / "qemu-system-x86_64").c_str(), 1);
    auto cfg = bootkitstudio::QemuConfigFromEnv(repo / "vms");
    auto r = bootkitstudio::RunQemuChainload(BiosScenario("p"), Guardrails(repo), repo,
                                             bootkitstudio::RunnerMode::kBaseline, cfg);
    Check(r.mode == "simulated", "placeholder image must be simulated");
  }

  // ---- safety: a physical disk path must throw (fail closed) ----
  {
    const fs::path repo = root / "phys";
    auto s = BiosScenario("p");
    s.vm_disk_image = "/dev/rdisk9";
    auto g = Guardrails(repo);
    auto cfg = bootkitstudio::QemuConfigFromEnv(repo / "vms");
    bool threw = false;
    try {
      bootkitstudio::RunQemuChainload(s, g, repo, bootkitstudio::RunnerMode::kBaseline, cfg);
    } catch (const std::exception&) {
      threw = true;
    }
    Check(threw, "a physical disk path must be refused");
  }

  fs::remove_all(root);
  if (g_failures != 0) { std::cerr << "Total failures: " << g_failures << "\n"; return 1; }
  std::cout << "All qemu tests passed.\n";
  return 0;
}
