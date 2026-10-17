#include "Registry.hpp"

#include "Asset.hpp"
#include "Curve.hpp"
#include "Data.hpp"

namespace UTool::Lua {

void registerModules(sol::state& lua, ScriptRegistrations& regs) {
  registerCurve(lua, regs);
  registerAsset(lua, regs);
  registerData(lua, regs);
}

}  // namespace UTool::Lua
