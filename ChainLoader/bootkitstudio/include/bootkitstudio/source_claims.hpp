#pragma once

#include <string>
#include <vector>

#include "bootkitstudio/types.hpp"

namespace bootkitstudio {

std::vector<ClaimEntry> ParseSourceClaims(const std::string& path);
bool ValidateSourceClaimsCoverage(const std::vector<ClaimEntry>& claims, std::string* reason);

}  // namespace bootkitstudio
