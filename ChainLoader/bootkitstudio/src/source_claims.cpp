#include "bootkitstudio/source_claims.hpp"

#include <fstream>
#include <set>
#include <stdexcept>

#include "bootkitstudio/util.hpp"

namespace bootkitstudio {
namespace {

bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

std::pair<std::string, std::string> ParseKeyValue(const std::string& line) {
  const size_t sep = line.find(':');
  if (sep == std::string::npos) {
    return {"", ""};
  }
  return {Trim(line.substr(0, sep)), Trim(line.substr(sep + 1))};
}

std::string Unquote(const std::string& value) {
  if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                            (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

std::vector<std::string> ParseInlineList(std::string value) {
  value = Trim(value);
  if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
    value = value.substr(1, value.size() - 2);
  }

  std::vector<std::string> values;
  for (const std::string& token : Split(value, ',')) {
    if (!token.empty()) {
      values.push_back(Unquote(token));
    }
  }
  return values;
}

}  // namespace

std::vector<ClaimEntry> ParseSourceClaims(const std::string& path) {
  std::ifstream input(path);
  if (!input.is_open()) {
    throw std::runtime_error("Unable to open source claims file: " + path);
  }

  std::vector<ClaimEntry> claims;
  ClaimEntry current;
  bool in_claims = false;
  bool has_current = false;

  std::string line;
  while (std::getline(input, line)) {
    line = Trim(line);
    if (line.empty() || StartsWith(line, "#")) {
      continue;
    }
    if (line == "claims:") {
      in_claims = true;
      continue;
    }
    if (!in_claims) {
      continue;
    }

    if (StartsWith(line, "- ")) {
      if (has_current) {
        claims.push_back(current);
      }
      current = ClaimEntry{};
      has_current = true;
      const auto kv = ParseKeyValue(Trim(line.substr(2)));
      if (kv.first == "claim_id") {
        current.claim_id = Unquote(kv.second);
      }
      continue;
    }

    const auto kv = ParseKeyValue(line);
    if (!has_current || kv.first.empty()) {
      continue;
    }

    if (kv.first == "claim_id") {
      current.claim_id = Unquote(kv.second);
    } else if (kv.first == "source") {
      current.source = Unquote(kv.second);
    } else if (kv.first == "status") {
      current.status = ToLower(Unquote(kv.second));
    } else if (kv.first == "tags") {
      current.tags = ParseInlineList(kv.second);
    } else if (kv.first == "statement") {
      current.statement = Unquote(kv.second);
    }
  }

  if (has_current) {
    claims.push_back(current);
  }

  return claims;
}

bool ValidateSourceClaimsCoverage(const std::vector<ClaimEntry>& claims, std::string* reason) {
  if (claims.empty()) {
    if (reason != nullptr) {
      *reason = "no claims found";
    }
    return false;
  }

  const std::set<std::string> allowed_status = {
      "validated", "needs-lab-validation", "deprecated/non-viable"};
  std::set<std::string> unique_ids;

  for (const ClaimEntry& claim : claims) {
    if (claim.claim_id.empty() || claim.source.empty() || claim.statement.empty()) {
      if (reason != nullptr) {
        *reason = "claim is missing required fields";
      }
      return false;
    }

    if (allowed_status.find(claim.status) == allowed_status.end()) {
      if (reason != nullptr) {
        *reason = "claim has unsupported status: " + claim.status;
      }
      return false;
    }

    if (!unique_ids.insert(claim.claim_id).second) {
      if (reason != nullptr) {
        *reason = "duplicate claim_id: " + claim.claim_id;
      }
      return false;
    }
  }

  return true;
}

}  // namespace bootkitstudio
