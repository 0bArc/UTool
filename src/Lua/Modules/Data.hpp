#pragma once

#include "UTool/Lua/Host.hpp"

#include <sol/sol.hpp>

namespace UTool::Lua {

void registerData(sol::state& lua, ScriptRegistrations& regs);

}  // namespace UTool::Lua
