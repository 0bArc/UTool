#include "UTool/Lua/Host.hpp"

#include "Modules/Registry.hpp"

#include <sol/sol.hpp>

#include <stdexcept>

namespace UTool::Lua {

ScriptRegistrations loadModScripts(const std::vector<std::filesystem::path>& scripts) {
  ScriptRegistrations regs;
  if (scripts.empty())
    return regs;

  regs.lua = std::make_shared<sol::state>();
  sol::state& lua = *regs.lua;
  lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table,
                     sol::lib::package);

  lua.create_named_table("utool");
  registerModules(lua, regs);

  for (const auto& script : scripts) {
    auto result = lua.safe_script_file(script.string());
    if (!result.valid()) {
      sol::error err = result;
      throw std::runtime_error("Failed to load " + script.string() + ": " + err.what());
    }
  }

  return regs;
}

}  // namespace UTool::Lua
