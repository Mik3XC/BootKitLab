#include "bootkitstudio/hash_utils.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace bootkitstudio {
namespace {

std::string ShellQuote(const std::string& value) {
  std::string quoted = "'";
  for (const char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
    } else {
      quoted += c;
    }
  }
  quoted += "'";
  return quoted;
}

std::string ReadFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    throw std::runtime_error("Unable to open file for hashing: " + path);
  }

  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}

}  // namespace

std::string Fnv1a64(const std::string& input) {
  constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
  constexpr uint64_t kFnvPrime = 1099511628211ULL;

  uint64_t hash = kFnvOffset;
  for (const unsigned char c : input) {
    hash ^= static_cast<uint64_t>(c);
    hash *= kFnvPrime;
  }

  std::ostringstream output;
  output << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
  return output.str();
}

std::string Sha256File(const std::string& path) {
  const std::string command = "shasum -a 256 " + ShellQuote(path);
  std::array<char, 256> buffer{};
  std::string result;

  FILE* pipe = popen(command.c_str(), "r");
  if (pipe != nullptr) {
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
      result += buffer.data();
    }
    const int rc = pclose(pipe);
    if (rc == 0) {
      const size_t sep = result.find(' ');
      if (sep != std::string::npos) {
        return "sha256:" + result.substr(0, sep);
      }
    }
  }

  // Fall back to deterministic non-cryptographic hashing if shasum is unavailable.
  return Fnv1a64(ReadFile(path));
}

}  // namespace bootkitstudio
