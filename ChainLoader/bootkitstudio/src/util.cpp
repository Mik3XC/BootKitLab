#include "bootkitstudio/util.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace bootkitstudio {

std::string Trim(const std::string& input) {
  size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
    ++start;
  }

  size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }

  return input.substr(start, end - start);
}

std::vector<std::string> Split(const std::string& input, char delimiter) {
  std::vector<std::string> parts;
  std::stringstream stream(input);
  std::string token;
  while (std::getline(stream, token, delimiter)) {
    parts.push_back(Trim(token));
  }
  return parts;
}

std::string ToLower(const std::string& input) {
  std::string lowered = input;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return lowered;
}

std::string CurrentIsoUtcTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t now_time = std::chrono::system_clock::to_time_t(now);
  std::tm gmt{};
#if defined(_WIN32)
  gmtime_s(&gmt, &now_time);
#else
  gmtime_r(&now_time, &gmt);
#endif
  std::ostringstream output;
  output << std::put_time(&gmt, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

std::string JsonEscape(const std::string& input) {
  std::ostringstream output;
  for (char c : input) {
    switch (c) {
      case '\\':
        output << "\\\\";
        break;
      case '"':
        output << "\\\"";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        output << c;
        break;
    }
  }
  return output.str();
}

std::string Join(const std::vector<std::string>& parts, const std::string& delimiter) {
  std::ostringstream output;
  for (size_t i = 0; i < parts.size(); ++i) {
    if (i != 0) {
      output << delimiter;
    }
    output << parts[i];
  }
  return output.str();
}

}  // namespace bootkitstudio
