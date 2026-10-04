#pragma once

#include <string>
#include <vector>

namespace bootkitstudio {

std::string Trim(const std::string& input);
std::vector<std::string> Split(const std::string& input, char delimiter);
std::string ToLower(const std::string& input);
std::string CurrentIsoUtcTimestamp();
std::string JsonEscape(const std::string& input);
std::string Join(const std::vector<std::string>& parts, const std::string& delimiter);

}  // namespace bootkitstudio
