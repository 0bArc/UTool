#pragma once

#include <string>
#include <string_view>

namespace UTool::Core {

[[nodiscard]] bool iequals(std::string_view a, std::string_view b);
[[nodiscard]] std::string toLower(std::string s);
[[nodiscard]] std::string trim(std::string_view s);

}  // namespace UTool::Core
