#pragma once

#include <sol/sol.hpp>

#include <memory>
#include <vector>

namespace UTool::Lua::detail {

using KeptFunctions = std::shared_ptr<std::vector<sol::protected_function>>;

inline KeptFunctions makeFunctionKeep(sol::state& lua, const char* storageKey) {
  auto keep = std::make_shared<std::vector<sol::protected_function>>();
  lua[storageKey] = keep;
  return keep;
}

}  // namespace UTool::Lua::detail
