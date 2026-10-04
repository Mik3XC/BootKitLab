#pragma once

#include <string>

namespace bootkitstudio {

std::string Sha256File(const std::string& path);
std::string Fnv1a64(const std::string& input);

}  // namespace bootkitstudio
