#include "UTool/Core/StringUtil.hpp"

#include <cctype>

namespace UTool::Core {

bool iequals(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i])))
      return false;
  }
  return true;
}

std::string toLower(std::string s) {
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::string trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.remove_suffix(1);
  return std::string(s);
}

}  // namespace UTool::Core
