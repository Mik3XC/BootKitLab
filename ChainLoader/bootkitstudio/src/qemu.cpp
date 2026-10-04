#include "bootkitstudio/qemu.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>

#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bootkitstudio/guardrails.hpp"
#include "bootkitstudio/hash_utils.hpp"
#include "bootkitstudio/util.hpp"

namespace bootkitstudio {
namespace {

std::string EnvOr(const char* name, const std::string& fallback) {
  const char* v = std::getenv(name);
  return (v != nullptr && *v != '\0') ? std::string(v) : fallback;
}

bool PathIsExecutable(const std::string& path) {
  return !path.empty() && ::access(path.c_str(), X_OK) == 0;
}

// Resolve a program to a runnable path: an explicit path as-is, or a PATH lookup.
std::string ResolveExe(const std::string& name) {
  if (name.find('/') != std::string::npos) {
    return PathIsExecutable(name) ? name : std::string();
  }
  const char* path_env = std::getenv("PATH");
  if (path_env == nullptr) return std::string();
  std::stringstream ss(path_env);
  std::string dir;
  while (std::getline(ss, dir, ':')) {
    if (dir.empty()) continue;
    std::string candidate = dir + "/" + name;
    if (PathIsExecutable(candidate)) return candidate;
  }
  return std::string();
}

long FileSizeBytes(const std::filesystem::path& p) {
  struct stat st {};
  if (::stat(p.c_str(), &st) != 0) return -1;
  return static_cast<long>(st.st_size);
}

std::string BackingFormatForExtension(const std::filesystem::path& p) {
  const std::string ext = ToLower(p.extension().string());
  if (ext == ".qcow2") return "qcow2";
  if (ext == ".img") return "raw";
  if (ext == ".vhd") return "vpc";
  if (ext == ".vmdk") return "vmdk";
  return "raw";
}

bool IsPhysicalDiskPath(const std::filesystem::path& path) {
  const std::string n = ToLower(path.string());
  return n.rfind("/dev/", 0) == 0 || n.rfind("\\\\.\\physicaldrive", 0) == 0;
}

bool UnderRoot(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  std::error_code ec;
  const std::filesystem::path r = std::filesystem::weakly_canonical(root, ec);
  if (ec) return false;
  const std::filesystem::path c = std::filesystem::weakly_canonical(candidate, ec);
  if (ec) return false;
  auto ri = r.begin();
  auto ci = c.begin();
  for (; ri != r.end() && ci != c.end(); ++ri, ++ci) {
    if (*ri != *ci) return false;
  }
  return ri == r.end();
}

// Run argv[0] with arguments, capturing combined stdout+stderr (bounded) and enforcing a
// wall-clock timeout by killing the child's whole process group. No shell is involved, so
// nothing in argv is interpreted — there is no command injection surface. POSIX (macOS+Linux).
int RunProc(const std::vector<std::string>& argv, int timeout_seconds,
            std::string* out, bool* timed_out) {
  if (timed_out) *timed_out = false;
  if (out) out->clear();
  if (argv.empty()) return -1;

  int pipefd[2];
  if (::pipe(pipefd) != 0) return -1;

  const pid_t pid = ::fork();
  if (pid < 0) {
    ::close(pipefd[0]);
    ::close(pipefd[1]);
    return -1;
  }
  if (pid == 0) {
    // Child: own process group so the parent can signal the whole tree on timeout.
    ::setpgid(0, 0);
    ::dup2(pipefd[1], STDOUT_FILENO);
    ::dup2(pipefd[1], STDERR_FILENO);
    ::close(pipefd[0]);
    ::close(pipefd[1]);
    std::vector<char*> c_argv;
    c_argv.reserve(argv.size() + 1);
    for (const std::string& a : argv) c_argv.push_back(const_cast<char*>(a.c_str()));
    c_argv.push_back(nullptr);
    ::execvp(c_argv[0], c_argv.data());
    ::_exit(127);  // exec failed
  }

  // Parent.
  ::close(pipefd[1]);
  ::setpgid(pid, pid);  // race-free with the child also setting it
  const int rfd = pipefd[0];
  ::fcntl(rfd, F_SETFL, O_NONBLOCK);

  const auto start = std::chrono::steady_clock::now();
  const size_t kCap = 64 * 1024;
  bool killed = false;
  int status = 0;

  while (true) {
    const pid_t w = ::waitpid(pid, &status, WNOHANG);
    // Drain whatever output is available.
    struct pollfd pfd { rfd, POLLIN, 0 };
    if (::poll(&pfd, 1, 100) > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
      char buf[4096];
      ssize_t n;
      while ((n = ::read(rfd, buf, sizeof(buf))) > 0) {
        if (out && out->size() < kCap) {
          out->append(buf, static_cast<size_t>(std::min<size_t>(n, kCap - out->size())));
        }
      }
    }
    if (w == pid) break;  // child reaped

    const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    if (!killed && timeout_seconds > 0 && elapsed >= timeout_seconds) {
      if (timed_out) *timed_out = true;
      killed = true;
      ::kill(-pid, SIGTERM);
      struct timespec ts { 1, 0 };
      ::nanosleep(&ts, nullptr);
      ::kill(-pid, SIGKILL);
    }
  }
  ::close(rfd);
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
  return -1;
}

std::string FirstLine(const std::string& s) {
  const auto pos = s.find('\n');
  return pos == std::string::npos ? s : s.substr(0, pos);
}

bool IsUefiScenario(const ScenarioSpec& scenario) {
  const std::string p = ToLower(scenario.platform + " " + scenario.vm_profile);
  return p.find("uefi") != std::string::npos;
}

bool IsSecureBootOn(const ScenarioSpec& scenario) {
  const std::string p = ToLower(scenario.platform + " " + scenario.vm_profile);
  return p.find("sb-on") != std::string::npos || p.find("secure boot on") != std::string::npos;
}

int BootTimeout(const ScenarioSpec& scenario, const QemuConfig& cfg) {
  int t = 120;
  auto it = scenario.timeouts.find("boot_seconds");
  if (it != scenario.timeouts.end() && it->second > 0) t = it->second;
  if (t > cfg.max_boot_seconds) t = cfg.max_boot_seconds;
  return t;
}

}  // namespace

QemuConfig QemuConfigFromEnv(const std::filesystem::path& vm_root) {
  QemuConfig cfg;
  cfg.qemu_img = EnvOr("BKS_QEMU_IMG", "qemu-img");
  cfg.qemu_system = EnvOr("BKS_QEMU_SYSTEM", "qemu-system-x86_64");
  cfg.ovmf_code = EnvOr("BKS_OVMF_CODE", "");
  cfg.ovmf_vars_secboot = EnvOr("BKS_OVMF_VARS_SECBOOT", "");
  cfg.ovmf_vars_nosecboot = EnvOr("BKS_OVMF_VARS_NOSECBOOT", "");
  cfg.force_simulate = EnvOr("BKS_QEMU_FORCE_SIMULATE", "0") == "1";
  try {
    cfg.min_image_bytes = std::stol(EnvOr("BKS_QEMU_MIN_IMAGE_BYTES", "1048576"));
  } catch (...) {
    cfg.min_image_bytes = 1 << 20;
  }
  try {
    cfg.max_boot_seconds = std::stoi(EnvOr("BKS_QEMU_MAX_BOOT", "180"));
  } catch (...) {
    cfg.max_boot_seconds = 180;
  }
  cfg.work_root = vm_root / ".qemu-work";
  return cfg;
}

QemuReport RunQemuChainload(const ScenarioSpec& scenario,
                            const GuardrailConfig& guardrails,
                            const std::filesystem::path& repo_root,
                            RunnerMode mode,
                            const QemuConfig& config) {
  (void)mode;
  QemuReport report;
  report.attempted = true;

  // --- fail-closed safety re-checks (independent of the guardrail layer) ---
  if (!IsLabModeEnabled()) {
    report.reason = "BKS_LAB_MODE is not set";
    report.events.push_back("qemu_step=refused reason=lab-mode-off");
    report.rollback_confirmed = true;  // nothing was touched
    return report;
  }

  std::filesystem::path base = scenario.vm_disk_image;
  if (base.is_relative()) base = repo_root / base;
  if (IsPhysicalDiskPath(base)) {
    // A physical disk must never reach QEMU. Fail closed, loudly.
    throw std::runtime_error("qemu: refusing physical disk path: " + base.string());
  }
  std::filesystem::path vm_root = guardrails.vm_root;
  if (vm_root.is_relative()) vm_root = repo_root / vm_root;
  if (!UnderRoot(vm_root, base)) {
    throw std::runtime_error("qemu: vm_disk_image is outside vm_root: " + base.string());
  }

  const std::string base_hash_pre = Sha256File(base.string());
  report.baseline_hashes["qemu:base_image_pre"] = base_hash_pre;

  // --- decide real vs simulated ---
  const std::string qemu_img = ResolveExe(config.qemu_img);
  const std::string qemu_system = ResolveExe(config.qemu_system);
  const long size = FileSizeBytes(base);
  std::string why_sim;
  if (config.force_simulate) why_sim = "forced (BKS_QEMU_FORCE_SIMULATE=1)";
  else if (qemu_img.empty()) why_sim = "qemu-img not found";
  else if (size < config.min_image_bytes)
    why_sim = "placeholder image (" + std::to_string(size) + " bytes < min)";

  if (!why_sim.empty()) {
    report.mode = "simulated";
    report.reason = why_sim;
    report.real_boot = false;
    report.rollback_confirmed = true;  // no overlay, no boot, base untouched by definition
    report.events.push_back("qemu_mode=simulated reason=" + why_sim);
    report.events.push_back("qemu_snapshot=simulated");
    report.events.push_back("qemu_boot=simulated");
    report.events.push_back("qemu_rollback=simulated rollback_confirmed=true");
    report.events.push_back("event_logs=simulated:no-serial-capture");
    report.alerts.push_back("boot-chain:qemu-simulated-only");
    report.baseline_hashes["qemu:base_image_post"] = base_hash_pre;
    return report;
  }

  // --- REAL snapshot-before: a qcow2 overlay backed read-only by the base image ---
  report.mode = "real";
  std::error_code ec;
  const std::filesystem::path work = config.work_root / scenario.vm_profile;
  std::filesystem::create_directories(work, ec);
  const std::string stamp = CurrentIsoUtcTimestamp();
  const std::filesystem::path overlay = work / ("overlay-" + stamp + ".qcow2");
  const std::string backing_fmt = BackingFormatForExtension(base);

  // Overlay must resolve under vm_root — never write anywhere else.
  if (!UnderRoot(vm_root, overlay)) {
    throw std::runtime_error("qemu: overlay path escaped vm_root: " + overlay.string());
  }

  std::string img_out;
  bool img_to = false;
  const int img_rc = RunProc(
      {qemu_img, "create", "-q", "-f", "qcow2", "-b", base.string(), "-F", backing_fmt,
       overlay.string()},
      60, &img_out, &img_to);
  if (img_rc != 0 || !std::filesystem::exists(overlay)) {
    // Could not snapshot — degrade to simulated, leave base untouched.
    report.mode = "simulated";
    report.reason = "qemu-img snapshot failed (rc=" + std::to_string(img_rc) + ")";
    report.real_boot = false;
    report.rollback_confirmed = true;
    report.events.push_back("qemu_snapshot=failed rc=" + std::to_string(img_rc));
    if (!img_out.empty()) report.events.push_back("qemu_img_stderr=" + FirstLine(img_out));
    report.events.push_back("qemu_mode=simulated reason=snapshot-failed");
    report.events.push_back("event_logs=simulated:no-serial-capture");
    report.baseline_hashes["qemu:base_image_post"] = Sha256File(base.string());
    std::filesystem::remove(overlay, ec);
    return report;
  }
  report.events.push_back("qemu_snapshot=overlay-created backing=" + backing_fmt);
  report.delta_hashes["qemu:overlay_sha256"] = Sha256File(overlay.string());

  // --- boot + evidence-capture (the OVERLAY only; base is never a drive) ---
  const std::filesystem::path serial = work / ("serial-" + stamp + ".log");
  std::filesystem::path vars_copy;  // per-run OVMF vars, so Secure Boot state is not shared
  const bool uefi = IsUefiScenario(scenario);
  bool can_boot = !qemu_system.empty();
  std::string boot_skip_reason;

  std::vector<std::string> argv;
  if (can_boot) {
    argv = {qemu_system,
            "-machine", uefi ? "q35" : "pc",
            "-m", "2048",
            "-nographic", "-display", "none",
            "-serial", "file:" + serial.string(),
            "-no-reboot",
            "-nic", "none",      // NO guest network
            "-rtc", "base=utc"};
    if (uefi) {
      if (config.ovmf_code.empty()) {
        can_boot = false;
        boot_skip_reason = "ovmf-code-not-configured";
      } else {
        const std::string vars_src =
            IsSecureBootOn(scenario) ? config.ovmf_vars_secboot : config.ovmf_vars_nosecboot;
        if (vars_src.empty()) {
          can_boot = false;
          boot_skip_reason = "ovmf-vars-not-configured";
        } else {
          vars_copy = work / ("OVMF_VARS-" + stamp + ".fd");
          std::filesystem::copy_file(vars_src, vars_copy,
                                     std::filesystem::copy_options::overwrite_existing, ec);
          if (ec) {
            can_boot = false;
            boot_skip_reason = "ovmf-vars-copy-failed";
          } else {
            argv.push_back("-drive");
            argv.push_back("if=pflash,format=raw,readonly=on,file=" + config.ovmf_code);
            argv.push_back("-drive");
            argv.push_back("if=pflash,format=raw,file=" + vars_copy.string());
          }
        }
      }
    }
    // The overlay is the only writable disk. The base is NEVER named here.
    argv.push_back("-drive");
    argv.push_back("file=" + overlay.string() + ",format=qcow2,if=ide");
  } else {
    boot_skip_reason = "qemu-system-not-found";
  }

  if (can_boot) {
    std::string ver;
    bool vto = false;
    RunProc({qemu_system, "--version"}, 10, &ver, &vto);
    report.events.push_back("qemu_system_version=" + FirstLine(ver));
    report.events.push_back("qemu_secure_boot=" +
                            std::string(uefi ? (IsSecureBootOn(scenario) ? "on" : "off") : "n/a-legacy"));

    const int to = BootTimeout(scenario, config);
    std::string boot_err;
    bool boot_to = false;
    const int brc = RunProc(argv, to, &boot_err, &boot_to);
    report.real_boot = true;
    // A timeout is EXPECTED: we measure early boot, we do not complete a Windows boot.
    report.events.push_back("qemu_boot=" + std::string(boot_to ? "timeout-reached" : "exited") +
                            " rc=" + std::to_string(brc) + " timeout_s=" + std::to_string(to));
    if (std::filesystem::exists(serial)) {
      report.delta_hashes["qemu:serial_log_sha256"] = Sha256File(serial.string());
      report.events.push_back("event_logs=serial:" + serial.filename().string());
    } else {
      report.events.push_back("event_logs=none:boot-produced-no-serial");
    }
    report.alerts.push_back("boot-chain:qemu-measured-boot");
  } else {
    report.real_boot = false;
    report.events.push_back("qemu_boot=skipped reason=" + boot_skip_reason);
    report.events.push_back("event_logs=none:boot-skipped");
    report.alerts.push_back("boot-chain:qemu-snapshot-only");
  }

  // --- rollback-after: discard the overlay (+ vars copy); base must be byte-identical ---
  std::filesystem::remove(overlay, ec);
  if (!vars_copy.empty()) std::filesystem::remove(vars_copy, ec);
  const std::string base_hash_post = Sha256File(base.string());
  report.baseline_hashes["qemu:base_image_post"] = base_hash_post;
  report.rollback_confirmed = (base_hash_pre == base_hash_post);
  report.events.push_back("qemu_rollback=overlay-discarded rollback_confirmed=" +
                          std::string(report.rollback_confirmed ? "true" : "false"));
  if (!report.rollback_confirmed) {
    report.alerts.push_back("boot-chain:ROLLBACK-FAILED-base-image-changed");
  }
  return report;
}

}  // namespace bootkitstudio
