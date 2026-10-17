#pragma once

#include <nlohmann/json.hpp>
#include <sol/sol.hpp>

namespace UTool::Lua {

[[nodiscard]] nlohmann::json solToJson(const sol::object& obj);
[[nodiscard]] sol::object jsonToSol(sol::state_view lua, const nlohmann::json& value);

}  // namespace UTool::Lua
